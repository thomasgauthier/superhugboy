# hugboy.cpp

C++ port of [superhugboy](https://github.com/thomasgauthier/superhugboy)'s challenge mode: a small
libretro frontend that plays a weighted rotation of "challenges".

A challenge is a ROM, a savestate and a few RAM predicates — the port of one upstream Lua handler.
Each frame the engine evaluates the current challenge's rules; the first rule that matches schedules
the next challenge's savestate after the number of seconds it returns (`nil` = keep playing,
`0` = switch at once, `reset()` = reload the current state). The Super Mario World interlude is
forced every 180 seconds, and the engine keeps a dynamic weight per challenge so recently played
ones fall back in the rotation.

The 36 challenges and their rule tables are the `challenges[]` table in `sdlarch.c`.

## Build

Requires a C compiler, SDL2 development headers and `pkg-config`.
[SDL2_gfx](https://github.com/giroletm/SDL2_gfx) primitives, its stock font and
rotozoom dependency are vendored unmodified under `vendor/SDL2_gfx/`
(revision `d985671e7ff715ff349f48295a9a6377d4927c35`); no separate gfx install is needed.

```
make
```

## Data

Game data lives in `game_data/` **next to the executable** (not next to your shell's cwd):

```
game_data/
  ROMS/        the carts, as raw dumps — the cores do not read archives
  converted/   this port's converted savestates, not BizHawk .State files
```

`previews/` next to the executable contains the bundled opening-frame BMPs and geometry
metadata. Missing previews are generated there automatically.

## Run

```
./sdlarch [challenge]
```

With no argument the engine starts a weighted-random challenge; with an argument it starts the named
one (exact name first, then a case-insensitive substring of the name, then the slug). A name that
matches nothing is an error rather than a silent fallback.

Cores are **not** passed on the command line. Each savestate names the core its system needs, and
the engine loads that core, swapping cores (and reloading the ROM) when the rotation moves to
another system:

| state file | core, looked for beside the executable or in the directory above it |
|---|---|
| `*.s9x` | `snes9x_libretro.so`, then `snes9x_libretro_v12.so` |
| `*.libretro-quicknes.state` | `quicknes_libretro.so` |
| `*.libretro-gambatte.state` | `gambatte_libretro.so` |
| `*.libretro-gpgx.state` | `genesis_plus_gx_libretro.so` |

macOS builds look for the same names with a `.dylib` extension instead.

A challenge is only offered when its ROM, its savestate **and** its core are all present, so the
count printed at startup is the number of playable challenges — and the ones marked `x` are missing
data, not broken logic.

## Display

Every core uses the same fixed, non-resizable **960×720** window. Games are centered and
scaled to fit without cropping or stretching, using the core's aspect ratio (or the frame's
width/height ratio when the core supplies none). Unused space is black. Core switches and
runtime geometry changes never resize the window.

`F` toggles desktop fullscreen without changing the monitor's resolution. Leaving fullscreen
restores the 960×720 window; switching cores preserves fullscreen. Title cards span the
window, with bar and text sizes shared across all cores and challenges.

## Challenge reel

Every entry (cold start, ordinary switch, `T`/`Y`, or rule-driven reset) rolls through cached
opening frames and their matching title cards. The existing weighted shuffle reserves the
winner first; up to 20 other available non-interlude challenges are sampled with the same
weights, without replacement. Preview picks never alter recency weights. The interlude
remains outside the random pool and can still be selected as the actual winner.

The **2.5-second** reel slows along one continuous curve: every contender takes longer than
the previous one, with no separate fast/slow phases or intermediate stops. With 20 previews,
the last four take about **210, 300, 430 and 640 ms**, keeping several possibilities in play.
Tune `ROLL_CURVE_STRENGTH` in `sdlarch.c`; higher values give later contenders more time.
The winner settles to rest, then holds for **250 ms** with a quiet landing bell.
Gameplay, rules and core audio are stopped during this theatre; `F` and `ESC` still work.
The winner's real savestate is loaded once at landing, and its title-card timer starts again
when live play resumes. Blank-text challenges still have no title card.

To compare styles, change this definition in `sdlarch.c` and run `make`:

```c
#define ROLL_STYLE ROLL_SCROLL  /* vertical reel; default */
// #define ROLL_STYLE ROLL_CUT  /* hard-cut succession */
// #define ROLL_STYLE ROLL_SLOT_MACHINE /* late detents and spring landing */
```

`ROLL_SLOT_MACHINE` adds progressively stronger, slightly irregular detents to the final
contenders, four quiet mechanical thunks, and a small overshoot/rebound during the 250 ms
landing hold. The screenshot and its title card move together; no extra gameplay runs.
`T` always forces a smooth scrolling switch and `Y` forces hard cuts, independently of the
default style above. Cold starts, automatic switches and resets use `ROLL_STYLE`.

Missing or stale previews are captured before the run using the real ROM/core/savestate,
with neutral input, muted audio and no rule evaluation. Later launches reuse them. The cache
is keyed by savestate (slugs are shared by several levels); newer ROM/state/core mtimes
invalidate it. Delete `previews/` to regenerate everything. If the cache directory
is unwritable, previews still work in memory for that run.

The small presentation regression check is runnable with:

```sh
make
cc $(pkg-config --cflags sdl2) tests/title-card.c build/glad.o \
  build/vendor/SDL2_gfx/*.o $(pkg-config --libs sdl2) -lm -o /tmp/title-card-test
/tmp/title-card-test
```

## Controls

`F` toggles fullscreen · `F9` writes the current state · `T` forces a scrolling switch ·
`Y` forces a hard-cut switch · `ESC` quits.

If no audio device can be opened (headless hosts, `xvfb`), the port reports it and runs silent
instead of refusing to start.

The binary is still named `sdlarch`, from the frontend it grew out of.
