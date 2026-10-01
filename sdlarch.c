
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include "libretro.h"
#include "glad.h"

static SDL_Window *g_win = NULL;
static SDL_GLContext *g_ctx = NULL;
static SDL_AudioDeviceID g_pcm = 0;
static struct retro_frame_time_callback runloop_frame_time;
static retro_usec_t runloop_frame_time_last = 0;
static double g_fps = 60.0;
static uint64_t g_last_frame = 0;
static const uint8_t *g_kbd = NULL;
static struct retro_audio_callback audio_callback;

static float g_scale = 3;
bool running = true;

static struct {
	GLuint tex_id;
    GLuint fbo_id;
    GLuint rbo_id;

    int glmajor;
    int glminor;


	GLuint pitch;
	GLint tex_w, tex_h;
	GLuint clip_w, clip_h;

	GLuint pixfmt;
	GLuint pixtype;
	GLuint bpp;

    struct retro_hw_render_callback hw;
} g_video  = {0};

static struct {
    GLuint vao;
    GLuint vbo;
    GLuint program;

    GLint i_pos;
    GLint i_coord;
    GLint u_tex;
    GLint u_tint;
    GLint u_mvp;

} g_shader = {0};

/* The matrix init_shaders() gave the game quad. The card pass overwrites the
 * shared u_mvp uniform, so it has to put this back. */
static float g_game_mvp[4][4];

/* Title card (defined with the challenge table, drawn from video_refresh). */
static void card_draw(int win_w, int win_h);
static void card_deinit(void);

static struct retro_variable *g_vars = NULL;

static const char *g_vshader_src =
    "#version 150\n"
    "in vec2 i_pos;\n"
    "in vec2 i_coord;\n"
    "out vec2 o_coord;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
        "o_coord = i_coord;\n"
        "gl_Position = vec4(i_pos, 0.0, 1.0) * u_mvp;\n"
    "}";

static const char *g_fshader_src =
    "#version 150\n"
    "in vec2 o_coord;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec4 u_tint;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
        "o_color = texture(u_tex, o_coord) * u_tint;\n"
    "}";




static struct {
	void *handle;
	bool initialized;
	bool supports_no_game;
	// The last performance counter registered. TODO: Make it a linked list.
	struct retro_perf_counter* perf_counter_last;

	void (*retro_init)(void);
	void (*retro_deinit)(void);
	unsigned (*retro_api_version)(void);
	void (*retro_get_system_info)(struct retro_system_info *info);
	void (*retro_get_system_av_info)(struct retro_system_av_info *info);
	void (*retro_set_controller_port_device)(unsigned port, unsigned device);
	void (*retro_reset)(void);
	void (*retro_run)(void);
	size_t (*retro_serialize_size)(void);
	bool (*retro_serialize)(void *data, size_t size);
	bool (*retro_unserialize)(const void *data, size_t size);
//	void retro_cheat_reset(void);
//	void retro_cheat_set(unsigned index, bool enabled, const char *code);
	bool (*retro_load_game)(const struct retro_game_info *game);
//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*retro_unload_game)(void);
//	unsigned retro_get_region(void);
	void *(*retro_get_memory_data)(unsigned id);
	size_t (*retro_get_memory_size)(unsigned id);
} g_retro;


struct keymap {
	unsigned k;
	unsigned rk;
};

static struct keymap g_binds[] = {
    { SDL_SCANCODE_X, RETRO_DEVICE_ID_JOYPAD_A },
    { SDL_SCANCODE_Z, RETRO_DEVICE_ID_JOYPAD_B },
    { SDL_SCANCODE_A, RETRO_DEVICE_ID_JOYPAD_Y },
    { SDL_SCANCODE_S, RETRO_DEVICE_ID_JOYPAD_X },
    { SDL_SCANCODE_UP, RETRO_DEVICE_ID_JOYPAD_UP },
    { SDL_SCANCODE_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { SDL_SCANCODE_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { SDL_SCANCODE_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
    { SDL_SCANCODE_RETURN, RETRO_DEVICE_ID_JOYPAD_START },
    { SDL_SCANCODE_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_SELECT },
    { SDL_SCANCODE_Q, RETRO_DEVICE_ID_JOYPAD_L },
    { SDL_SCANCODE_W, RETRO_DEVICE_ID_JOYPAD_R },
    { 0, 0 }
};

static unsigned g_joy[RETRO_DEVICE_ID_JOYPAD_R3+1] = { 0 };

static char g_game_path[4096] = "";
static bool g_game_loaded = false;

#define load_sym(V, S) do {\
    if (!((*(void**)&V) = SDL_LoadFunction(g_retro.handle, #S))) \
        die("Failed to load symbol '" #S "'': %s", SDL_GetError()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)


static void die(const char *fmt, ...) {
	char buffer[4096];

	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	fputs(buffer, stderr);
	fputc('\n', stderr);
	fflush(stderr);

	exit(EXIT_FAILURE);
}

static GLuint compile_shader(unsigned type, unsigned count, const char **strings) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, count, strings, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        char buffer[4096];
        glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
        die("Failed to compile %s shader: %s", type == GL_VERTEX_SHADER ? "vertex" : "fragment", buffer);
    }

    return shader;
}

void ortho2d(float m[4][4], float left, float right, float bottom, float top) {
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
    m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (top - bottom);
    m[2][2] = -1.0f;
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
}



static void init_shaders() {
    GLuint vshader = compile_shader(GL_VERTEX_SHADER, 1, &g_vshader_src);
    GLuint fshader = compile_shader(GL_FRAGMENT_SHADER, 1, &g_fshader_src);
    GLuint program = glCreateProgram();

    SDL_assert(program);

    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glLinkProgram(program);

    glDeleteShader(vshader);
    glDeleteShader(fshader);

    glValidateProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if(status == GL_FALSE) {
        char buffer[4096];
        glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
        die("Failed to link shader program: %s", buffer);
    }

    g_shader.program = program;
    g_shader.i_pos   = glGetAttribLocation(program,  "i_pos");
    g_shader.i_coord = glGetAttribLocation(program,  "i_coord");
    g_shader.u_tex   = glGetUniformLocation(program, "u_tex");
    g_shader.u_tint  = glGetUniformLocation(program, "u_tint");
    g_shader.u_mvp   = glGetUniformLocation(program, "u_mvp");

    glGenVertexArrays(1, &g_shader.vao);
    glGenBuffers(1, &g_shader.vbo);

    glUseProgram(g_shader.program);

    glUniform1i(g_shader.u_tex, 0);
    /* The game quad must sample the core's pixels untinted. The card pass is the
     * only thing that changes this, and it puts it back. */
    glUniform4f(g_shader.u_tint, 1.0f, 1.0f, 1.0f, 1.0f);

    float m[4][4];
    if (g_video.hw.bottom_left_origin)
        ortho2d(m, -1, 1, 1, -1);
    else
        ortho2d(m, -1, 1, -1, 1);

    memcpy(g_game_mvp, m, sizeof g_game_mvp);

    glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float*)m);

    glUseProgram(0);
}


static void refresh_vertex_data() {
    SDL_assert(g_video.tex_w);
    SDL_assert(g_video.tex_h);
    SDL_assert(g_video.clip_w);
    SDL_assert(g_video.clip_h);

    float bottom = (float)g_video.clip_h / g_video.tex_h;
    float right  = (float)g_video.clip_w / g_video.tex_w;

    float vertex_data[] = {
        // pos, coord
        -1.0f, -1.0f, 0.0f,  bottom, // left-bottom
        -1.0f,  1.0f, 0.0f,  0.0f,   // left-top
         1.0f, -1.0f, right,  bottom,// right-bottom
         1.0f,  1.0f, right,  0.0f,  // right-top
    };

    glBindVertexArray(g_shader.vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_shader.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STREAM_DRAW);

    glEnableVertexAttribArray(g_shader.i_pos);
    glEnableVertexAttribArray(g_shader.i_coord);
    glVertexAttribPointer(g_shader.i_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);
    glVertexAttribPointer(g_shader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void init_framebuffer(int width, int height)
{
    /* Re-entrant: drop previous FBO/RBO before regenerating (ROM swaps). */
    if (g_video.fbo_id)
        glDeleteFramebuffers(1, &g_video.fbo_id);
    if (g_video.rbo_id)
        glDeleteRenderbuffers(1, &g_video.rbo_id);
    g_video.fbo_id = 0;
    g_video.rbo_id = 0;

    glGenFramebuffers(1, &g_video.fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, g_video.fbo_id);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_video.tex_id, 0);

    if (g_video.hw.depth && g_video.hw.stencil) {
        glGenRenderbuffers(1, &g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
    } else if (g_video.hw.depth) {
        glGenRenderbuffers(1, &g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
    }

    if (g_video.hw.depth || g_video.hw.stencil)
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    SDL_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


static void resize_cb(int w, int h) {
	glViewport(0, 0, w, h);
}


static void create_window(int width, int height) {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    g_win = SDL_CreateWindow("sdlarch", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);

	if (!g_win)
        die("Failed to create window: %s", SDL_GetError());

    /* Build an ordered list of candidate GL contexts. macOS only provides
     * OpenGL up to 4.1 core (the API is deprecated there), so the requested
     * version may need to fall back. We also avoid the debug flag, which
     * macOS rejects. */
    int major_candidates[8], minor_candidates[8], profile_candidates[8];
    int n = 0;

    switch (g_video.hw.context_type) {
    case RETRO_HW_CONTEXT_OPENGLES2:
    case RETRO_HW_CONTEXT_OPENGLES3:
    case RETRO_HW_CONTEXT_OPENGLES_VERSION:
        major_candidates[n] = g_video.hw.version_major ? g_video.hw.version_major : 2;
        minor_candidates[n] = g_video.hw.version_minor;
        profile_candidates[n] = SDL_GL_CONTEXT_PROFILE_ES;
        n++;
        break;
    case RETRO_HW_CONTEXT_OPENGL_CORE:
    default: {
        /* Desktop GL: try the requested version first, then walk down to
         * versions that macOS can actually deliver. */
        const int req_major[] = {
            g_video.hw.version_major, 4, 4, 3, 3, 2,
        };
        const int req_minor[] = {
            g_video.hw.version_minor, 1, 0, 3, 2, 1,
        };
        /* Core profile for >= 3.2 (we need it for our #version 150 shaders);
         * fall back to legacy at the very end. */
        const int is_core[] = { 1, 1, 1, 1, 1, 0 };

        for (int i = 0; i < 6; ++i) {
            int profile = is_core[i] ? SDL_GL_CONTEXT_PROFILE_CORE
                                     : SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
            int dup = 0;
            for (int j = 0; j < n; ++j)
                if (major_candidates[j] == req_major[i] &&
                    minor_candidates[j] == req_minor[i] &&
                    profile_candidates[j] == profile)
                    dup = 1;
            if (dup)
                continue;
            major_candidates[n] = req_major[i];
            minor_candidates[n] = req_minor[i];
            profile_candidates[n] = profile;
            n++;
        }
        break;
    }
    }

    g_ctx = NULL;
    for (int i = 0; i < n && !g_ctx; ++i) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major_candidates[i]);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_candidates[i]);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile_candidates[i]);
        g_ctx = SDL_GL_CreateContext(g_win);
        if (g_ctx) {
            g_video.hw.version_major = major_candidates[i];
            g_video.hw.version_minor = minor_candidates[i];
        }
    }

    if (!g_ctx)
        die("Failed to create OpenGL context: %s", SDL_GetError());

    SDL_GL_MakeCurrent(g_win, g_ctx);

    if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGLES2) {
        if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
            die("Failed to initialize glad.");
    } else {
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
            die("Failed to initialize glad.");
    }

    fprintf(stderr, "GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    fprintf(stderr, "GL_VERSION: %s\n", glGetString(GL_VERSION));


    init_shaders();

    SDL_GL_SetSwapInterval(1);
    SDL_GL_SwapWindow(g_win); // make apitrace output nicer

    resize_cb(width, height);
}


static void resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
	*dw = sw;
	*dh = sh;

	if (ratio <= 0)
		ratio = (double)sw / sh;

	if ((float)sw / sh < 1)
		*dw = *dh * ratio;
	else
		*dh = *dw / ratio;
}


static void video_configure(const struct retro_game_geometry *geom) {
	int nwidth, nheight;

	resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);

	nwidth *= g_scale;
	nheight *= g_scale;

	if (!g_win)
		create_window(nwidth, nheight);

	if (g_video.tex_id)
		glDeleteTextures(1, &g_video.tex_id);

	g_video.tex_id = 0;

	if (!g_video.pixfmt)
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;

    SDL_SetWindowSize(g_win, nwidth, nheight);

	glGenTextures(1, &g_video.tex_id);

	if (!g_video.tex_id)
		die("Failed to create the video texture");

	g_video.pitch = geom->max_width * g_video.bpp;

	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

//	glPixelStorei(GL_UNPACK_ALIGNMENT, s_video.pixfmt == GL_UNSIGNED_INT_8_8_8_8_REV ? 4 : 2);
//	glPixelStorei(GL_UNPACK_ROW_LENGTH, s_video.pitch / s_video.bpp);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
			g_video.pixtype, g_video.pixfmt, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

    init_framebuffer(geom->max_width, geom->max_height);

	g_video.tex_w = geom->max_width;
	g_video.tex_h = geom->max_height;
	g_video.clip_w = geom->base_width;
	g_video.clip_h = geom->base_height;

	refresh_vertex_data();

    g_video.hw.context_reset();
}


static bool video_set_pixel_format(unsigned format) {
	switch (format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		g_video.pixfmt = GL_UNSIGNED_INT_8_8_8_8_REV;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint32_t);
		break;
	case RETRO_PIXEL_FORMAT_RGB565:
		g_video.pixfmt  = GL_UNSIGNED_SHORT_5_6_5;
		g_video.pixtype = GL_RGB;
		g_video.bpp = sizeof(uint16_t);
		break;
	default:
		die("Unknown pixel type %u", format);
	}

	return true;
}


static void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    if (g_video.clip_w != width || g_video.clip_h != height)
    {
		g_video.clip_h = height;
		g_video.clip_w = width;

		refresh_vertex_data();
	}

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

	if (pitch != g_video.pitch)
		g_video.pitch = pitch;

    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
						g_video.pixtype, g_video.pixfmt, data);
	}

    int w = 0, h = 0;
    SDL_GetWindowSize(g_win, &w, &h);
    glViewport(0, 0, w, h);

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_shader.program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_video.tex_id);


    glBindVertexArray(g_shader.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    /* Overlay, not a cutscene: the game keeps running and the quad's geometry
     * is untouched, so the frame never shifts. */
    card_draw(w, h);

    SDL_GL_SwapWindow(g_win);
}

static void video_deinit() {
    if (g_video.fbo_id)
        glDeleteFramebuffers(1, &g_video.fbo_id);

	if (g_video.tex_id)
		glDeleteTextures(1, &g_video.tex_id);

    if (g_shader.vao)
        glDeleteVertexArrays(1, &g_shader.vao);

    if (g_shader.vbo)
        glDeleteBuffers(1, &g_shader.vbo);

    if (g_shader.program)
        glDeleteProgram(g_shader.program);

    card_deinit();

    g_video.fbo_id = 0;
	g_video.tex_id = 0;
    g_shader.vao = 0;
    g_shader.vbo = 0;
    g_shader.program = 0;

    SDL_GL_MakeCurrent(g_win, g_ctx);
    SDL_GL_DeleteContext(g_ctx);

    g_ctx = NULL;

    SDL_DestroyWindow(g_win);
}


static int g_pcm_rate = 0;

static void audio_init(int frequency) {
    /* Re-entrant: keep the device across ROM and core swaps, reopening only when
     * the rate changes. */
    if (g_pcm && g_pcm_rate == frequency) {
        if (audio_callback.set_state)
            audio_callback.set_state(true);
        return;
    }
    if (g_pcm) {
        SDL_CloseAudioDevice(g_pcm);
        g_pcm = 0;
    }

    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    SDL_zero(desired);
    SDL_zero(obtained);

    desired.format = AUDIO_S16;
    desired.freq   = frequency;
    desired.channels = 2;
    desired.samples = 4096;

    g_pcm = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!g_pcm) {
        /* Headless host with no sound card: run silent rather than refusing to run. */
        fprintf(stderr, "[engine] audio disabled: %s\n", SDL_GetError());
        if (audio_callback.set_state)
            audio_callback.set_state(true);
        return;
    }
    g_pcm_rate = frequency;

    SDL_PauseAudioDevice(g_pcm, 0);

    // Let the core know that the audio device has been initialized.
    if (audio_callback.set_state) {
        audio_callback.set_state(true);
    }
}


static void audio_deinit() {
    if (g_pcm)
        SDL_CloseAudioDevice(g_pcm);
}

static size_t audio_write(const int16_t *buf, unsigned frames) {
    if (g_pcm)
        SDL_QueueAudio(g_pcm, buf, sizeof(*buf) * frames * 2);
    return frames;
}


static void core_log(enum retro_log_level level, const char *fmt, ...) {
	char buffer[4096] = {0};
	static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (level == 0)
		return;

	fprintf(stderr, "[%s] %s", levelstr[level], buffer);
	fflush(stderr);

	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
}

static uintptr_t core_get_current_framebuffer() {
    return g_video.fbo_id;
}

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
retro_time_t cpu_features_get_time_usec(void) {
    return (retro_time_t)SDL_GetTicks() * 1000;
}

/**
 * Get the CPU Features.
 *
 * @see retro_get_cpu_features_t
 * @return uint64_t Returns a bit-mask of detected CPU features (RETRO_SIMD_*).
 */
static uint64_t core_get_cpu_features() {
    uint64_t cpu = 0;
    if (SDL_HasAVX()) {
        cpu |= RETRO_SIMD_AVX;
    }
    if (SDL_HasAVX2()) {
        cpu |= RETRO_SIMD_AVX2;
    }
    if (SDL_HasMMX()) {
        cpu |= RETRO_SIMD_MMX;
    }
    if (SDL_HasSSE()) {
        cpu |= RETRO_SIMD_SSE;
    }
    if (SDL_HasSSE2()) {
        cpu |= RETRO_SIMD_SSE2;
    }
    if (SDL_HasSSE3()) {
        cpu |= RETRO_SIMD_SSE3;
    }
    if (SDL_HasSSE41()) {
        cpu |= RETRO_SIMD_SSE4;
    }
    if (SDL_HasSSE42()) {
        cpu |= RETRO_SIMD_SSE42;
    }
    return cpu;
}

/**
 * A simple counter. Usually nanoseconds, but can also be CPU cycles.
 *
 * @see retro_perf_get_counter_t
 * @return retro_perf_tick_t The current value of the high resolution counter.
 */
static retro_perf_tick_t core_get_perf_counter() {
    return (retro_perf_tick_t)SDL_GetPerformanceCounter();
}

/**
 * Register a performance counter.
 *
 * @see retro_perf_register_t
 */
static void core_perf_register(struct retro_perf_counter* counter) {
    g_retro.perf_counter_last = counter;
    counter->registered = true;
}

/**
 * Starts a registered counter.
 *
 * @see retro_perf_start_t
 */
static void core_perf_start(struct retro_perf_counter* counter) {
    if (counter->registered) {
        counter->start = core_get_perf_counter();
    }
}

/**
 * Stops a registered counter.
 *
 * @see retro_perf_stop_t
 */
static void core_perf_stop(struct retro_perf_counter* counter) {
    counter->total = core_get_perf_counter() - counter->start;
}

/**
 * Log and display the state of performance counters.
 *
 * @see retro_perf_log_t
 */
static void core_perf_log() {
    // TODO: Use a linked list of counters, and loop through them all.
    core_log(RETRO_LOG_INFO, "[timer] %s: %i - %i", g_retro.perf_counter_last->ident, g_retro.perf_counter_last->start, g_retro.perf_counter_last->total);
}

static bool core_environment(unsigned cmd, void *data) {
	switch (cmd) {
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable *vars = (const struct retro_variable *)data;
        size_t num_vars = 0;

        for (const struct retro_variable *v = vars; v->key; ++v) {
            num_vars++;
        }

        g_vars = (struct retro_variable*)calloc(num_vars + 1, sizeof(*g_vars));
        for (unsigned i = 0; i < num_vars; ++i) {
            const struct retro_variable *invar = &vars[i];
            struct retro_variable *outvar = &g_vars[i];

            const char *semicolon = strchr(invar->value, ';');
            const char *first_pipe = strchr(invar->value, '|');

            SDL_assert(semicolon && *semicolon);
            semicolon++;
            while (isspace(*semicolon))
                semicolon++;

            if (first_pipe) {
                outvar->value = malloc((first_pipe - semicolon) + 1);
                memcpy((char*)outvar->value, semicolon, first_pipe - semicolon);
                ((char*)outvar->value)[first_pipe - semicolon] = '\0';
            } else {
                outvar->value = strdup(semicolon);
            }

            outvar->key = strdup(invar->key);
            SDL_assert(outvar->key && outvar->value);
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;

        if (!g_vars)
            return false;

        for (const struct retro_variable *v = g_vars; v->key; ++v) {
            if (strcmp(var->key, v->key) == 0) {
                var->value = v->value;
                break;
            }
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        bool *bval = (bool*)data;
		*bval = false;
        return true;
    }
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
		struct retro_log_callback *cb = (struct retro_log_callback *)data;
		cb->log = core_log;
        return true;
	}
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
        struct retro_perf_callback *perf = (struct retro_perf_callback *)data;
        perf->get_time_usec = cpu_features_get_time_usec;
        perf->get_cpu_features = core_get_cpu_features;
        perf->get_perf_counter = core_get_perf_counter;
        perf->perf_register = core_perf_register;
        perf->perf_start = core_perf_start;
        perf->perf_stop = core_perf_stop;
        perf->perf_log = core_perf_log;
        return true;
    }
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
		bool *bval = (bool*)data;
		*bval = true;
        return true;
    }
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;

		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;

		return video_set_pixel_format(*fmt);
	}
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
        hw->get_current_framebuffer = core_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)SDL_GL_GetProcAddress;
        g_video.hw = *hw;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
        const struct retro_frame_time_callback *frame_time =
            (const struct retro_frame_time_callback*)data;
        runloop_frame_time = *frame_time;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
        struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
        audio_callback = *audio_cb;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        const char **dir = (const char**)data;
        *dir = ".";
        return true;
    }
    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
        const struct retro_game_geometry *geom = (const struct retro_game_geometry *)data;
        g_video.clip_w = geom->base_width;
        g_video.clip_h = geom->base_height;

        // some cores call this before we even have a window
        if (g_win) {
            refresh_vertex_data();

            int ow = 0, oh = 0;
            resize_to_aspect(geom->aspect_ratio, geom->base_width, geom->base_height, &ow, &oh);

            ow *= g_scale;
            oh *= g_scale;

            SDL_SetWindowSize(g_win, ow, oh);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: {
        g_retro.supports_no_game = *(bool*)data;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
        int *value = (int*)data;
        *value = 1 << 0 | 1 << 1;
        return true;
    }
	default:
		core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
		return false;
	}

    return false;
}


static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    video_refresh(data, width, height, pitch);
}


static void core_input_poll(void) {
	int i;
    g_kbd = SDL_GetKeyboardState(NULL);

	for (i = 0; g_binds[i].k || g_binds[i].rk; ++i)
        g_joy[g_binds[i].rk] = g_kbd[g_binds[i].k];

    if (g_kbd[SDL_SCANCODE_ESCAPE])
        running = false;
}


static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port || index || device != RETRO_DEVICE_JOYPAD)
		return 0;

	return g_joy[id];
}


static void core_audio_sample(int16_t left, int16_t right) {
	int16_t buf[2] = {left, right};
	audio_write(buf, 1);
}


static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
	return audio_write(data, frames);
}


static void core_load(const char *sofile) {
	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
	memset(&g_retro, 0, sizeof(g_retro));
    g_retro.handle = SDL_LoadObject(sofile);

	if (!g_retro.handle)
        die("Failed to load core: %s", SDL_GetError());

	load_retro_sym(retro_init);
	load_retro_sym(retro_deinit);
	load_retro_sym(retro_api_version);
	load_retro_sym(retro_get_system_info);
	load_retro_sym(retro_get_system_av_info);
	load_retro_sym(retro_set_controller_port_device);
	load_retro_sym(retro_reset);
	load_retro_sym(retro_run);
	load_retro_sym(retro_serialize_size);
	load_retro_sym(retro_serialize);
	load_retro_sym(retro_unserialize);
	load_retro_sym(retro_load_game);
	load_retro_sym(retro_unload_game);
	load_retro_sym(retro_get_memory_data);
	load_retro_sym(retro_get_memory_size);

	load_sym(set_environment, retro_set_environment);
	load_sym(set_video_refresh, retro_set_video_refresh);
	load_sym(set_input_poll, retro_set_input_poll);
	load_sym(set_input_state, retro_set_input_state);
	load_sym(set_audio_sample, retro_set_audio_sample);
	load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

	set_environment(core_environment);
	set_video_refresh(core_video_refresh);
	set_input_poll(core_input_poll);
	set_input_state(core_input_state);
	set_audio_sample(core_audio_sample);
	set_audio_sample_batch(core_audio_sample_batch);

	g_retro.retro_init();
	g_retro.initialized = true;

	puts("Core loaded");
}


/* Load a game into the loaded core. Returns false (without killing the
 * process) if the file cannot be read or the core refuses it, so the
 * challenge engine can drop the offending challenge and try the next one. */
static bool core_load_game(const char *filename) {
	struct retro_system_av_info av = {0};
	struct retro_system_info system = {0};
	struct retro_game_info info = { filename, 0 };

    info.path = filename;
    info.meta = "";
    info.data = NULL;
    info.size = 0;

    if (filename) {
        g_retro.retro_get_system_info(&system);

        if (!system.need_fullpath) {
            SDL_RWops *file = SDL_RWFromFile(filename, "rb");
            Sint64 size;

            if (!file) {
                fprintf(stderr, "[core] cannot open %s: %s\n", filename, SDL_GetError());
                return false;
            }

            size = SDL_RWsize(file);

            if (size < 0) {
                fprintf(stderr, "[core] cannot stat %s: %s\n", filename, SDL_GetError());
                SDL_RWclose(file);
                return false;
            }

            info.size = size;
            info.data = SDL_malloc(info.size);

            if (!info.data) {
                fprintf(stderr, "[core] OOM for %s\n", filename);
                SDL_RWclose(file);
                return false;
            }

            if (!SDL_RWread(file, (void*)info.data, info.size, 1)) {
                fprintf(stderr, "[core] failed to read %s: %s\n", filename, SDL_GetError());
                SDL_RWclose(file);
                SDL_free((void*)info.data);
                return false;
            }

            SDL_RWclose(file);
        }
    }

	if (filename)
		snprintf(g_game_path, sizeof(g_game_path), "%s", filename);

	if (!g_retro.retro_load_game(&info)) {
		fprintf(stderr, "[core] the core failed to load %s\n", filename ? filename : "(null)");
        if (info.data)
            SDL_free((void*)info.data);
		return false;
	}
	g_game_loaded = true;

	g_retro.retro_get_system_av_info(&av);
	g_fps = av.timing.fps;

	video_configure(&av.geometry);
	audio_init(av.timing.sample_rate);

    if (info.data)
        SDL_free((void*)info.data);

    // Now that we have the system info, set the window title.
    char window_title[255];
    snprintf(window_title, sizeof(window_title), "sdlarch %s %s", system.library_name, system.library_version);
    SDL_SetWindowTitle(g_win, window_title);
    return true;
}

static void core_unload() {
	g_game_loaded = false;
	if (g_retro.initialized)
		g_retro.retro_deinit();

	if (g_retro.handle)
        SDL_UnloadObject(g_retro.handle);
}

/* Serialize the core's current state to a native savestate on disk.
 * Filename derives from the loaded game (e.g. "Super Mario World (USA).state"). */
static void save_state_to_disk(void) {
	size_t size = g_retro.retro_serialize_size ? g_retro.retro_serialize_size() : 0;
	if (!size) { fprintf(stderr, "[savestate] core reports 0-size state\n"); return; }

	void *buf = malloc(size);
	if (!buf) { fprintf(stderr, "[savestate] buffer alloc failed\n"); return; }

	if (!g_retro.retro_serialize(buf, size)) {
		free(buf);
		fprintf(stderr, "[savestate] retro_serialize failed\n");
		return;
	}

	const char *base = g_game_path[0] ? g_game_path : "game";
	const char *name = strrchr(base, '/');
	name = name ? name + 1 : base;
	char out[1024];
	snprintf(out, sizeof(out), "%s", name);
	char *dot = strrchr(out, '.');
	if (dot) *dot = '\0';
	strncat(out, ".state", sizeof(out) - strlen(out) - 1);

	FILE *f = fopen(out, "wb");
	if (!f) { free(buf); fprintf(stderr, "[savestate] cannot open %s\n", out); return; }
	size_t wrote = fwrite(buf, 1, size, f);
	fclose(f);
	free(buf);
	fprintf(stderr, "[savestate] wrote %zu/%zu bytes to %s\n", wrote, size, out);
}

/* Load a native savestate from disk via retro_unserialize. */
static void core_load_state(const char *filename) {
	if (!filename || !g_retro.retro_unserialize)
		return;
	SDL_RWops *file = SDL_RWFromFile(filename, "rb");
	if (!file) {
		/* Optional state: don't hard-fail if the file is absent. */
		fprintf(stderr, "[state] skip %s: %s\n", filename, SDL_GetError());
		return;
	}
	Sint64 size = SDL_RWsize(file);
	if (size < 0)
		die("Failed to query state file size: %s", SDL_GetError());
	void *data = SDL_malloc(size);
	if (!SDL_RWread(file, data, size, 1))
		die("Failed to read state file: %s", SDL_GetError());
	SDL_RWclose(file);
	if (!g_retro.retro_unserialize(data, size))
		die("Failed to load state %s", filename);
	SDL_free(data);
	puts("State loaded");
}

/* ============================================================================
 * Generalized challenge engine
 *
 * Every BizHawk challenge handler in ../challenges/ is a DNF (disjunction of
 * AND-groups) of RAM comparisons that fires one of three actions:
 *
 *   ACT_SWITCH    schedule a switch to the next challenge after wait_s
 *                 (0 = next frame). Stops evaluating further rules for this
 *                 frame (mirrors the Lua `return <seconds>`). While a switch
 *                 is already pending, further matches are ignored (mirrors
 *                 `if not switch_timer.active`).
 *   ACT_RESET     reload the current challenge's savestate after wait_s
 *                 (0 = next frame). Evaluation continues, and repeated calls
 *                 refresh the timer (mirrors Lua `reset()`, which does not
 *                 return and keeps resetting frames_left).
 *   ACT_SET_LATCH set a one-shot flag consumed by FLAG_NEED_LATCH rules.
 *                 Any SWITCH that matches clears the latch (mirrors the
 *                 `state.boss_spawned = nil` lines in Streets of Rage 2).
 *
 * A sample reads one value from the core's system RAM ($7E:0000 WRAM on
 * snes9x) with optional endianness, signedness, bit mask/shift, an additive
 * offset, and a "prev" flag selecting the previous frame's value (kept in a
 * shadow cache, mirroring the `state.prev_*` fields). A term compares a
 * sample against a constant or another sample. A rule is an AND of terms;
 * when all terms hold - optionally for N+1 consecutive frames
 * (`stable_frames`, the Mega Man camera settle) - the rule's action fires.
 * Zero-initialized terms carry op OP_NONE (0) and terminate a rule, and a
 * rule with act ACT_NONE (0) terminates a rule list.
 *
 * Nothing in this section is game specific: the challenges[] table below is
 * the data-driven port of the 24 Lua handler files in challenges/.
 * ========================================================================== */

struct sample {
    uint16_t addr;   /* offset from system-RAM base */
    uint8_t  size;   /* 1 or 2 bytes */
    uint8_t  be;     /* big-endian (0 = little) */
    uint8_t  sgn;    /* sign-extend */
    uint8_t  prev;   /* read the previous frame's value */
    uint16_t mask;   /* applied before shift */
    uint8_t  shift;  /* right shift after mask */
    int32_t  offset; /* added after shift */
};

enum { OP_NONE = 0, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE };

struct term {
    struct sample a;
    uint8_t  b_is_const;
    struct sample b;
    int32_t  c;
    uint8_t  op;
};

enum { ACT_NONE = 0, ACT_SWITCH, ACT_RESET, ACT_SET_LATCH };
enum { FLAG_NEED_LATCH = 1 };

struct rule {
    uint8_t  act;
    uint8_t  flags;
    uint16_t stable_frames;
    double   wait_s;
    struct term terms[12];
};

struct rw { uint16_t addr; uint8_t value; };

/* A cartridge's identity, authored once per ROM and shared by every challenge
 * that runs it (the title card's top plate). */
struct game_meta {
    const char *title;     /* display title, uppercase, at most 24 columns */
    const char *year;
    const char *publisher;
    const char *platform;  /* abbreviated: NES, SNES, GB, GBA, GEN, NEOGEO */
};

struct challenge {
    const char *slug;        /* game group (labeling) */
    const char *name;        /* human label */
    const char *rom;         /* game ROM on disk, relative to the cwd */
    const char *state;       /* savestate on disk, relative to the cwd */
    double      weight;      /* base selection weight */
    int         interlude;   /* forced every INTERLUDE_INTERVAL_S seconds */
    int         n_writes;
    const struct rw  *writes; /* per-frame RAM writes (interlude force-spawn) */
    const struct rule *rules; /* ACT_NONE-terminated */
    const struct game_meta *game; /* cartridge identity (title card) */
    const char *text;             /* upstream challenge_text; "" = no card */
};

/* Sample constructors (field order: addr, size, be, sgn, prev, mask, shift,
 * offset). P = prev-frame shadow, S = signed, M = mask/shift, BE = big-endian. */
#define R8(a)         { (a), 1, 0, 0, 0, 0x00FF, 0, 0 }
#define R8P(a)        { (a), 1, 0, 0, 1, 0x00FF, 0, 0 }
#define R8S(a)        { (a), 1, 0, 1, 0, 0x00FF, 0, 0 }
#define R8PO(a, o)    { (a), 1, 0, 0, 1, 0x00FF, 0, (o) }
#define R8M(a, m, s)  { (a), 1, 0, 0, 0, (m), (s), 0 }
#define R8MP(a, m, s) { (a), 1, 0, 0, 1, (m), (s), 0 }
#define R16(a)        { (a), 2, 0, 0, 0, 0xFFFF, 0, 0 }
#define R16BE(a)      { (a), 2, 1, 0, 0, 0xFFFF, 0, 0 }
#define R16BES(a)     { (a), 2, 1, 1, 0, 0xFFFF, 0, 0 }
#define R16BEP(a)     { (a), 2, 1, 0, 1, 0xFFFF, 0, 0 }
#define R16BEPS(a)    { (a), 2, 1, 1, 1, 0xFFFF, 0, 0 }

/* term constructors: a OP const / a OP sample-b.
 * Parameter names must NOT collide with the field designators (.a, .op, .b)
 * or the designators would be substituted too. The sample args expand to
 * brace-enclosed initializer lists, which cannot be parenthesized. */
#define A(sa, s_op, sv) { .a = sa, .b_is_const = 1, .c = (int32_t)(sv), .op = (s_op) }
#define B(sa, s_op, sb) { .a = sa, .b_is_const = 0, .b = sb, .op = (s_op) }

/* Rule constructors; the terms list terminates at the first zero op. */
#define SW(wait, ...) { .act = ACT_SWITCH, .wait_s = (double)(wait), .terms = { __VA_ARGS__ } }
#define RS(wait, ...) { .act = ACT_RESET,  .wait_s = (double)(wait), .terms = { __VA_ARGS__ } }
#define LATCH(...)    { .act = ACT_SET_LATCH, .terms = { __VA_ARGS__ } }

/* --- Rule sets ------------------------------------------------------------ */

/* ALinkToThePast.lua: exit-dungeon reset, then death / mini-boss music. */
static const struct rule alttp_rules[] = {
    RS(0.2,  A(R8(0x005E), OP_EQ, 2)),
    SW(1.1,  A(R16(0x0132), OP_EQ, 61712)),
    SW(1.6,  A(R16(0x0132), OP_EQ, 6416)),
    { .act = ACT_NONE },
};

/* Castlevania.lua: edge on the trigger byte, edge on the death byte. */
static const struct rule castlevania_rules[] = {
    SW(1.0,  A(R8P(0x0018), OP_NE, 8), A(R8(0x0018), OP_EQ, 8)),
    SW(1.6,  A(R8P(0x0045), OP_NE, 0), A(R8(0x0045), OP_EQ, 0)),
    { .act = ACT_NONE },
};

/* DonkeyKongCountry.lua: all three states share the game-state edge and the
 * lives check; the barrel adds the x-position check, the boss the HP check. */
static const struct rule dkc_rules_barrel[] = {
    SW(0.5,    A(R16(0x00BE), OP_GT, 4800)),
    SW(0.016,  A(R8P(0x0040), OP_NE, 12), A(R8(0x0040), OP_EQ, 12)),
    SW(0.8,    B(R8P(0x0575), OP_GT, R8(0x0575))),
    { .act = ACT_NONE },
};
static const struct rule dkc_rules_level1[] = {
    SW(0.016,  A(R8P(0x0040), OP_NE, 12), A(R8(0x0040), OP_EQ, 12)),
    SW(0.8,    B(R8P(0x0575), OP_GT, R8(0x0575))),
    { .act = ACT_NONE },
};
static const struct rule dkc_rules_boss1[] = {
    SW(2.6,    A(R8(0x1503), OP_EQ, 0)),
    SW(0.016,  A(R8P(0x0040), OP_NE, 12), A(R8(0x0040), OP_EQ, 12)),
    SW(0.8,    B(R8P(0x0575), OP_GT, R8(0x0575))),
    { .act = ACT_NONE },
};

/* Earthbound.lua: OR of two immediate-switch conditions, then flee. */
static const struct rule earthbound_rules[] = {
    SW(0,    A(R8(0x9A15), OP_EQ, 0)),
    SW(0,    A(R8(0x1085), OP_EQ, 148)),
    SW(3.8,  A(R8(0xA22D), OP_EQ, 0)),
    { .act = ACT_NONE },
};

/* Gradius.lua: win, then game-over (immediate). */
static const struct rule gradius_rules[] = {
    SW(1.6,  A(R8(0x0100), OP_EQ, 2)),
    SW(0,    A(R8(0x001C), OP_EQ, 147)),
    { .act = ACT_NONE },
};

/* Kirby.lua: score must beat previous score + 100; check 255 = done. */
static const struct rule kirby_rules_miniboss[] = {
    SW(0.8,    B(R8(0x0593), OP_GE, R8PO(0x0593, 100))),
    SW(0.8,    A(R8(0x0597), OP_EQ, 255)),
    { .act = ACT_NONE },
};
static const struct rule kirby_rules_level1[] = {
    SW(0.016,  A(R8P(0x058E), OP_NE, 32), A(R8(0x058E), OP_EQ, 32)),
    SW(0.8,    A(R8(0x0597), OP_EQ, 255)),
    { .act = ACT_NONE },
};

/* LinksAwakening.lua: player HP or enemy HP hits zero. */
static const struct rule linksawakening_rules[] = {
    SW(2.87,  A(R8(0x0364), OP_EQ, 0)),
    SW(1.6,   A(R8(0x1B5A), OP_EQ, 0)),
    { .act = ACT_NONE },
};

/* Mario1.lua: three fail conditions (OR), then the level-clear check. */
static const struct rule mario1_rules_castle[] = {
    SW(2.78,   A(R8(0x000E), OP_EQ, 11)),
    SW(2.78,   A(R8(0x0712), OP_EQ, 1)),
    SW(2.78,   A(R8(0x07F8), OP_EQ, 0), A(R8(0x07F9), OP_EQ, 0), A(R8(0x07FA), OP_EQ, 0)),
    SW(0.016,  A(R8(0x0016), OP_EQ, 0), A(R8(0x0017), OP_EQ, 0),
               A(R8(0x0018), OP_EQ, 0), A(R8(0x0019), OP_EQ, 0), A(R8(0x001A), OP_EQ, 0)),
    { .act = ACT_NONE },
};
static const struct rule mario1_rules_1_1[] = {
    SW(2.78,   A(R8(0x000E), OP_EQ, 11)),
    SW(2.78,   A(R8(0x0712), OP_EQ, 1)),
    SW(2.78,   A(R8(0x07F8), OP_EQ, 0), A(R8(0x07F9), OP_EQ, 0), A(R8(0x07FA), OP_EQ, 0)),
    SW(0.8,    A(R8(0x000E), OP_EQ, 4)),
    { .act = ACT_NONE },
};

/* Mario3.lua: all three states share the death-fanfare edge. */
static const struct rule mario3_rules_miniboss[] = {
    SW(0.75,   A(R8P(0x05F3), OP_NE, 1), A(R8(0x05F3), OP_EQ, 1)),
    SW(0.75,   A(R8P(0x04F4), OP_NE, 1), A(R8(0x04F4), OP_EQ, 1)),
    { .act = ACT_NONE },
};
static const struct rule mario3_rules_ceiling[] = {
    SW(0.016,  A(R8P(0x0075), OP_NE, 7), A(R8(0x0075), OP_EQ, 7)),
    SW(0.8,    A(R8P(0x04F4), OP_NE, 1), A(R8(0x04F4), OP_EQ, 1)),
    { .act = ACT_NONE },
};
static const struct rule mario3_rules_hammer[] = {
    SW(1.6,    A(R8(0x05F3), OP_EQ, 2)),
    SW(0.016,  A(R8P(0x0075), OP_NE, 7), A(R8(0x0075), OP_EQ, 7)),
    SW(0.8,    A(R8P(0x04F4), OP_NE, 1), A(R8(0x04F4), OP_EQ, 1)),
    { .act = ACT_NONE },
};

/* MarioWorld.lua: the three states formerly hard-wired as the phase machine.
 * level1/castle key on the magic flag; the boss additionally on the boss id. */
static const struct rule marioworld_rules_level1[] = {
    SW(1.2,    A(R16(0x0DDA), OP_EQ, 255)),
    { .act = ACT_NONE },
};
static const struct rule marioworld_rules_castle[] = {
    SW(1.2,    A(R16(0x0DDA), OP_EQ, 255)),
    SW(0.016,  A(R16(0x0DDA), OP_EQ, 5)),
    { .act = ACT_NONE },
};
static const struct rule marioworld_rules_boss[] = {
    SW(1.2,    A(R16(0x0DDA), OP_EQ, 255)),
    SW(1.2,    A(R16(0x0A54), OP_EQ, 1)),
    { .act = ACT_NONE },
};

/* MarioWorldInterlude.lua: force the spawn byte every frame, then win on the
 * interlude flag or the magic door. */
static const struct rw interlude_writes[] = {
    { 0x0F30, 128 },
};
static const struct rule marioworld_interlude_rules[] = {
    SW(0.8,    A(R16(0x13D2), OP_EQ, 1)),
    SW(1.2,    A(R16(0x0DDA), OP_EQ, 255)),
    { .act = ACT_NONE },
};

/* Megaman.lua: one rule set shared by all three states. The camera must
 * settle (fire on the 4th consecutive matching frame), then the hp edge and
 * the ten all-zero enemy checks. */
static const struct rule megaman_rules[] = {
    { .act = ACT_SWITCH, .wait_s = 0.016, .stable_frames = 3,
      .terms = { A(R8(0x001C), OP_EQ, 2) } },
    SW(0.8,    A(R8P(0x006A), OP_GT, 0), A(R8(0x006A), OP_EQ, 0)),
    SW(0.8,    A(R8(0x00E0), OP_EQ, 0), A(R8(0x01FA), OP_EQ, 0),
               A(R8(0x0500), OP_EQ, 0), A(R8(0x0501), OP_EQ, 0),
               A(R8(0x051F), OP_EQ, 0), A(R8(0x0520), OP_EQ, 0),
               A(R8(0x053E), OP_EQ, 0), A(R8(0x053F), OP_EQ, 0),
               A(R8(0x055D), OP_EQ, 0), A(R8(0x055E), OP_EQ, 0)),
    { .act = ACT_NONE },
};

/* MetroidClassic.lua (NES layout in that file) vs Metroid.lua (SNES). */
static const struct rule metroid_classic_rules[] = {
    SW(1.0,    A(R8(0x0106), OP_EQ, 0), A(R8(0x0107), OP_EQ, 0)),
    SW(0.8,    A(R8(0x0056), OP_NE, 0)),
    { .act = ACT_NONE },
};
static const struct rule metroid_rules[] = {
    SW(1.6,    A(R16(0x0998), OP_EQ, 32)),
    SW(1.6,    A(R16(0x0998), OP_EQ, 35)),
    { .act = ACT_NONE },
};

/* Pokemon.lua: once a pokemon joins the team, switch. */
static const struct rule pokemon_rules[] = {
    SW(0.016,  A(R8(0x1163), OP_GE, 1)),
    { .act = ACT_NONE },
};

/* RiverCityRansom.lua: death, then screen transition. */
static const struct rule rivercityransom_rules[] = {
    SW(0.8,    A(R8(0x04BF), OP_EQ, 0)),
    SW(0.3,    A(R8(0x0042), OP_EQ, 1)),
    { .act = ACT_NONE },
};

/* Sonic.lua: signed big-endian 16-bit lives and score-bonus; the boss state
 * adds the boss HP byte check first. */
static const struct rule sonic_rules_level1[] = {
    SW(0.016,  B(R16BEP(0xFE12), OP_GT, R16BES(0xFE12))),
    SW(0.016,  A(R16BES(0xF7D2), OP_GT, 0)),
    { .act = ACT_NONE },
};
static const struct rule sonic_rules_boss[] = {
    SW(3.0,    A(R8S(0xD921), OP_EQ, 0)),
    SW(0.016,  B(R16BEP(0xFE12), OP_GT, R16BES(0xFE12))),
    SW(0.016,  A(R16BES(0xF7D2), OP_GT, 0)),
    { .act = ACT_NONE },
};

/* Starfox.lua: player HP zero, then stage-1 victory code. */
static const struct rule starfox_rules[] = {
    SW(1.8,    A(R8(0x0396), OP_EQ, 0)),
    SW(3.2,    A(R16(0x14AC), OP_EQ, 31)),
    { .act = ACT_NONE },
};

/* StreetFighter.lua: one rule set shared by both states (stage 7 + a timer
 * counter expired). */
static const struct rule streetfighter_rules[] = {
    SW(1.6,    A(R16(0x00E0), OP_EQ, 7), A(R16(0x0636), OP_LE, 0)),
    SW(1.6,    A(R16(0x00E0), OP_EQ, 7), A(R16(0x0836), OP_LE, 0)),
    { .act = ACT_NONE },
};

/* StreetsofRage2.lua: three fail conditions (OR), the boss-spawn latch, then
 * the latch-gated victory rule (boss HP + enemy lives both expired). */
static const struct rule sor2_rules[] = {
    SW(1.0,    A(R16BEPS(0xEFA8), OP_LE, 0)),
    SW(1.0,    A(R16BEPS(0xEFA8), OP_GT, 300)),
    SW(1.0,    A(R16BE(0xFC3C), OP_LE, 0)),
    LATCH(A(R16BE(0xF182), OP_GT, 0)),
    { .act = ACT_SWITCH, .flags = FLAG_NEED_LATCH, .wait_s = 1.6,
      .terms = { A(R16BE(0xF180), OP_LE, 0), A(R16BE(0xF182), OP_LE, 0) } },
    { .act = ACT_NONE },
};

/* SuperBomberman.lua: level cleared, then victory (immediate). */
static const struct rule super_bomberman_rules[] = {
    SW(1.6,    A(R8(0x2804), OP_NE, 0)),
    SW(0,      A(R8(0x0D7D), OP_EQ, 4)),
    { .act = ACT_NONE },
};

/* Tetris.lua: level-4 edge, then the OR-latched game-end (prev==0, cur>0). */
static const struct rule tetris_rules[] = {
    SW(1.07,   A(R8P(0x0048), OP_NE, 4), A(R8(0x0048), OP_EQ, 4)),
    SW(1.07,   A(R8P(0x0058), OP_EQ, 0), A(R8(0x0058), OP_GT, 0)),
    { .act = ACT_NONE },
};

/* Zelda1.lua: take-this flag edge (switch) or room-change reset; the boss
 * state keys on hearts (low/high nibble), heart container, and room 69. */
static const struct rule zelda1_rules_take_this[] = {
    SW(1.0,    A(R8P(0x0657), OP_EQ, 0), A(R8(0x0657), OP_NE, 0)),
    RS(0,      A(R8P(0x0006), OP_EQ, 0), A(R8(0x0006), OP_NE, 0)),
    { .act = ACT_NONE },
};
static const struct rule zelda1_rules_boss[] = {
    SW(0.8,    A(R8MP(0x066F, 0x0F, 0), OP_GT, 0), A(R8M(0x066F, 0x0F, 0), OP_EQ, 0), A(R8(0x0670), OP_EQ, 0)),
    SW(0.8,    A(R8P(0x0670), OP_GT, 0), A(R8M(0x066F, 0x0F, 0), OP_EQ, 0), A(R8(0x0670), OP_EQ, 0)),
    SW(0.5,    B(R8MP(0x066F, 0xF0, 4), OP_LT, R8M(0x066F, 0xF0, 4))),
    RS(0,      A(R8P(0x00EB), OP_NE, 69), A(R8(0x00EB), OP_EQ, 69)),
    { .act = ACT_NONE },
};

/* --- Cartridge identities: authored once per ROM, shared by its challenges -- */

static const struct game_meta game_alttp = {
    "A LINK TO THE PAST", "1991", "NINTENDO", "SNES" };
static const struct game_meta game_castlevania = {
    "CASTLEVANIA", "1987", "KONAMI", "NES" };
static const struct game_meta game_dkc = {
    "DONKEY KONG COUNTRY", "1994", "NINTENDO", "SNES" };
static const struct game_meta game_earthbound = {
    "EARTHBOUND", "1995", "NINTENDO", "SNES" };
static const struct game_meta game_gradius = {
    "GRADIUS", "1986", "KONAMI", "NES" };
static const struct game_meta game_kirby = {
    "KIRBY'S ADVENTURE", "1993", "NINTENDO", "NES" };
static const struct game_meta game_linksawakening = {
    "LINK'S AWAKENING DX", "1998", "NINTENDO", "GB" };
static const struct game_meta game_mario1 = {
    "SUPER MARIO BROS.", "1985", "NINTENDO", "NES" };
static const struct game_meta game_mario3 = {
    "SUPER MARIO BROS. 3", "1990", "NINTENDO", "NES" };
static const struct game_meta game_marioworld = {
    "SUPER MARIO WORLD", "1991", "NINTENDO", "SNES" };
static const struct game_meta game_megaman = {
    "MEGA MAN", "1987", "CAPCOM", "NES" };
static const struct game_meta game_metroid = {
    "METROID", "1987", "NINTENDO", "NES" };
static const struct game_meta game_pokemon = {
    "POKEMON RED", "1998", "NINTENDO", "GB" };
static const struct game_meta game_rivercity = {
    "RIVER CITY RANSOM", "1990", "TECHNOS", "NES" };
static const struct game_meta game_sonic = {
    "SONIC THE HEDGEHOG", "1991", "SEGA", "GEN" };
static const struct game_meta game_sor2 = {
    "STREETS OF RAGE 2", "1992", "SEGA", "GEN" };
static const struct game_meta game_starfox = {
    "STAR FOX", "1993", "NINTENDO", "SNES" };
static const struct game_meta game_sf2 = {
    "STREET FIGHTER II TURBO", "1993", "CAPCOM", "SNES" };
static const struct game_meta game_superbomberman = {
    "SUPER BOMBERMAN", "1993", "HUDSON SOFT", "SNES" };
static const struct game_meta game_supermetroid = {
    "SUPER METROID", "1994", "NINTENDO", "SNES" };
static const struct game_meta game_tetris = {
    "TETRIS", "1989", "NINTENDO", "NES" };
static const struct game_meta game_zelda1 = {
    "THE LEGEND OF ZELDA", "1987", "NINTENDO", "NES" };

/* --- Challenge table: the port of the ../challenges Lua handlers ----------- */

static const struct challenge challenges[] = {
    {
        "alttp_cell", "A Link to the Past - mini boss",
        "game_data/ROMS/Legend of Zelda, The - A Link to the Past (USA).zip",
        "game_data/states/A Link to the Past - mini boss.State",
        1.0, 0, 0, NULL, alttp_rules,
        .game = &game_alttp, .text = "Free the princess!",
    },
    {
        "castlevania", "Castlevania - level 1",
        "game_data/ROMS/Castlevania (USA) (Rev A).nes",
        "game_data/converted/Castlevania - level 1.libretro-quicknes.state",
        1.0, 0, 0, NULL, castlevania_rules,
        .game = &game_castlevania, .text = "Finish the screen!",
    },
    {
        "donkeykong", "Donkey Kong Country - level 1",
        "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).sfc",
        "game_data/converted/Donkey Kong Country - level 1.s9x",
        1.0, 0, 0, NULL, dkc_rules_level1,
        .game = &game_dkc, .text = "Finish the level!",
    },
    {
        "donkeykong", "Donkey Kong Country - barrel level",
        "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).sfc",
        "game_data/converted/Donkey Kong Country - barrel level.s9x",
        1.0, 0, 0, NULL, dkc_rules_barrel,
        .game = &game_dkc, .text = "Pass the barrels!",
    },
    {
        "donkeykong", "Donkey Kong Country - boss 1",
        "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).sfc",
        "game_data/converted/Donkey Kong Country - boss 1.s9x",
        1.0, 0, 0, NULL, dkc_rules_boss1,
        .game = &game_dkc, .text = "Beat the boss!",
    },
    {
        "earthbound", "EarthBound - battle",
        "game_data/ROMS/EarthBound (USA).zip",
        "game_data/states/EarthBound - battle.State",
        1.0, 0, 0, NULL, earthbound_rules,
        .game = &game_earthbound, .text = "Win the fight!",
    },
    {
        "gradius", "Gradius - boss",
        "game_data/ROMS/Gradius (USA).zip",
        "game_data/states/Gradius - boss.State",
        1.0, 0, 0, NULL, gradius_rules,
        .game = &game_gradius, .text = "Defeat the boss!",
    },
    {
        "kirby", "Kirby's Adventure - mini boss",
        "game_data/ROMS/Kirby's Adventure (USA) (Rev A).nes",
        "game_data/converted/Kirby's Adventure - mini boss.libretro-quicknes.state",
        1.0, 0, 0, NULL, kirby_rules_miniboss,
        .game = &game_kirby, .text = "Defeat the boss!",
    },
    {
        "kirby", "Kirby's Adventure - level 1 (until door)",
        "game_data/ROMS/Kirby's Adventure (USA) (Rev A).nes",
        "game_data/converted/Kirby's Adventure - level 1 (until door).libretro-quicknes.state",
        1.0, 0, 0, NULL, kirby_rules_level1,
        .game = &game_kirby, .text = "Reach the door!",
    },
    {
        "awakening_boss", "Link's Awakening - mini boss",
        "game_data/ROMS/Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).gb",
        "game_data/converted/Link's Awakening - mini boss.libretro-gambatte.state",
        1.0, 0, 0, NULL, linksawakening_rules,
        .game = &game_linksawakening, .text = "Defeat the boss!",
    },
    {
        "mario1", "Super Mario Bros - castle",
        "game_data/ROMS/Super Mario Bros. (Japan, USA).nes",
        "game_data/converted/Super Mario Bros - castle.libretro-quicknes.state",
        0.5, 0, 0, NULL, mario1_rules_castle,
        .game = &game_mario1, .text = "Defeat Bowser!",
    },
    {
        "mario1", "Super Mario Bros - 1-1",
        "game_data/ROMS/Super Mario Bros. (Japan, USA).nes",
        "game_data/converted/Super Mario Bros - 1-1.libretro-quicknes.state",
        0.5, 0, 0, NULL, mario1_rules_1_1,
        .game = &game_mario1, .text = "Beat the level!",
    },
    {
        "mario3", "Super Mario Bros. 3 - first mini boss",
        "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).nes",
        "game_data/converted/Super Mario Bros. 3 - first mini boss.libretro-quicknes.state",
        0.5, 0, 0, NULL, mario3_rules_miniboss,
        .game = &game_mario3, .text = "Beat the boss!",
    },
    {
        "mario3", "Super Mario Bros. 3 - crushing ceiling",
        "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).nes",
        "game_data/converted/Super Mario Bros. 3 - crushing ceiling.libretro-quicknes.state",
        0.5, 0, 0, NULL, mario3_rules_ceiling,
        .game = &game_mario3, .text = "Reach the door!",
    },
    {
        "mario3", "Super Mario Bros. 3 - hammer bro",
        "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).nes",
        "game_data/converted/Super Mario Bros. 3 - hammer bro.libretro-quicknes.state",
        0.5, 0, 0, NULL, mario3_rules_hammer,
        .game = &game_mario3, .text = "Beat the hammer bro!",
    },
    {
        "marioworld", "Super Mario World - level 1",
        "game_data/ROMS/Super Mario World (USA).sfc",
        "game_data/converted/Super Mario World - level 1.s9x",
        0.5, 0, 0, NULL, marioworld_rules_level1,
        .game = &game_marioworld, .text = "Beat the level!",
    },
    {
        "marioworld", "Super Mario World - castle level",
        "game_data/ROMS/Super Mario World (USA).sfc",
        "game_data/converted/Super Mario World - castle level.s9x",
        0.5, 0, 0, NULL, marioworld_rules_castle,
        .game = &game_marioworld, .text = "Reach the door!",
    },
    {
        "marioworld", "Super Mario World - first boss",
        "game_data/ROMS/Super Mario World (USA).sfc",
        "game_data/converted/Super Mario World - first boss.s9x",
        0.5, 0, 0, NULL, marioworld_rules_boss,
        .game = &game_marioworld, .text = "Defeat the boss!",
    },
    {
        "marioworldinterlude", "Super Mario World - interlude",
        "game_data/ROMS/Super Mario World (USA).sfc",
        "game_data/converted/Super Mario World - interlude.s9x",
        1.0, 1, 1, interlude_writes, marioworld_interlude_rules,
        .game = &game_marioworld, .text = "",
    },
    {
        "megaman", "Mega Man - bomb man",
        "game_data/ROMS/Mega Man (USA).nes",
        "game_data/converted/Mega Man - bomb man.libretro-quicknes.state",
        1.0, 0, 0, NULL, megaman_rules,
        .game = &game_megaman, .text = "Finish the screen!",
    },
    {
        "megaman", "Mega Man - fire man",
        "game_data/ROMS/Mega Man (USA).nes",
        "game_data/converted/Mega Man - fire man.libretro-quicknes.state",
        1.0, 0, 0, NULL, megaman_rules,
        .game = &game_megaman, .text = "Finish the screen!",
    },
    {
        "megaman", "Mega Man - cut man",
        "game_data/ROMS/Mega Man (USA).nes",
        "game_data/converted/Mega Man - cut man.libretro-quicknes.state",
        1.0, 0, 0, NULL, megaman_rules,
        .game = &game_megaman, .text = "Finish the screen!",
    },
    {
        "metroid_classic", "Metroid - level 1",
        "game_data/ROMS/Metroid (USA).zip",
        "game_data/states/Metroid - level 1.State",
        1.0, 0, 0, NULL, metroid_classic_rules,
        .game = &game_metroid, .text = "Reach the door!",
    },
    {
        "supermetroid_escape", "Super Metroid - First Escape",
        "game_data/ROMS/Super Metroid (Japan, USA) (En,Ja).sfc",
        "game_data/converted/Super Metroid - First Escape.s9x",
        1.0, 0, 0, NULL, metroid_rules,
        .game = &game_supermetroid, .text = "Escape!",
    },
    {
        "pokemon", "Pokemon Red - choose pokemon",
        "game_data/ROMS/Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb",
        "game_data/converted/Pokemon Red - choose pokemon.libretro-gambatte.state",
        1.0, 0, 0, NULL, pokemon_rules,
        .game = &game_pokemon, .text = "Choose a Pokemon!",
    },
    {
        "rivercityransom", "River City Ransom - level 1",
        "game_data/ROMS/River City Ransom (USA).zip",
        "game_data/states/River City Ransom - level 1.State",
        1.0, 0, 0, NULL, rivercityransom_rules,
        .game = &game_rivercity, .text = "Finish the screen!",
    },
    {
        "sonic", "Sonic The Hedgehog - level 1",
        "game_data/ROMS/Sonic The Hedgehog (USA, Europe).md",
        "game_data/converted/Sonic The Hedgehog - level 1.libretro-gpgx.state",
        1.0, 0, 0, NULL, sonic_rules_level1,
        .game = &game_sonic, .text = "Beat the level!",
    },
    {
        "sonic", "Sonic The Hedgehog - boss 1",
        "game_data/ROMS/Sonic The Hedgehog (USA, Europe).md",
        "game_data/converted/Sonic The Hedgehog - boss 1.libretro-gpgx.state",
        1.0, 0, 0, NULL, sonic_rules_boss,
        .game = &game_sonic, .text = "Defeat the boss!",
    },
    {
        "starfox", "Star Fox - boss",
        "game_data/ROMS/Star Fox (USA).zip",
        "game_data/states/Star Fox - boss.State",
        1.0, 0, 0, NULL, starfox_rules,
        .game = &game_starfox, .text = "Beat the boss!",
    },
    {
        "sf2", "Street Fighter II Turbo - blanka vs dhalsim",
        "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip",
        "game_data/states/Street Fighter II Turbo - blanka vs dhalsim.State",
        1.0, 0, 0, NULL, streetfighter_rules,
        .game = &game_sf2, .text = "Win the fight!",
    },
    {
        "sf2", "Street Fighter II Turbo - ryu vs guile",
        "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip",
        "game_data/states/Street Fighter II Turbo - ryu vs guile.State",
        1.0, 0, 0, NULL, streetfighter_rules,
        .game = &game_sf2, .text = "Win the fight!",
    },
    {
        "streetsofrage2", "Streets of Rage 2 - mini boss 1",
        "game_data/ROMS/Streets of Rage 2 (USA).md",
        "game_data/converted/Streets of Rage 2 - mini boss 1.libretro-gpgx.state",
        1.0, 0, 0, NULL, sor2_rules,
        .game = &game_sor2, .text = "Defeat the boss!",
    },
    {
        "superbomberman", "Super Bomberman - level 1",
        "game_data/ROMS/Super Bomberman (USA).zip",
        "game_data/states/Super Bomberman - level 1.State",
        1.0, 0, 0, NULL, super_bomberman_rules,
        .game = &game_superbomberman, .text = "Finish the level!",
    },
    {
        "tetris", "Tetris",
        "game_data/ROMS/Tetris (USA).nes",
        "game_data/converted/Tetris.libretro-quicknes.state",
        1.0, 0, 0, NULL, tetris_rules,
        .game = &game_tetris, .text = "Make 1 line!",
    },
    {
        "zelda1", "Legend of Zelda - take this",
        "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).nes",
        "game_data/converted/Legend of Zelda - take this.libretro-quicknes.state",
        1.0, 0, 0, NULL, zelda1_rules_take_this,
        .game = &game_zelda1, .text = "",
    },
    {
        "zelda1", "Legend of Zelda - boss 1",
        "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).nes",
        "game_data/converted/Legend of Zelda - boss 1.libretro-quicknes.state",
        1.0, 0, 0, NULL, zelda1_rules_boss,
        .game = &game_zelda1, .text = "Defeat the boss!",
    },
};

#define N_CHALLENGES ((int)(sizeof(challenges) / sizeof(challenges[0])))
/* --- Runner state ---------------------------------------------------------- */

#define MAX_SHADOWS 48
#define MAX_RULES   64
#define PLAYED_PENALTY   0.01
/* From Game.lua: `interval = 180  -- 5 seconds` (the code wins). */
#define INTERLUDE_INTERVAL_S 180.0

struct shadow { uint16_t addr; uint8_t size; int32_t val; int valid; };

static int         g_cur = 0;
static int         g_avail[N_CHALLENGES];
static double      g_dynw[N_CHALLENGES];
static char        g_cur_rom[4096] = "";
static char        g_cur_core[4096] = "";
static void        noop(void);
static int         g_pending_switch = 0, g_pending_reset = 0;
static uint64_t    g_switch_at = 0, g_reset_at = 0;
static unsigned    g_switch_ms = 0, g_reset_ms = 0;
static int         g_latch = 0;
static int         g_force_switch = 0;   /* T key */
static uint64_t    g_last_interlude = 0;
static uint16_t    g_stable[MAX_RULES];
static struct shadow g_shadow[MAX_SHADOWS];
static int         g_nshadow = 0;

/* --- Title card ------------------------------------------------------------
 *
 * Upstream superhugboy overlays the current challenge's `challenge_text` on the
 * frame for five seconds after a state load, and draws nothing else. The port
 * restores that as the bottom plate and adds a top plate carrying the ROM's
 * identity. The game keeps running underneath: the card is a pure overlay, so
 * nothing is paused, no input is gated, and the video transform is untouched.
 * See docs/title-card.md.
 */

#define CARD_MS          5000  /* upstream's `challenge_text_timer = 5` */
#define CARD_PAD_X       8     /* margin, framebuffer px */
#define CARD_PAD_Y       4     /* plate padding, framebuffer px */
#define CARD_CELL_H      8
#define CARD_GLYPH_W     5     /* glyph box inside an 8x8 atlas cell */
#define CARD_CELL_MIN_W  6     /* narrow cell: 5 px glyph + 1 px gap */
#define CARD_CELL_MAX_W  8
#define CARD_MAX_LINES   4     /* per plate */
#define CARD_MAX_COLS    96
/* The card's size, in multiples of the glyph scale the framebuffer maps to.
 * The plates are sized from their text, so this grows the bars with it. The
 * scale steps down from here when the window cannot hold the result. */
#define CARD_SCALE_MULT  3
#define CARD_ATLAS_CELL  8
#define CARD_ATLAS_COLS  16
#define CARD_ATLAS_W     (CARD_ATLAS_COLS * CARD_ATLAS_CELL)
#define CARD_ATLAS_H     (8 * CARD_ATLAS_CELL)
#define CARD_FIRST_CHAR  0x20
#define CARD_LAST_CHAR   0x7E
#define CARD_MAX_VERTS   (6 * CARD_MAX_LINES * 2 * CARD_MAX_COLS)

/* 5x7 glyphs, one byte per row, bit 4 = leftmost column. ASCII 0x20-0x7E. */
static const uint8_t card_font[][7] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /*   */
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 }, /* ! */
    { 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* " */
    { 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A }, /* # */
    { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 }, /* $ */
    { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 }, /* % */
    { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D }, /* & */
    { 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' */
    { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 }, /* ( */
    { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 }, /* ) */
    { 0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00 }, /* * */
    { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, /* + */
    { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08 }, /* , */
    { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, /* - */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C }, /* . */
    { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 }, /* / */
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, /* 0 */
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* 1 */
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, /* 2 */
    { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E }, /* 3 */
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, /* 4 */
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, /* 5 */
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, /* 6 */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, /* 7 */
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, /* 8 */
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, /* 9 */
    { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 }, /* : */
    { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x08 }, /* ; */
    { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 }, /* < */
    { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 }, /* = */
    { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 }, /* > */
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 }, /* ? */
    { 0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E }, /* @ */
    { 0x04, 0x0A, 0x11, 0x11, 0x1F, 0x11, 0x11 }, /* A */
    { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, /* B */
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, /* C */
    { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C }, /* D */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, /* E */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, /* F */
    { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, /* G */
    { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, /* H */
    { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* I */
    { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, /* J */
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, /* K */
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, /* L */
    { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, /* M */
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, /* N */
    { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* O */
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, /* P */
    { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, /* Q */
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, /* R */
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E }, /* S */
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, /* T */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* U */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 }, /* V */
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, /* W */
    { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, /* X */
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, /* Y */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, /* Z */
    { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E }, /* [ */
    { 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01 }, /* \ */
    { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E }, /* ] */
    { 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00 }, /* ^ */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F }, /* _ */
    { 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ` */
    { 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F }, /* a */
    { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E }, /* b */
    { 0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E }, /* c */
    { 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F }, /* d */
    { 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E }, /* e */
    { 0x06, 0x08, 0x08, 0x1E, 0x08, 0x08, 0x08 }, /* f */
    { 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x0E }, /* g */
    { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11 }, /* h */
    { 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E }, /* i */
    { 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C }, /* j */
    { 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12 }, /* k */
    { 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* l */
    { 0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11 }, /* m */
    { 0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11 }, /* n */
    { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E }, /* o */
    { 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10 }, /* p */
    { 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x01 }, /* q */
    { 0x00, 0x00, 0x16, 0x18, 0x10, 0x10, 0x10 }, /* r */
    { 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E }, /* s */
    { 0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06 }, /* t */
    { 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D }, /* u */
    { 0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04 }, /* v */
    { 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A }, /* w */
    { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 }, /* x */
    { 0x00, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x0E }, /* y */
    { 0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F }, /* z */
    { 0x03, 0x04, 0x04, 0x08, 0x04, 0x04, 0x03 }, /* { */
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, /* | */
    { 0x18, 0x04, 0x04, 0x02, 0x04, 0x04, 0x18 }, /* } */
    { 0x00, 0x00, 0x0D, 0x12, 0x00, 0x00, 0x00 }, /* ~ */
};

/* Text colours for the two plates. The objective is the thing the player acts
 * on, so it carries the accent; the cartridge's identity stays neutral. The
 * glyph atlas is white, so these are pure multipliers. */
static const float CARD_TITLE_COLOR[3] = { 1.0f, 1.0f, 1.0f };
static const float CARD_TEXT_COLOR[3]  = { 1.0f, 1.0f, 0.0f };

struct card_vert { float x, y, u, v; };

static struct {
    GLuint   vao, vbo, atlas;
    uint64_t armed_at;
    int      showing;
    struct card_vert verts[CARD_MAX_VERTS];
} g_card = { 0 };

/* Arm the card for the challenge now in g_cur. Loaded anywhere a savestate is
 * loaded, so the first drop, an ordinary switch, `T` and `ACT_RESET` all show
 * it, and a challenge with no text shows nothing. */
static void card_begin(void) {
    const struct challenge *c = &challenges[g_cur];
    g_card.armed_at = SDL_GetTicks64();
    g_card.showing = (c->game && c->text && c->text[0]) ? 1 : 0;
}

static void card_deinit(void) {
    if (g_card.atlas) glDeleteTextures(1, &g_card.atlas);
    if (g_card.vao)   glDeleteVertexArrays(1, &g_card.vao);
    if (g_card.vbo)   glDeleteBuffers(1, &g_card.vbo);
    g_card.atlas = 0;
    g_card.vao = 0;
    g_card.vbo = 0;
}

/* Build the glyph atlas and the card's own VAO/VBO. The game quad's buffer is
 * rebuilt only on a video configure, so the card must not share it. */
static void card_init_gl(void) {
    uint8_t px[CARD_ATLAS_W * CARD_ATLAS_H * 4];
    memset(px, 0, sizeof px);

    for (size_t g = 0; g < sizeof card_font / sizeof card_font[0]; g++) {
        int ox = (int)(g % CARD_ATLAS_COLS) * CARD_ATLAS_CELL;
        int oy = (int)(g / CARD_ATLAS_COLS) * CARD_ATLAS_CELL;
        for (int ry = 0; ry < 7; ry++) {
            for (int rx = 0; rx < CARD_GLYPH_W; rx++) {
                if (!(card_font[g][ry] & (1 << (CARD_GLYPH_W - 1 - rx))))
                    continue;
                uint8_t *d = &px[((oy + ry) * CARD_ATLAS_W + ox + rx) * 4];
                d[0] = 255;
                d[1] = 255;
                d[2] = 255;
                d[3] = 255;
            }
        }
    }

    glGenTextures(1, &g_card.atlas);
    glBindTexture(GL_TEXTURE_2D, g_card.atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* video_refresh leaves UNPACK_ROW_LENGTH set to the video pitch for its own
     * upload; inheriting it here would read the atlas rows at the wrong stride. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CARD_ATLAS_W, CARD_ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &g_card.vao);
    glGenBuffers(1, &g_card.vbo);
    glBindVertexArray(g_card.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_card.vbo);
    glEnableVertexAttribArray(g_shader.i_pos);
    glEnableVertexAttribArray(g_shader.i_coord);
    glVertexAttribPointer(g_shader.i_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(struct card_vert), (void *)0);
    glVertexAttribPointer(g_shader.i_coord, 2, GL_FLOAT, GL_FALSE,
                          sizeof(struct card_vert), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* Uppercase, drop what the atlas cannot draw, and greedily wrap `src` into at
 * most `max` lines of `cols` columns. Returns the number of lines written.
 * Sets *truncated when the plate was too small to show every word, which the
 * caller treats as "this scale does not fit". */
static int card_paragraph(const char *src, int cols, char out[][CARD_MAX_COLS], int max,
                          int *truncated) {
    int line = 0;
    size_t len = 0;

    if (max < 1)
        return 0;
    if (cols < 1)
        cols = 1;
    out[0][0] = '\0';

    while (*src) {
        char word[CARD_MAX_COLS];
        size_t wlen = 0;

        while (*src == ' ')
            src++;
        if (!*src)
            break;
        while (*src && *src != ' ') {
            char ch = *src++;
            if (ch >= 'a' && ch <= 'z')
                ch -= 'a' - 'A';
            if ((unsigned char)ch >= 0x80) {
                /* Skip the rest of a UTF-8 sequence: "pokemon" must not become
                 * "pokmon" if a transliterated string ever slips back in. */
                while (*src && ((unsigned char)*src & 0xC0) == 0x80)
                    src++;
                continue;
            }
            if (ch < CARD_FIRST_CHAR || ch > CARD_LAST_CHAR)
                continue;
            if (wlen + 1 < sizeof word)
                word[wlen++] = ch;
        }
        if (!wlen)
            continue;
        if (wlen > (size_t)cols) {
            /* A word this plate cannot hold at all: no number of lines saves
             * it, so the layout has to shrink. Cut it rather than overrun. */
            *truncated = 1;
            wlen = (size_t)cols;
        }
        if (len && len + 1 + wlen > (size_t)cols) {
            if (line + 1 >= max) {
                *truncated = 1;
                break;
            }
            /* The break already separates the fields, so a separator left
             * dangling at the end of the line only reads as a typo. */
            if (len >= 2 && out[line][len - 1] == '-' && out[line][len - 2] == ' ') {
                len -= 2;
                out[line][len] = '\0';
            }
            line++;
            out[line][0] = '\0';
            len = 0;
        }
        if (len)
            out[line][len++] = ' ';
        memcpy(out[line] + len, word, wlen);
        len += wlen;
        out[line][len] = '\0';
    }

    /* Greedy filling can strand one short word on the last line ("STREETS OF
     * RAGE" / "2"). Pull the previous line's last word down, but only when that
     * leaves a readable line behind — never one ending in a bare separator. */
    if (line >= 1) {
        char *prev = out[line - 1];
        char *last = out[line];
        size_t plen = strlen(prev), llen = strlen(last), cut = plen;

        while (cut > 0 && prev[cut - 1] != ' ')
            cut--;
        if (cut > 0 && llen * 2 < plen && cut - 1 >= 4 &&
            prev[cut - 2] != '-' && llen + plen - cut + 1 <= (size_t)cols) {
            char merged[CARD_MAX_COLS];
            snprintf(merged, sizeof merged, "%s %s", prev + cut, last);
            prev[cut - 1] = '\0';
            memcpy(last, merged, strlen(merged) + 1);
        }
    }

    return line + 1;
}

/* Longest run of printing characters in s, as the wrapper sees it. A line can
 * always be wrapped; a single word cannot, so this is the floor on columns. */
static int card_longest_word(const char *s) {
    int best = 1, run = 0;
    for (; *s; s++) {
        char ch = *s;
        if (ch >= 'a' && ch <= 'z')
            ch -= 'a' - 'A';
        if (ch == ' ') {
            run = 0;   /* only a space ends a word */
        } else if (ch >= CARD_FIRST_CHAR && ch <= CARD_LAST_CHAR) {
            if (++run > best)
                best = run;
        }
        /* Anything the atlas cannot draw is dropped without breaking the word. */
    }
    return best;
}

/* What the card is before anything is drawn: the cell and scale that fit, the
 * wrapped lines, and the plate heights that follow from them. */
struct card_layout {
    int  cell_w, scale, cols, line_h, pad_y;
    int  ntop, nbot, top_h, bot_h;
    int  ok;                  /* every word shown, both plates inside the window */
    char top[CARD_MAX_LINES][CARD_MAX_COLS];
    char bot[CARD_MAX_LINES][CARD_MAX_COLS];
};

/* Lay the card out at one glyph scale. The text is measured in window pixels,
 * because that is where it lands, so a scale too large for the window is
 * reported through `ok` rather than drawn off the edge. */
static void card_layout_compute(int win_w, int win_h, int scale, const char *title,
                                const char *meta, const char *text,
                                struct card_layout *L) {
    int wm, wt, word, cols, truncated = 0;
    int room = win_w - 2 * CARD_PAD_X * scale;
    size_t longest = strlen(title);

    memset(L, 0, sizeof *L);
    L->scale = scale;
    if (strlen(meta) > longest)
        longest = strlen(meta);
    if (strlen(text) > longest)
        longest = strlen(text);

    /* Largest cell at which every authored line still fits whole. */
    L->cell_w = CARD_CELL_MIN_W;
    if ((int)longest <= room / (CARD_CELL_MAX_W * scale))
        L->cell_w = CARD_CELL_MAX_W;
    cols = room / (L->cell_w * scale);

    /* A word longer than a line can never be shown, whatever the wrapping does,
     * so that alone rules the wider cell out. */
    word = card_longest_word(title);
    wm = card_longest_word(meta);
    wt = card_longest_word(text);
    if (wm > word)
        word = wm;
    if (wt > word)
        word = wt;

    if (cols < word && L->cell_w == CARD_CELL_MAX_W) {
        L->cell_w = CARD_CELL_MIN_W;
        cols = room / (L->cell_w * scale);
    }
    if (cols > CARD_MAX_COLS - 1)
        cols = CARD_MAX_COLS - 1;
    if (cols < word || cols < 1) {
        L->cols = 1;
        return;                          /* ok stays 0: shrink further */
    }

    L->cols = cols;
    L->ntop = card_paragraph(title, cols, L->top, CARD_MAX_LINES, &truncated);
    if (L->ntop < CARD_MAX_LINES)
        L->ntop += card_paragraph(meta, cols, L->top + L->ntop,
                                  CARD_MAX_LINES - L->ntop, &truncated);
    L->nbot = card_paragraph(text, cols, L->bot, CARD_MAX_LINES, &truncated);

    L->line_h = CARD_CELL_H * scale;
    L->pad_y  = CARD_PAD_Y * scale;
    L->top_h  = L->ntop * L->line_h + 2 * L->pad_y;
    L->bot_h  = L->nbot * L->line_h + 2 * L->pad_y;
    L->ok = !truncated && L->top_h + L->bot_h <= win_h;
}

/* Pixel-space matrix for the card's own pass. The vertex shader multiplies the
 * position as a row vector (`vec4(i_pos, 0, 1) * u_mvp`), so the matrix it
 * consumes is the transpose of the usual column-vector ortho: the translation
 * belongs in the last column of this layout, and the bottom row stays
 * (0, 0, 0, 1) so w comes out as 1. The game quad's own matrix is translation
 * free, which is why ortho2d() never had to care. */
static void card_ortho(int win_w, int win_h, float m[4][4]) {
    memset(m, 0, sizeof(float) * 16);
    m[0][0] =  2.0f / (float)win_w;
    m[1][1] = -2.0f / (float)win_h;
    m[0][3] = -1.0f;
    m[1][3] =  1.0f;
    m[2][2] =  1.0f;
    m[3][3] =  1.0f;
}

/* Emit the quads for one line of text, centred horizontally, top at `y`. */
static void card_emit_line(const char *line, int cell_w, int scale, int win_w, int y,
                           struct card_vert **vpp, int *nv, int maxv) {
    size_t n = strlen(line);
    struct card_vert *v = *vpp;
    int added = 0;

    if (n) {
        int gw = cell_w * scale, gh = CARD_CELL_H * scale;
        int x0 = ((win_w - (int)n * gw) / (2 * scale)) * scale;

        for (size_t i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)line[i];
            int g = (ch >= CARD_FIRST_CHAR && ch <= CARD_LAST_CHAR)
                  ? ch - CARD_FIRST_CHAR : 0;
            float u0 = (float)((g % CARD_ATLAS_COLS) * CARD_ATLAS_CELL) / CARD_ATLAS_W;
            float t0 = (float)((g / CARD_ATLAS_COLS) * CARD_ATLAS_CELL) / CARD_ATLAS_H;
            float u1 = u0 + (float)cell_w / CARD_ATLAS_W;
            float t1 = t0 + (float)CARD_ATLAS_CELL / CARD_ATLAS_H;
            float xa = (float)(x0 + (int)i * gw), ya = (float)y;
            float xb = xa + gw, yb = ya + gh;
            struct card_vert quad[6] = {
                { xa, ya, u0, t0 }, { xb, ya, u1, t0 }, { xa, yb, u0, t1 },
                { xb, ya, u1, t0 }, { xb, yb, u1, t1 }, { xa, yb, u0, t1 },
            };

            if (*nv + added + 6 > maxv)
                break;
            memcpy(v + added, quad, sizeof quad);
            added += 6;
        }
    }

    *vpp = v + added;
    *nv += added;
}

/* The two plates plus their text, over the frame the game just produced. */
static void card_draw(int win_w, int win_h) {
    const struct challenge *c;
    struct card_layout L;
    char meta[CARD_MAX_COLS];
    GLfloat clear[4];
    float m[4][4];
    struct card_vert *v;
    int fb_w, fb_h, base, scale;
    int nv = 0, title_verts = 0;

    if (!g_card.showing)
        return;
    if (SDL_GetTicks64() - g_card.armed_at >= CARD_MS) {
        g_card.showing = 0;   /* hard cut: no fade */
        return;
    }
    if (!g_card.atlas)
        card_init_gl();
    if (!g_card.atlas || win_w < 32 || win_h < 32)
        return;

    c = &challenges[g_cur];
    snprintf(meta, sizeof meta, "%s - %s - %s", c->game->year,
             c->game->publisher, c->game->platform);

    /* Integer, square-pixel scale: the card never antialiases or stretches.
     * Start at the requested multiple of the framebuffer's own scale, then step
     * down until the window can hold the result. */
    fb_w = g_video.clip_w > 0 ? (int)g_video.clip_w : 256;
    fb_h = g_video.clip_h > 0 ? (int)g_video.clip_h : 224;
    base = win_w / fb_w;
    if (win_h / fb_h < base)
        base = win_h / fb_h;
    if (base < 1)
        base = 1;

    scale = base * CARD_SCALE_MULT;
    do {
        card_layout_compute(win_w, win_h, scale, c->game->title, meta, c->text, &L);
    } while (!L.ok && --scale >= 1);

    if (L.top_h > win_h)
        L.top_h = win_h;
    if (L.bot_h > win_h)
        L.bot_h = win_h;

    v = g_card.verts;
    for (int i = 0; i < L.ntop; i++)
        card_emit_line(L.top[i], L.cell_w, L.scale, win_w, L.pad_y + i * L.line_h,
                       &v, &nv, CARD_MAX_VERTS);
    title_verts = nv;   /* the top plate is drawn first, and tinted differently */
    for (int i = 0; i < L.nbot; i++)
        card_emit_line(L.bot[i], L.cell_w, L.scale, win_w,
                       win_h - L.bot_h + L.pad_y + i * L.line_h,
                       &v, &nv, CARD_MAX_VERTS);
    if (!nv)
        return;

    /* Plates: an opaque black clear inside the plate rect, over the game image.
     * Full window width, because the video quad fills the window. */
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glScissor(0, win_h - L.top_h, win_w, L.top_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(0, 0, win_w, L.bot_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);

    card_ortho(win_w, win_h, m);

    glUseProgram(g_shader.program);
    glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float *)m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_card.atlas);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(g_card.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_card.vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)nv * (long)sizeof(struct card_vert),
                 g_card.verts, GL_STREAM_DRAW);
    glUniform4f(g_shader.u_tint, CARD_TITLE_COLOR[0], CARD_TITLE_COLOR[1],
                CARD_TITLE_COLOR[2], 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, title_verts);
    if (nv > title_verts) {
        glUniform4f(g_shader.u_tint, CARD_TEXT_COLOR[0], CARD_TEXT_COLOR[1],
                    CARD_TEXT_COLOR[2], 1.0f);
        glDrawArrays(GL_TRIANGLES, title_verts, nv - title_verts);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    /* The game quad shares this program and these uniforms: put them back. */
    glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float *)g_game_mvp);
    glUniform4f(g_shader.u_tint, 1.0f, 1.0f, 1.0f, 1.0f);
    glUseProgram(0);
}

/* Cached system RAM, refreshed once per evaluation. */
static uint8_t *g_ram = NULL;
static size_t   g_ram_sz = 0;

static void ram_refresh(void) {
    g_ram = g_retro.retro_get_memory_data
         ? g_retro.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) : NULL;
    g_ram_sz = (g_ram && g_retro.retro_get_memory_size)
            ? g_retro.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) : 0;
}

/* Raw value of a sample (masked/shifted, no offset). *ok = 0 when the RAM
 * region is unavailable or out of bounds. */
static int32_t read_raw(const struct sample *s, int *ok) {
    *ok = 0;
    if (!g_ram)
        return 0;
    if ((size_t)s->addr + s->size > g_ram_sz)
        return 0;
    /* Mask the raw (unsigned) bits first, THEN sign-extend: a signed read
     * with a full-width mask (the only combination the handlers use) must
     * yield -1 for 0xFF / 0xFFFF, which sign-then-mask would destroy. */
    uint32_t raw;
    if (s->size == 1)
        raw = g_ram[s->addr];
    else
        raw = s->be
            ? ((uint32_t)g_ram[s->addr] << 8) | g_ram[s->addr + 1]
            : (uint32_t)g_ram[s->addr] | ((uint32_t)g_ram[s->addr + 1] << 8);
    raw &= (uint32_t)s->mask;
    int32_t v;
    if (s->sgn)
        v = (s->size == 1) ? (int32_t)(int8_t)(raw & 0xFF)
                           : (int32_t)(int16_t)(raw & 0xFFFF);
    else
        v = (int32_t)raw;
    if (s->shift)
        v >>= s->shift;
    *ok = 1;
    return v;
}

/* Full value of a sample: shadow lookup for prev, else live read; offset
 * applied last (so a prev sample's offset composes, as in Kirby's
 * `prev_score + 100`). */
static int32_t sample_value(const struct sample *s, int *ok) {
    int32_t v;
    if (s->prev) {
        for (int i = 0; i < g_nshadow; i++) {
            if (g_shadow[i].addr == s->addr && g_shadow[i].size == s->size) {
                *ok = g_shadow[i].valid;
                v = g_shadow[i].valid ? g_shadow[i].val + s->offset : 0;
                return v;
            }
        }
        *ok = 0;
        return 0;
    }
    v = read_raw(s, ok);
    if (!*ok)
        return 0;
    return v + s->offset;
}

static int term_eval(const struct term *t) {
    int32_t va, vb;
    int oka, okb;
    va = sample_value(&t->a, &oka);
    if (t->b_is_const) {
        vb = t->c;
        okb = 1;
    } else {
        vb = sample_value(&t->b, &okb);
    }
    if (!oka || !okb)
        return 0;
    switch (t->op) {
    case OP_EQ: return va == vb;
    case OP_NE: return va != vb;
    case OP_LT: return va < vb;
    case OP_LE: return va <= vb;
    case OP_GT: return va > vb;
    case OP_GE: return va >= vb;
    default:    return 0;
    }
}

static int rule_match(const struct rule *r, int ri) {
    for (int t = 0; t < 12; t++) {
        const struct term *term = &r->terms[t];
        if (term->op == OP_NONE)
            break;
        if (!term_eval(term)) {
            if (r->stable_frames)
                g_stable[ri] = 0;
            return 0;
        }
    }
    if (r->stable_frames) {
        /* Lua: the counter increments on each match and fires when > N,
         * i.e. on frame N+1. g_stable reaches N after frame N. */
        if (g_stable[ri] < r->stable_frames) {
            g_stable[ri]++;
            return 0;
        }
    }
    return 1;
}

/* Collect the prev-shadow slots a challenge needs (mirrors the
 * `state.prev_* = state.prev_* or current` initialization). */
static void collect_shadows(const struct challenge *c) {
    g_nshadow = 0;
    for (int ri = 0; ri < MAX_RULES && c->rules[ri].act != ACT_NONE; ri++) {
        for (int t = 0; t < 12; t++) {
            const struct term *term = &c->rules[ri].terms[t];
            if (term->op == OP_NONE)
                break;
            const struct sample *candidates[2] = {
                &term->a, term->b_is_const ? NULL : &term->b
            };
            for (int k = 0; k < 2; k++) {
                const struct sample *s = candidates[k];
                if (!s || !s->prev)
                    continue;
                int found = 0;
                for (int i = 0; i < g_nshadow; i++)
                    if (g_shadow[i].addr == s->addr && g_shadow[i].size == s->size) {
                        found = 1;
                        break;
                    }
                if (!found && g_nshadow < MAX_SHADOWS) {
                    g_shadow[g_nshadow].addr = s->addr;
                    g_shadow[g_nshadow].size = s->size;
                    g_shadow[g_nshadow].val = 0;
                    g_shadow[g_nshadow].valid = 0;
                    g_nshadow++;
                }
            }
        }
    }
}

static void refresh_shadows(void) {
    for (int i = 0; i < g_nshadow; i++) {
        struct sample s = { g_shadow[i].addr, g_shadow[i].size,
                            0, 0, 0, 0xFFFF, 0, 0 };
        int ok;
        g_shadow[i].val = read_raw(&s, &ok);
        g_shadow[i].valid = 1;
    }
}

/* Evaluate the current challenge's rules once (one emulated frame's worth).
 * Mirrors the Lua handler-call contract:
 *   - SWITCH matches stop evaluation and, if no switch is pending, schedule
 *     one; they clear the latch and skip the shadow update (the handler
 *     returned before its trailing `state.x = current` lines ran).
 *   - RESET matches schedule/refresh a reset and let evaluation continue.
 *   - SET_LATCH matches set the latch and continue.
 */
static int eval_challenge(void) {
    const struct challenge *c = &challenges[g_cur];
    ram_refresh();

    for (int i = 0; i < c->n_writes; i++) {
        const struct rw *w = &c->writes[i];
        if (g_ram && (size_t)w->addr < g_ram_sz)
            g_ram[w->addr] = w->value;
    }

    int switch_matched = 0;
    for (int ri = 0; ri < MAX_RULES && c->rules[ri].act != ACT_NONE; ri++) {
        const struct rule *r = &c->rules[ri];
        if ((r->flags & FLAG_NEED_LATCH) && !g_latch)
            continue;
        if (!rule_match(r, ri))
            continue;
        switch (r->act) {
        case ACT_SET_LATCH:
            g_latch = 1;
            break;
        case ACT_RESET:
            g_pending_reset = 1;
            g_reset_at = SDL_GetTicks64();
            g_reset_ms = (unsigned)(r->wait_s * 1000.0 + 0.5);
            printf("[engine] %s: reset scheduled in %.3fs\n",
                   c->name, r->wait_s);
            break;
        case ACT_SWITCH:
            switch_matched = 1;
            if (!g_pending_switch) {
                g_pending_switch = 1;
                g_switch_at = SDL_GetTicks64();
                g_switch_ms = (unsigned)(r->wait_s * 1000.0 + 0.5);
                printf("[engine] %s: done - switching in %.3fs\n",
                       c->name, r->wait_s);
            }
            goto done;
        default:
            break;
        }
    }
done:
    if (switch_matched)
        g_latch = 0;
    if (!switch_matched)
        refresh_shadows();
    return switch_matched;
}

/* --- Selection (port of Game.lua's weight policy) -------------------------- */

/* ---------------------------------------------------------------------------
 * Data paths: the challenge table's ROM/savestate entries are resolved
 * relative to the directory holding the executable, so the game_data/ tree is
 * expected next to the binary and the program can be started from anywhere.
 * Absolute paths pass through untouched.
 * ------------------------------------------------------------------------ */
static char g_exe_dir[4096] = "";   /* "" => unknown, use the paths as written */

static void resolve_exe_dir(void) {
#if defined(__APPLE__)
    uint32_t sz = sizeof(g_exe_dir);
    if (_NSGetExecutablePath(g_exe_dir, &sz) == 0) {
        char *slash = strrchr(g_exe_dir, '/');
        if (slash) { *slash = '\0'; return; }
    }
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", g_exe_dir, sizeof(g_exe_dir) - 1);
    if (n > 0) {
        g_exe_dir[n] = '\0';
        char *slash = strrchr(g_exe_dir, '/');
        if (slash) { *slash = '\0'; return; }
    }
#endif
    g_exe_dir[0] = '\0';
}

/* Resolve a table-relative path against the executable's directory; `buf` must
 * outlive the returned pointer. */
static const char *data_path(const char *rel, char *buf, size_t bufsz) {
    if (!rel || !rel[0])
        return rel;
    if (rel[0] == '/' || !g_exe_dir[0])
        return rel;
    snprintf(buf, bufsz, "%s/%s", g_exe_dir, rel);
    return buf;
}

static int file_exists(const char *p) {
    if (!p)
        return 0;
    FILE *f = fopen(p, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Cores. A challenge's state file names the core that produced it, and that is
 * the core its system needs, so the core is a property of the challenge - not
 * of the invocation. Each core is looked up beside the executable and in the
 * data root above it, exactly like game_data/.
 * ------------------------------------------------------------------------ */
struct core_map { const char *suffix; const char *files[3]; };
#if defined(__APPLE__)
/* macOS builds link Mach-O dylibs: same lookup keys, different filenames. */
static const struct core_map g_cores[] = {
    { ".s9x",                     { "snes9x_libretro.dylib", NULL, NULL } },
    { ".libretro-quicknes.state", { "quicknes_libretro.dylib", NULL, NULL } },
    { ".libretro-gambatte.state", { "gambatte_libretro.dylib", NULL, NULL } },
    { ".libretro-gpgx.state",     { "genesis_plus_gx_libretro.dylib", NULL, NULL } },
    { NULL, { NULL, NULL, NULL } }
};
#else
static const struct core_map g_cores[] = {
    { ".s9x",                     { "snes9x_libretro.so", "snes9x_libretro_v12.so", NULL } },
    { ".libretro-quicknes.state", { "quicknes_libretro.so", NULL, NULL } },
    { ".libretro-gambatte.state", { "gambatte_libretro.so", NULL, NULL } },
    { ".libretro-gpgx.state",     { "genesis_plus_gx_libretro.so", NULL, NULL } },
    { NULL, { NULL, NULL, NULL } }
};
#endif

/* Core for a challenge state path, or NULL when the system has no core present.
 * `buf` must outlive the returned pointer. */
static const char *core_for_state(const char *state, char *buf, size_t bufsz) {
    if (!state || !state[0] || !g_exe_dir[0])
        return NULL;
    const struct core_map *m = NULL;
    size_t nl = strlen(state);
    for (int i = 0; g_cores[i].suffix; i++) {
        size_t sl = strlen(g_cores[i].suffix);
        if (nl >= sl && strcmp(state + nl - sl, g_cores[i].suffix) == 0) { m = &g_cores[i]; break; }
    }
    if (!m)
        return NULL;
    char up[4096];
    snprintf(up, sizeof(up), "%s", g_exe_dir);
    char *slash = strrchr(up, '/');
    if (slash && slash != up) *slash = '\0';
    for (int d = 0; d < 2; d++) {
        const char *dir = d ? up : g_exe_dir;
        for (int f = 0; m->files[f]; f++) {
            snprintf(buf, bufsz, "%s/%s", dir, m->files[f]);
            if (file_exists(buf))
                return buf;
        }
    }
    return NULL;
}

static void compute_availability(void) {
    int n_avail = 0;
    for (int i = 0; i < N_CHALLENGES; i++) {
        const struct challenge *c = &challenges[i];
        char rombuf[4096], stbuf[4096], corebuf[4096];
        int rom_ok  = file_exists(data_path(c->rom, rombuf, sizeof(rombuf)));
        int st_ok   = file_exists(data_path(c->state, stbuf, sizeof(stbuf)));
        int core_ok = core_for_state(stbuf, corebuf, sizeof(corebuf)) != NULL;
        g_avail[i] = rom_ok && st_ok && core_ok;
        g_dynw[i] = c->weight > 0.0 ? c->weight : 1.0;
        if (g_avail[i])
            n_avail++;
        printf("  %c [%s] %s%s\n", g_avail[i] ? ' ' : 'x',
               c->slug, c->name, c->interlude ? "  (interlude)" : "");
    }
    printf("  (%d/%d challenges available)\n", n_avail, N_CHALLENGES);
}

/* Per-switch recovery: w = min(orig, w + orig / N). */
static void weights_update(void) {
    double recovery = 1.0 / N_CHALLENGES;
    for (int i = 0; i < N_CHALLENGES; i++) {
        if (!g_avail[i])
            continue;
        double w = g_dynw[i] + challenges[i].weight * recovery;
        g_dynw[i] = w > challenges[i].weight ? challenges[i].weight : w;
    }
}

/* The just-finished challenge drops to orig * penalty. */
static void weights_reduce(int cur) {
    g_dynw[cur] = challenges[cur].weight * PLAYED_PENALTY;
}

static int select_next(int current) {
    uint64_t now = SDL_GetTicks64();

    /* Forced interlude (excluded from the random pool). */
    for (int i = 0; i < N_CHALLENGES; i++) {
        if (!g_avail[i] || !challenges[i].interlude)
            continue;
        if (now - g_last_interlude >= (uint64_t)(INTERLUDE_INTERVAL_S * 1000.0))
            return i;
        break;
    }

    int n_pool = 0;
    for (int i = 0; i < N_CHALLENGES; i++)
        if (g_avail[i] && !challenges[i].interlude)
            n_pool++;
    if (n_pool == 0)
        return -1;

    int next = current;
    for (int attempt = 0; attempt < 8; attempt++) {
        double total = 0;
        for (int i = 0; i < N_CHALLENGES; i++)
            if (g_avail[i] && !challenges[i].interlude)
                total += g_dynw[i];
        if (total <= 0.0)
            return -1;
        double pick = (double)rand() / (double)RAND_MAX * total;
        double cum = 0.0;
        next = current;
        for (int i = 0; i < N_CHALLENGES; i++) {
            if (!g_avail[i] || challenges[i].interlude)
                continue;
            cum += g_dynw[i];
            if (pick <= cum) {
                next = i;
                break;
            }
        }
        if (next != current)
            break;
    }
    if (next == current) {
        /* The weighted draw kept landing on the current challenge; fall
         * back to the next available non-interlude challenge so a switch
         * always changes something. */
        for (int k = 1; k <= N_CHALLENGES; k++) {
            int i = (current + k) % N_CHALLENGES;
            if (g_avail[i] && !challenges[i].interlude) {
                next = i;
                break;
            }
        }
    }
    return next;
}

/* Load challenge i: ROM (swap if different), state, scratch state. Returns 0
 * on failure so the caller can drop the challenge and retry. */
static int load_challenge(int i) {
    const struct challenge *c = &challenges[i];

    char rombuf[4096], stbuf0[4096], corebuf[4096];
    const char *rom = data_path(c->rom, rombuf, sizeof(rombuf));
    const char *statep = data_path(c->state, stbuf0, sizeof(stbuf0));

    /* The challenge's state names its core; swap cores when the system changes. */
    const char *core = core_for_state(statep, corebuf, sizeof(corebuf));
    if (!core) {
        fprintf(stderr, "[engine] no core present for %s (%s)\n", c->name, c->state);
        return 0;
    }
    if (!g_cur_core[0] || strcmp(g_cur_core, core) != 0) {
        if (g_cur_core[0]) {
            core_unload();
            g_game_loaded = 0;
            g_cur_rom[0] = '\0';
        }
        core_load(core);
        printf("[engine] core: %s\n", core);
        snprintf(g_cur_core, sizeof(g_cur_core), "%s", core);
        /* A new core must not inherit the previous one's hw-render request: its
         * callbacks point into the object we just unloaded. */
        memset(&g_video.hw, 0, sizeof(g_video.hw));
        g_video.hw.version_major = 4;
        g_video.hw.version_minor = 5;
        g_video.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
        g_video.hw.context_reset = noop;
        g_video.hw.context_destroy = noop;
    }

    int rom_same = g_game_loaded && rom[0] && strcmp(rom, g_cur_rom) == 0;
    if (rom_same) {
        printf("Same ROM, skipping reload\n");
    } else {
        if (g_game_loaded) {
            g_retro.retro_unload_game();
            g_game_loaded = false;
        }
        if (!core_load_game(rom)) {
            fprintf(stderr, "[engine] cannot load ROM for %s: %s\n",
                    c->slug, rom);
            return 0;
        }
        snprintf(g_cur_rom, sizeof(g_cur_rom), "%s", rom);
        g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
        printf("[engine] ROM loaded: %s\n", rom);
    }

    core_load_state(statep);

    g_cur = i;
    g_latch = 0;
    memset(g_stable, 0, sizeof(g_stable));
    g_pending_reset = 0;
    collect_shadows(c);
    ram_refresh();
    refresh_shadows();

    printf("[engine] challenge: %s (%s) weight=%.2f%s\n",
           c->name, c->slug, c->weight, c->interlude ? " interlude" : "");

    card_begin();
    return 1;
}

/* Reload the current challenge's state (ACT_RESET). Also clears the
 * per-challenge scratch, as Lua's `reset()` clears the whole state table. */
static void reload_current_state(void) {
    const struct challenge *c = &challenges[g_cur];
    char stbuf[4096];
    const char *st = data_path(c->state, stbuf, sizeof(stbuf));
    if (!st)
        return;
    core_load_state(st);
    g_latch = 0;
    memset(g_stable, 0, sizeof(g_stable));
    ram_refresh();
    refresh_shadows();
    card_begin();
}

static void do_switch(void) {
    g_pending_switch = 0;
    const struct challenge *c = &challenges[g_cur];
    if (c->interlude)
        g_last_interlude = SDL_GetTicks64();
    weights_update();
    if (!c->interlude)
        weights_reduce(g_cur);

    for (int attempt = 0; attempt < N_CHALLENGES; attempt++) {
        int next = select_next(g_cur);
        if (next < 0)
            break;
        if (load_challenge(next))
            return;
        g_avail[next] = 0;
        g_dynw[next] = 0.0;
    }
    fprintf(stderr, "[engine] no challenge left to load\n");
}

/* Case-insensitive substring test. */
static int ci_contains(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (!nl)
        return 1;
    for (const char *p = hay; *p; p++)
        if (strncasecmp(p, needle, nl) == 0)
            return 1;
    return 0;
}

/* Pick the challenge named on the command line: exact name first, then a
 * case-insensitive substring of the name, then the slug. -1 if nothing matches. */
static int match_challenge(const char *want) {
    if (!want || !want[0])
        return -1;
    size_t wl = strlen(want);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < N_CHALLENGES; i++) {
            const char *n = challenges[i].name;
            const char *s = challenges[i].slug;
            int hit = 0;
            if (pass == 0)
                hit = n && strlen(n) == wl && strncasecmp(n, want, wl) == 0;
            else if (pass == 1)
                hit = n && ci_contains(n, want);
            else
                hit = s && strcasecmp(s, want) == 0;
            if (hit)
                return i;
        }
    }
    return -1;
}

static void noop() {}

int main(int argc, char *argv[]) {
    /* Optional single argument: the challenge to start with (name or slug, case
     * insensitive). Cores are chosen per challenge, so no core is passed here. */
    const char *want = argc > 1 ? argv[1] : NULL;

    /* Unbuffered stdout: the app is also a headless challenge harness, and
     * buffered engine logs would be lost if the process is killed. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* The challenge data tree (game_data/) is expected next to the executable. */
    resolve_exe_dir();

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0)
        die("Failed to initialize SDL");

    g_video.hw.version_major = 4;
    g_video.hw.version_minor = 5;
    g_video.hw.context_type  = RETRO_HW_CONTEXT_OPENGL_CORE;
    g_video.hw.context_reset   = noop;
    g_video.hw.context_destroy = noop;

    /* Start the challenge cycle: the one named on the command line, otherwise a
     * weighted-random pick; load its core, ROM and savestate. */
    printf("Available challenges:\n");
    compute_availability();

    int start = match_challenge(want);
    if (want && start < 0)
        die("no challenge matches '%s'", want);
    if (start < 0)
        start = select_next(0);
    if (start < 0)
        die("no challenge is loadable: no ROM/state/core present");
    if (!load_challenge(start))
        die("failed to load first challenge: %s", challenges[start].name);

    printf("Controls: F9 save state | T force switch | ESC quit\n");
    printf("The engine picks the next challenge at random (weighted); the\n");
    printf("Super Mario World interlude is forced every %.0fs.\n\n",
           INTERLUDE_INTERVAL_S);

    SDL_Event ev;

    while (running) {
        // Update the game loop timer.
        if (runloop_frame_time.callback) {
            retro_time_t current = cpu_features_get_time_usec();
            retro_time_t delta = current - runloop_frame_time_last;

            if (!runloop_frame_time_last)
                delta = runloop_frame_time.reference;
            runloop_frame_time_last = current;
            runloop_frame_time.callback(delta);
        }

        // Ask the core to emit the audio.
        if (audio_callback.callback) {
            audio_callback.callback();
        }

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = false; break;
            case SDL_KEYDOWN:
                if (!ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_F9)
                    save_state_to_disk();
                if (!ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_T) {
                    g_force_switch = 1;
                    printf("[engine] force switch requested (T)\n");
                }
                break;
            case SDL_WINDOWEVENT:
                switch (ev.window.event) {
                case SDL_WINDOWEVENT_CLOSE: running = false; break;
                case SDL_WINDOWEVENT_RESIZED:
                    resize_cb(ev.window.data1, ev.window.data2);
                    break;
                }
            }
        }

        // Frame-rate limiter: cap to the core's nominal fps so the game runs at
        // real speed. Without this, sdlarch runs hundreds of fps and a loaded
        // state whose scene is short-lived (e.g. a boss fight) is over in ~1s.
        {
            uint64_t now = SDL_GetTicks64();
            if (g_last_frame && g_fps > 0.0) {
                uint64_t target = (uint64_t)(1000.0 / g_fps);
                uint64_t elapsed = now - g_last_frame;
                if (elapsed < target) SDL_Delay((Uint32)(target - elapsed));
            }
            g_last_frame = SDL_GetTicks64();
        }

        // Challenge engine: run the current challenge's rule set each frame.
        // A SWITCH match stops the rule set for the rest of the frame (the
        // Lua handlers' `return` does); RESET / SET_LATCH matches continue.
        (void)eval_challenge();

        // Pending switch/reset: wait out the handler-specified delay while the
        // game keeps running, then load the next challenge / the state again.
        if (g_force_switch || g_pending_switch || g_pending_reset) {
            if (g_force_switch) {
                g_force_switch = 0;
                g_pending_reset = 0;
                do_switch();
            } else {
                uint64_t now = SDL_GetTicks64();
                int switch_due = g_pending_switch &&
                                 (now - g_switch_at) >= (uint64_t)g_switch_ms;
                int reset_due  = g_pending_reset &&
                                 (now - g_reset_at) >= (uint64_t)g_reset_ms;
                if (switch_due)
                    do_switch();
                else if (reset_due)
                    reload_current_state();
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
		g_retro.retro_run();
	}

	core_unload();
	audio_deinit();
	video_deinit();

    if (g_vars) {
        for (const struct retro_variable *v = g_vars; v->key; ++v) {
            free((char*)v->key);
            free((char*)v->value);
        }
        free(g_vars);
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
