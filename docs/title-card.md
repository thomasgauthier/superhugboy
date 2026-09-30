# Title card (in-game overlay)

Status: **specified, not implemented.**

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
| Both | — | Solid `rgb(0,0,0)`. Text horizontally centred, vertically centred in its line box. |

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
| Cell size | Largest of `{8x8, 6x8}` at which every line fits the plate width |
| Overflow | If the widest line still does not fit at `6x8`, wrap it onto an additional line inside its plate |
| Case | Uppercase |
| Colour | Light, high contrast on black |
| Coordinate space | Core framebuffer space, drawn through the same transform as the video quad |
| Character set | ASCII `0x20`-`0x7E` (see *Transliteration*) |

Fit rule: `usable_columns = floor((framebuffer_width - 2 * pad_x) / cell_width)`, `pad_x = 8`.

| Core frame | 8x8 | 6x8 |
|---|---|---|
| NES / SNES 256 px | 30 cols | 40 cols |
| GBA 240 px | 28 cols | 37 cols |
| GB 160 px | 18 cols | 24 cols |
| Genesis / Neo Geo 320 px | 38 cols | 50 cols |

A Game Boy frame cannot fit a metadata line at `8x8`; `6x8` is the working size on narrow frames.

### Transliteration

The bitmap atlas is ASCII-only, so:

- Platform names are abbreviated: `NES`, `SNES`, `GB`, `GBA`, `GEN`, `NEOGEO`.
- Separator between metadata fields is an ASCII hyphen, `1990 - NINTENDO - SNES`. U+00B7 (`·`) is
  not available.
- Upstream `"Choose a pokémon!"` transliterates to `"CHOOSE A POKEMON!"` rather than adding `É` to
  the atlas.

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
- `games.csv` is not safe to split on commas naively — quoted fields contain commas
  (`"Pokemon - Red Version (USA, Europe) (SGB Enhanced).zip"`). Parse as CSV.

Open: whether year/publisher are new `games.csv` columns or a separate metadata table. Either way
they are authored once, by ROM.

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
6. Across an entire run: the game image never shifts, resizes, or gains/loses a border. The only
   thing that changes is pixels drawn on top.
7. A Game Boy challenge renders both plates without overflow at `6x8`.

## Non-goals

Letterboxing the game itself · per-challenge generated artwork · fade/slide reveals · pause-on-card ·
end-of-run scorecard · stage numbering · input-gated dismissal · drawing outside the framebuffer.

## Open questions

1. Where the port's level qualifier goes. `struct challenge.name` is `"Castlevania - level 1"`; the
   game half belongs to the top plate, but the level half has no home in the two-plate layout.
   Options: a second line in the bottom plate, appended to the top plate's title line, or dropped.
2. `challenge_text_pos` — confirm it is dropped in favour of the fixed layout rather than honoured
   as an override.
3. Year/publisher storage: `games.csv` columns or a separate ROM-keyed table.
