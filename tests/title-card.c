/* Run: make && cc $(pkg-config --cflags sdl2) tests/title-card.c
 * build/glad.o build/vendor/SDL2_gfx/*.o $(pkg-config --libs sdl2) -lm
 * -o /tmp/title-card-test && /tmp/title-card-test */
#define main frontend_main
#include "../sdlarch.c"
#undef main
#include <assert.h>

int main(void) {
    const int sizes[][2] = {{960,720}, {1280,1024}, {1920,1080}, {3840,2160}};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        struct card_layout first = {0};
        for (size_t i = 0; i < sizeof challenges / sizeof challenges[0]; i++) {
            const struct challenge *c = &challenges[i];
            if (!c->game || !c->text || !c->text[0]) continue;
            struct card_layout L;
            g_video.clip_w = 160 + i * 16;
            g_video.clip_h = 144 + i * 16;
            card_window_layout(sizes[s][0], sizes[s][1], c, &L);
            assert(L.ok && L.whole);
            if (!first.ok) first = L;
            assert(L.top_h == first.top_h && L.bot_h == first.bot_h);
            assert(L.title_scale == first.title_scale && L.text_scale == first.text_scale);
        }
        printf("%dx%d: bars %d/%d, text scales %d/%d: PASS\n",
               sizes[s][0], sizes[s][1], first.top_h, first.bot_h,
               first.title_scale, first.text_scale);
    }
    assert(card_slide_offset(0, 144) == 0);
    assert(card_slide_offset(CARD_MS, 144) == 0);
    assert(card_slide_offset(CARD_MS + CARD_SLIDE_MS / 2, 144) == 72);
    assert(card_slide_offset(CARD_MS + CARD_SLIDE_MS, 144) == 144);
    int previous = 0;
    for (uint64_t t = 0; t <= CARD_MS + CARD_SLIDE_MS + 100; t++) {
        int offset = card_slide_offset(t, 144);
        assert(offset >= previous && offset <= 144);
        previous = offset;
    }
    puts("Slide hold, midpoint, completion and monotonicity: PASS");

    /* Preview draws must exclude the winner and earlier picks, and leave the
     * real weight/availability arrays untouched. Test small and exhausted pools. */
    for (int available = 1; available <= N_CHALLENGES; available++) {
        int enabled = 0, winner = -1;
        for (int i = 0; i < N_CHALLENGES; i++) {
            g_avail[i] = !challenges[i].interlude && enabled < available;
            if (g_avail[i]) { enabled++; winner = i; }
            g_dynw[i] = (i + 1) * 0.125;
        }
        int avail_before[N_CHALLENGES];
        double weights_before[N_CHALLENGES];
        memcpy(avail_before, g_avail, sizeof g_avail);
        memcpy(weights_before, g_dynw, sizeof g_dynw);
        int sequence[ROLL_PREVIEWS + 1], seen[N_CHALLENGES] = {0};
        int count = roll_sequence(winner, sequence);
        assert(count == (enabled > ROLL_PREVIEWS ? ROLL_PREVIEWS + 1 : enabled));
        assert(sequence[count - 1] == winner);
        for (int p = 0; p < count; p++) {
            assert(g_avail[sequence[p]] && !seen[sequence[p]]);
            seen[sequence[p]] = 1;
            if (p < count - 1) assert(sequence[p] != winner);
        }
        assert(memcmp(avail_before, g_avail, sizeof g_avail) == 0);
        assert(memcmp(weights_before, g_dynw, sizeof g_dynw) == 0);
        int previews = count - 1;
        assert(roll_position(0, previews) == 0);
        assert(roll_position(ROLL_MS, previews) == previews);
        assert(roll_position(ROLL_MS + 100, previews) == previews);
        double previous_speed = previews;
        for (uint64_t t = 1; t <= ROLL_MS; t++) {
            double speed = roll_position(t, previews) - roll_position(t - 1, previews);
            assert(speed >= 0 && speed <= previous_speed + 1e-12);
            if (roll_position(t, previews) < previews - 1)
                assert(speed > 0); /* Never stop on an intermediate preview. */
            previous_speed = speed;
        }
    }
    /* Every 100 ms interval is slower than the previous one: no slow cruise,
     * no knee where braking suddenly finishes or starts again. */
    double last_distance = ROLL_PREVIEWS;
    for (uint64_t t = 100; t <= ROLL_MS; t += 100) {
        double distance = roll_position(t, ROLL_PREVIEWS) -
                          roll_position(t - 100, ROLL_PREVIEWS);
        assert(distance > 0 && distance < last_distance - 1e-6);
        last_distance = distance;
    }
    /* Several late candidates get meaningful time, not just the winner. */
    uint64_t crossed_at = 0, last_dwell = 0;
    int crossed = 0;
    for (uint64_t t = 1; t <= ROLL_MS; t++) {
        if (roll_position(t, ROLL_PREVIEWS) < crossed + 1) continue;
        uint64_t dwell = t - crossed_at;
        assert(dwell >= last_dwell && dwell >= 16);
        if (crossed >= ROLL_PREVIEWS - 4) assert(dwell >= 200 && dwell <= 700);
        crossed_at = t;
        last_dwell = dwell;
        crossed++;
    }
    assert(crossed == ROLL_PREVIEWS);
    puts("Reel exclusions, recency isolation and uninterrupted slowdown: PASS");
    assert(roll_slot_position(1000, 0) == 0);
    for (int previews = 1; previews <= ROLL_PREVIEWS; previews++) {
        assert(roll_slot_position(0, previews) == 0);
        assert(roll_slot_position(ROLL_MS, previews) == previews);
        assert(roll_slot_position(ROLL_MS + ROLL_PAUSE_MS, previews) == previews);
        assert(roll_slot_position(ROLL_MS + ROLL_PAUSE_MS / 6, previews) > previews);
        assert(roll_slot_position(ROLL_MS + ROLL_PAUSE_MS / 2, previews) < previews);
        double previous = 0;
        for (uint64_t t = 0; t <= ROLL_MS + ROLL_PAUSE_MS; t++) {
            double p = roll_slot_position(t, previews);
            assert(isfinite(p) && p >= 0 && p <= previews + 0.08);
            if (t < ROLL_MS) assert(p >= previous - 1e-12 && p < previews);
            assert((int)p <= previews); /* Safe even during the overshoot. */
            previous = p;
        }
    }
    puts("Slot detents, overshoot, rebound, settled endpoint and index bounds: PASS");
    for (int i = 0; i < N_CHALLENGES; i++) {
        char bmp[4096], info[4096];
        preview_paths(i, bmp, info);
        for (int j = 0; j < i; j++) {
            if (!strcmp(challenges[i].state, challenges[j].state)) continue;
            char other[4096], other_info[4096];
            preview_paths(j, other, other_info);
            assert(strcmp(bmp, other) && strcmp(info, other_info));
        }
    }
    puts("Distinct savestates have distinct preview cache keys: PASS");
    return 0;
}
