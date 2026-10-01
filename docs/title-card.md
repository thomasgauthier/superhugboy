# Title card (in-game overlay)

Status: **implemented** in `sdlarch.c` (branch `hugboycpp`). The card module sits
with the challenge table, the pass is drawn at the end of `video_refresh()`, and
the timer is armed from `load_challenge()` and `reload_current_state()`.

## What it is

When hugboy drops you into a challenge, two black plates appear over the live gameplay — one
pinned to the top of the frame, one pinned to the bottom — carrying the game's identity and the
challenge objective. Five seconds later they are gone. The game runs underneath the whole time.

It is an overlay, not a cutscene. Inputs are never blocked, the simulation is never paused, and
nothing on screen resizes.

## Why

Upstream superhugboy's Lua handlers carry `challenge_text` (`"Free the princess!"`, `"Escape!"`,
`"Make 1 line!"`) and draw it every frame for five seconds. The C port lost the field in
translation. This restores it and adds a top plate with game metadata the port does not currently
display at all.

## Layout

Two plates, spanning the full width of the game image (not the window — on a pillarboxed window the
plates must align with the quad's edges, not float over the side bars).

The frontend fits each core into a fixed 960×720 window, or the desktop in fullscreen.
The card uses that fitted game viewport, including its offset; it never fills the surrounding
letterbox or pillarbox space.

```
+--------------------------------------------------+  <- top plate, pinned to y = 0
|            SUPER MARIO WORLD                     |     line 1: display title
|         1990 - NINTENDO - SNES                   |     line 2: metadata
+--------------------------------------------------+
|                                                  |
|                  (live gameplay)                 |
|                                                  |
+--------------------------------------------------+
|              DEFEAT THE BOSS!                    |  <- bottom plate, pinned to y = h
+--------------------------------------------------+
```

| Plate | Anchored | Contents |
|---|---|---|
| Top | `y = 0`, full framebuffer width | Line 1: game display title. Line 2: year - publisher - platform. |
| Bottom | `y = framebuffer_height`, full framebuffer width | Challenge text (upstream `challenge_text`). |
| Both | — | Solid `rgb(0,0,0)`. Text centred horizontally and vertically inside its bar. |

Plate height = `lines * cell_height + 2 * pad_y`. Plate colour is pure black; opacity is one
constant (opaque recommended — it is what makes the plates read as letterboxing).

## Behaviour

| Property | Requirement |
|---|---|
| Appears on | `load_challenge()` and `reload_current_state()` — cold start, every switch, every `ACT_RESET` |
| Duration | 5 s (5000 ms), wall clock; same clock as `INTERLUDE_INTERVAL_S` |
| Expiry | Hard cut. Both plates appear and vanish together. No fade, slide, typewriter, or minimum display time |
| Dismissable | No. No key, no input, no click |
| Pauses game | No. Sim advances; rules evaluate from frame 0 |
| Blocks input | No |
| Changes geometry | No. Game quad transform untouched; no letterbox added or removed |
| Re-arms on | `T` force-switch, `ACT_RESET` reload, ordinary switch |

A challenge whose rule fires at `0s` may show its card for a single frame before the next
challenge's card replaces it. That is upstream behaviour, not a defect. It is also why the plates
must be readable in one frame and why no reveal animation is permitted.

## Typography

| Property | Requirement |
|---|---|
| Typeface | Embedded monospace bitmap, integer-scaled, no antialiasing, no letterspacing |
| Bar size | The bars are `CARD_SCALE_MULT` (3) times the glyph scale the framebuffer maps to 1:1, so they thicken with the card |
| Top text | Draws at half again the framebuffer's own 1:1 scale and sits centred, both ways, inside its bar — the cartridge's identity is a caption |
| Bottom text | Draws at 80% of the card's scale — the objective is the headline, trimmed just enough to sit one step under the bars |
| Cell size | Fixed 8×8 SDL2_gfx bitmap-font cell |
| Overflow | The scale steps down rather than wrap; wrapping remains the floor if no scale can fit the words |
| Case | Uppercase |
| Colour | Top plate white; bottom plate yellow (`rgb(255,255,0)`). See *Plate colours*. |
| Coordinate space | Drawable pixels local to the fitted game viewport; plates and text share the video viewport |
| Character set | ASCII `0x20`-`0x7E` (see *Transliteration*) |

Fit rule, measured in **game-viewport pixels**, and **per plate**
because each plate draws at its own scale:

```
usable_columns(plate) = (game_viewport_width - 2 * pad_x * plate_scale) / (cell_width * plate_scale)
```

`pad_x` is 8. The card scale starts at `base * CARD_SCALE_MULT`, where `base` is the largest integer
scale at which the core's framebuffer maps 1:1 into the fitted game viewport, and steps down to 1.
A scale is accepted when every line fits whole and both bars fit inside that viewport.
At scale 1, word wrapping is allowed if no text is truncated. If even that layout cannot fit,
the card is not drawn at that viewport size.

At normal gameplay window sizes, authored lines stay whole. The stock SDL2_gfx 8×8 font is wider
than the former custom 6×8 option, so some challenges use smaller card scales to keep their text
inside the plates.

### Wrapping

Wrapping is a floor, not the normal case. It is accepted only at scale 1 after all larger
whole-line layouts fail. Lines break between words, a separator left dangling at a break is
dropped, and a single short word stranded on the last line pulls the previous line's last word
down with it (`STREETS OF RAGE` / `2` becomes `STREETS OF` / `RAGE 2`). A word that cannot fit at
scale 1 makes the layout invalid rather than being clipped.

### Plate colours

The cartridge's identity reads white; the objective reads yellow, because the objective
is the one line the player acts on and the accent makes it the thing the eye lands on.

Both are pure multipliers on the white text texture (`CARD_TITLE_COLOR`, `CARD_TEXT_COLOR`
in `sdlarch.c`), applied through the shader's `u_tint`. Changing either — or making the
whole card one colour — is a single three-float constant; the font and layout do not change.

The game quad shares that program, so the tint is set to white both at shader setup and
after the card pass; the core's pixels are never tinted.

### Transliteration

The card accepts printable ASCII only, so:

- Platform names are abbreviated: `NES`, `SNES`, `GB`, `GBA`, `GEN`, `NEOGEO`.
- Separator between metadata fields is an ASCII hyphen, `1990 - NINTENDO - SNES`. U+00B7 (`·`) is
  not available.
- Upstream `"Choose a pokémon!"` transliterates to `"CHOOSE A POKEMON!"`.

## Data

### Challenge text (from upstream)

| Field | Required | Meaning |
|---|---|---|
| `challenge_text` | Yes (upstream schema, `Game.lua:106-132`) | Objective line. Empty string is legal. |
| `challenge_text_pos` | No | `{x, y}` override, default `{10, 10}`. `"center"` anchor. |

Source of truth: upstream `challenges/*.lua` on `main` — 40 rows across 23 files.

**Decision: the two-plate layout supersedes `challenge_text_pos`.** Upstream hand-tuned a position
per row because it had no plate to sit on — the corpus uses `{128,96}`, `{128,84}`, `{128,64}`,
`{128,72}`, `{80,32}`, `{30,56}`. With fixed plates the text goes in the bottom plate for every row
and the field is copied from upstream for fidelity but not consulted. This is a deliberate
divergence: one consistent presentation instead of forty hand-tuned coordinates.

`""` still means **no card at all** — no plates, no text. That covers `MarioWorldInterlude` and
Zelda1's first row, both of which are meant to be seamless.

### Game metadata (new)

Year and publisher do not exist anywhere in the repo. `game_data/games.csv` has:

```
Filename, CRC32, Identified ROM, Platform, Region, target_libretro_core,
state_conversion_playbook_exists
```

`Platform` is present (`NES`, `SNES`, `Game Boy`, `Game Boy Advance`, `Sega Genesis`, `Neo Geo`).
`Year` and `Publisher` are **not**, and are not derivable from the ROM at runtime.

Requirements:

- Metadata is keyed by **ROM**, not by challenge. ~36 challenges share ~15 ROMs; duplicating year
  and publisher per challenge row duplicates data that can only drift.
- `Identified ROM` is a No-Intro dat name (`"Super Mario World (USA)"`). It is not a display title —
  region and revision parentheticals must be stripped, and long multi-game entries shortened
  (`"Super Mario All-Stars + Super Mario World (USA)"` → `SUPER MARIO WORLD`).
- Display titles should be authored to fit the narrowest plate: **<= 24 characters**, which every
  current title satisfies (`CASTLEVANIA`, `DONKEY KONG COUNTRY`, `SONIC THE HEDGEHOG`,
  `POKEMON RED`, `SUPER METROID`, `METAL SLUG 3`).
- `games.csv` is not used for this. It would need real CSV parsing (quoted fields contain
  commas: `"Pokemon - Red Version (USA, Europe) (SGB Enhanced).zip"`) and it carries no
  year or publisher to parse in the first place.

Storage: an in-code `static const struct game_meta` per cartridge, referenced by pointer
from the challenge rows — authored once, by ROM. See *Open questions*.

## Edge cases

- **Blank text** → no card at all. Do not substitute the display title, do not draw an empty plate.
- **Long line** → grows the plate by one line; must not overflow the framebuffer.
- **Short-lived challenge** → card may flash for one frame. Expected.
- **Hardware-render cores** → the plates are a separate draw pass. Never write into the core's
  framebuffer pixels.
- **Blend state** → enable alpha blending for the overlay pass, restore afterwards so the game quad
  pass is unaffected.
- **Headless / no audio device** → card is unaffected; still drawn, still expires.

## Acceptance criteria

1. Launch a named challenge → both plates visible over live gameplay on the first frame, carrying
   title, metadata and objective; gone after 5 s.
2. Rules still fire while the plates are up — a `0s`-switch challenge moves on immediately, with no
   delay from the card.
3. `T` → new challenge, new plates, immediately; no trace of the previous card.
4. `ACT_RESET` path → card re-armed on reload.
5. A blank-text challenge → no plates, nothing drawn.
6. Showing or hiding the card never changes the game viewport. Only core geometry or fullscreen
   changes alter the fitted image; the windowed size remains 960×720 across the run.
7. Bars start at `CARD_SCALE_MULT` (3) times their 1× thickness, stepping down as needed for
   the 8×8 font. The top caption uses half again the 1:1 scale (capped by card scale);
   the objective uses 80% of card scale, rounded to an integer. Both are centred.
   Whole lines are preferred; wrapping is only the scale-1 fallback.
8. At every scale the search reaches, nothing is cut: no word clipped, no line past the window
   edge, no plate taller than the window, no plate covering the other.

## Non-goals

Game viewport sizing (handled by the frontend) · per-challenge generated artwork · fade/slide reveals · pause-on-card ·
end-of-run scorecard · stage numbering · input-gated dismissal · drawing outside the framebuffer.

## Open questions

1. **Where the port's level qualifier goes — resolved: nowhere.** `struct challenge.name`
   is `"Castlevania - level 1"`, but the top plate carries the ROM's *display title* and
   the bottom plate carries the upstream objective; the two-plate layout has no third
   slot, and putting "level 1" in either would duplicate or muddy what is already there.
   `name` is unchanged and still used for the engine log and command-line matching.
2. **`challenge_text_pos` — resolved: not ported.** The rows carry `challenge_text`
   only; the corpus' hand-tuned coordinates (`{128,96}`, `{80,32}`, …) were not brought
   over, because the fixed plates supersede them.
3. **Year/publisher storage — resolved: in code.** One `static const struct game_meta`
   per cartridge, referenced from each challenge row by pointer, so a ROM's year and
   publisher exist exactly once and cannot drift between the challenges that share it.
   `games.csv` is not involved, so its quoted-comma rows never need parsing.

## Implementation notes

- **SDL2_gfx rendering.** `GFX_stringRGBA()` draws complete lines using the vendored stock
  8×8 font into a small transparent software-rendered surface. OpenGL composites one
  quad per line at the plate's integer text scale. There is no private font, glyph atlas,
  or per-character vertex generation.
- **Caching.** Layout, text texture and line vertices rebuild on challenge load, reset,
  fullscreen viewport size or framebuffer dimension changes, not every frame.
- **Font cache lifetime.** SDL2_gfx caches glyph textures globally. Reset that cache before
  destroying the temporary software renderer.
- **State restore.** The card pass saves and restores the clear colour and puts the game
  quad's matrix and tint back into `u_mvp` and `u_tint`, because the two passes share one
  program.
- **Pre-existing engine bug, not part of this feature.** `g_pending_reset` is never
  cleared after the reset fires, so `reload_current_state()` runs once per frame from
  then on — the state reloads ~50x/s and the game appears frozen. Since the card is armed
  on each reload, a challenge that resets keeps its card on screen indefinitely. The
  card's own behaviour is per spec; the loop is in the reset path.

