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

## Controls

`F9` writes the current state · `T` forces the next challenge · `ESC` quits.

If no audio device can be opened (headless hosts, `xvfb`), the port reports it and runs silent
instead of refusing to start.

The binary is still named `sdlarch`, from the frontend it grew out of.
