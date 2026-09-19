# Alley Cat — C/SDL2 Port: Progress Log

Source: mechanical reconstruction of the original 1984 Alley Cat (IBM/Synapse)
DOS executable, from `alleycat-disassembly-main` (byte-identical NASM
reconstruction, verified against `cat.exe` per that repo's own PROGRESS.md).
This document tracks the C/SDL2 port built on top of it — what's done, what's
verified, what's still open, and the reasoning/evidence behind each finding,
so we can pick this back up without re-deriving it from scratch.

**Personal/educational use only** — this reproduces the original's code and
graphics assets, so it is not meant to be published or redistributed.

---

## 0. Current focus / todo / blockers

**Current focus:** `alley.asm` is ported (§6f), so the background
save/restore pipeline the whole alley scene rests on is finally real and the
screen is no longer wiped every frame. Next up is `throw.asm` — which is
what §17 item (h) is *actually* blocked on (see the correction below).

- [x] `sound.asm` — full port, wired into every previously-stubbed call site (§6e)
- [x] PC speaker / PIT channel 2 emulation + SDL2 audio backend (§6e)
- [x] `init_sound` ported for real (it was never a sound.asm function — §6e finding 2)
- [x] `save_alley_buffer`/`restore_alley_buffer`/`draw_alley_foreground` for real (§6f) — inert stubs since §5f/§5h, in four different files
- [x] `spawn_window_event`, `enter_building`, `handle_cat_death` (§6f)
- [ ] `throw.asm` — maintains `current_floor`/`window_column`; **the real blocker for §17 item (h)**
- [ ] `ui.asm` — `window_open_state` toggling, the other half of item (h)
- [ ] §17 item (f): level-2 collectibles + bg tiles (unblocked since §6e)
- [ ] `update_viewport` (alley.asm) — needs the DS 0x000e sprite scratch area modeled (§6f)
- [ ] `render_sprites` (lives in sound.asm; a decorative-sprite drawing routine — §6f corrects §6e about this)

**Latest blockers/discoveries:** item (h) was never blocked on
`spawn_window_event` at all — two different things called "the window state
machine" had been conflated. See §6f's corrections. Earlier: `sound_enabled`
was typed `bool` in this port but is a 0xFF/0x00 BYTE in the original, which
would have silenced two effects permanently (§6e finding 1).

---

## 1. Status summary

| Layer | Status |
|---|---|
| CGA framebuffer emulation (`cga.c`) | **Ported & verified** |
| Input (`input.c`) | Ported (scancode mapping recovered from `cat.asm`'s key table) |
| Video/SDL2 presentation (`video.c`) | Ported, builds & runs |
| RNG (`cga_random`) | Ported (16-bit LFSR) |
| Sprite data extraction (mechanical, all `data/*.asm`) | **Done**, byte-exact |
| Sprite data extraction (semantically correct dims — cat walk cycles) | **Done & verified** for 2 of many animation sets |
| Frame-selection logic (`game_loop.asm`) | **Decoded, ported (`select_cat_sprite`), and unit-tested — see §5b** |
| Real sprite rendering wired into `main.c` | **Done** — see §5c. Cat responds to arrow keys, shows real walk-cycle/turn/climb poses through the verified `blit_masked` black-silhouette path |
| Real cat movement physics (momentum ramp, scroll, dive depth) | Ported & verified, but **mislabeled — see §5g: this is actually level-2 "drowning" physics, not general movement.** |
| **Real general alley-movement path** (`update_walk_frame`, `update_alley_movement`) | **Done, verified — see §5h.** Climbing/window-transition pose selection ported (§5m), and now gated by the REAL level geometry trigger for levels 1-6 (§5n) — levels 0/7 still use the input-driven fallback. |
| Scene initialization (`setup_alley`/`setup_level`) | **Done, verified — see §5f** |
| Sprite-category verification (enemy/object/title/result/extralife/foreground) | **All 6 categories checked — see §5i/§5k/§5l.** `extralife_sprites.asm`, most of `title_sprites.asm`, and now all of `object_sprites.asm` fully verified/resolved (the small 28-byte item left open in §5l for `collision_sprite` is now fully resolved too — see §5s: it's 3 legitimate hit-reaction sprites, not padding); `result_sprites.asm`/`foreground_sprites.asm` confirmed to contain no actual sprite bitmaps. |
| `blit_transparent` (color-keyed, black=transparent) | **Fixed & verified — see §5j** |
| Real geometry-gated climb trigger (`level_objects.asm`/`level_physics.asm`) | **Levels 1-6 done, verified — see §5n.** Levels 0/7 need the unported window-state machine. |
| Dog enemy AI (`enemy.asm`) | **State machine done & verified (§5o); real sprite extraction + rendering also done (§5p); main-loop wiring bug fixed (§5q).** Dog is now visible, animated, and correctly triggerable from both call sites. |
| `level_objects.asm` | **Fully surveyed (§5q) — 6 mostly-independent subsystems identified, none yet ported.** Level-2 prop physics (needs sprite extraction), level-2 collectibles (needs sound.asm), level-2 bg tiles, `check_stairs_collision` (confirmed blocked on window state machine, §5r), the level-7 victory/love-scene epilogue (~600 lines, self-contained), and footprint decals (needs new sprite extraction). |
| Master game loop (`entry.asm`) | **Fully read & documented, §5r.** Confirms `update_animation` (already ported) is correctly the shared per-level dispatch, and identifies the alley scene (level 0)'s extra per-frame systems — **now correctly characterized and partly ported, see §5s** (this replaces §5r's "jump/gravity/obstacle-dodge mechanic" label, which conflated 3 separate systems and mischaracterized what actually jumps). |
| Alley patrol objects (rats/mice — `objects.asm`'s `init_objects`/`cycle_animations`) | **Done, verified, unit-tested — see §5s.** Real AI (idle/patrol/chase/reversal), real sprites, real cat-collision knockback + climb-transition trigger, wired into the main loop. |
| Fish-jump enemy + thrown-projectile gravity (`level_physics.asm`'s `update_cat_jump`/`apply_cat_gravity`) | **Surveyed, not yet ported — see §5s.** Data tables (`gravity_sprite_ptrs`/`gravity_sprite_dims_tbl`) already verified in §5i. |
| Falling window objects (`level_physics.asm`'s `animate_falling`/`check_jump_collision`) | **Surveyed, not yet ported — see §5s.** `fall_sprite`'s dims are computed dynamically (not a lookup table) and need more work to fully verify. |
| `alley.asm` (background save/restore, window events, death) | **Ported & verified — see §6f.** `save_alley_buffer`/`restore_alley_buffer`/`draw_alley_foreground`/`save_cat_background` are real in all four files that stubbed them, so sprites erase themselves and the demo no longer wipes the screen every frame. `spawn_window_event`/`enter_building`/`handle_cat_death` ported too. |
| `sound.asm` (PC speaker, music, effects) | **Ported & verified — see §6e.** All ~40 routines, driving an emulated PIT channel 2 + port 0x61 (`src/speaker.c`) through SDL2 audio (`src/audio.c`). Every previously-stubbed sound call site in the project is now wired to the real thing. |
| Game loop / physics / enemies / sound / UI / score | **Substantially advanced (§5b/5e/5f/5n/5o/5q/5r/5s/5t/5u/5v); fish-jump/gravity-toss (§5t), falling-object dodge (§5u), and score/lives HUD (§5v) all ported and verified. `draw_level_background` covers all indoor levels (1-6, §5w/§5x) except level 7's victory epilogue. `sound.asm` done (§6e); `ui.asm`/`throw.asm` still not started.** |

Build: `make` (needs `libsdl2-dev`). Run: `make run` or `./build/alleycat`.

**Stale-paragraph correction:** this spot used to say the build "renders a
placeholder bouncing blob, not real sprites yet". That has been untrue since
§5c — the demo renders real sprites, real movement, real enemies, real
patrol objects, a real HUD, and (as of §6e) real sound. Left as a note
rather than silently deleted, since the §1 table above is the live status.

---

## 2. Project layout

```
include/
  cga.h          — CGA framebuffer API (see §4 for the critical width-units finding)
  input.h        — SDL keyboard -> original's key-state variables
  video.h        — SDL2 window/renderer/texture wrapper
  sprite.h       — shared cat_walk_frame_t struct (data ptr + width_words + height)
  gen/           — AUTO-GENERATED sprite data headers, do not hand-edit;
                   regenerate via tools/*.py instead (see below)
src/
  cga.c, input.c, video.c, main.c
tools/
  asm_data_to_c.py          — generic: converts a data/*.asm's labeled `db`
                               regions into 1:1 byte-exact C arrays. Good for
                               bulk/mechanical extraction, but DOES NOT know
                               about pointer tables — see §3 for why that
                               matters for anything sprite-shaped.
  resolve_data_segment.py   — walks cat.asm's ENTIRE .data section (following
                               %include) as one linear byte stream, recording
                               every label's true DS-relative offset. This is
                               the ground truth for resolving pointer tables.
                               Outputs: /tmp/data_segment.bin (28976 bytes,
                               verified == 0x7130 expected size) and
                               /tmp/data_segment_labels.txt (783 labels).
  extract_walk_sprites.py   — uses resolve_data_segment's output + the
                               verified word-width formula (§4) to slice the
                               13-frame "Pool A" walk cycle referenced by
                               walk_sprite_ptrs/walk_sprite_dims (cat.asm src/game_loop.asm ~line 409).
                               -> include/gen/cat_walk_frames.h
  extract_walk_frame_table.py — same idea for the 12-frame "Pool B" cycle
                               referenced by walk_frame_table (src/alley.asm
                               update_walk_frame), plus the single fixed
                               climb/enter sprite at DS 0x9da.
                               -> include/gen/cat_alley_walk_frames.h
```

To regenerate everything from a clean checkout of the disassembly repo:
```
python3 tools/resolve_data_segment.py       # rebuilds /tmp/data_segment.bin + labels
python3 tools/extract_walk_sprites.py
python3 tools/extract_walk_frame_table.py
python3 tools/asm_data_to_c.py <data/X.asm> include/gen/X.h X_H   # per remaining data file
```

---

## 3. Why mechanical `db` extraction alone is NOT enough for sprites

`data/*.asm`'s own labels are unreliable as region boundaries. Example:
`walk_sprite_dims` in `data/cat_sprites.asm` visually looks like it spans
1466 bytes before the next label — but the *real* dims table inside it is
only the first 26 bytes (13 width/height word pairs); everything after that
is unlabeled raw sprite bitmap data that the disassembler's auto-labeler
just lumped under the preceding label.

The only reliable way to find true region boundaries for anything addressed
via a pointer table is:
1. Resolve the **absolute DS offset** of every label across the *entire*
   linear data segment (`resolve_data_segment.py`).
2. Read the pointer table (e.g. `walk_sprite_ptrs`) to get each sprite's
   real start offset.
3. Compute each frame's byte size from its **dims table entry** (see §4 for
   the units gotcha) and slice `data_segment.bin[ptr : ptr+size]` directly —
   never trust a same-file label as the end boundary.
4. **Cross-validate**: sort the pointers, diff consecutive ones, and check
   that the diff equals the predicted size from the dims table. If it
   doesn't match for every single frame (including the last one ending
   exactly at the next table's start), the extraction is wrong somewhere.
   This caught the §4 bug immediately and unambiguously.

This generalizes to every other sprite category (enemy, object, title,
result, extralife, foreground) — none of those have been through this
rigorous pointer-resolution pass yet, only the two cat walk-cycle pools.
Treat any *other* generated header in `include/gen/` as **raw, unverified,
byte-exact data** — good for archival, not yet sliced into meaningful
per-sprite frames.

---

## 4. THE key finding: sprite "width" fields are in WORDS, not bytes

The original's blit routines (`cga.asm`) walk their inner loop with
`lodsw`/`stosw` — word instructions — not byte instructions. The dims
tables' first byte (call it `w`) is therefore a **word count**: real bytes
per scanline = `w * 2`.

### Evidence (airtight — verified for all 13 + 12 = 25 frames across 2 pools)

For the 13-frame "Pool A" cycle (`walk_sprite_ptrs` + `walk_sprite_dims`,
both at DS `0x09a6`/`0x09c0`):

| frame | dims (w,h) | predicted bytes (`w*2*h`) | actual ptr delta to next frame |
|---|---|---|---|
| 0 | (3,12) | 72 | 72 ✓ |
| 1 | (3,13) | 78 | 78 ✓ |
| 4 | (3,12) | 72 | 72 ✓ |
| 5 | (3,13) | 78 | 78 ✓ |
| 8 | (3,14) | 84 | 84 ✓ |
| 9 | (3,13) | 78 | 78 ✓ |
| 10 | (2,14) | 56 | 56 ✓ |
| 11 | (2,13) | 52 | 52 ✓ |
| 12 | (2,9) | 36 | predicted end `0x09a6` == `walk_sprite_ptrs` start, **exact** |

(Frames 2/3 and 6/7 are mirror-reuse pairs — same pointer value twice by
design, so delta-checking doesn't apply to them, but every other frame
checks out, including the final frame's end boundary landing exactly on
the next table with **zero** slack either direction.)

For the 12-frame "Pool B" cycle (`walk_frame_table` at DS `0x0f7a`): all 12
pointers are spaced *exactly* 66 bytes apart, and the dims word used at both
call sites in `game_loop.asm` (`0x0b03` = 3 words, 11 rows) predicts
`3*2*11 = 66` — exact match, uniformly, for all 12 frames.

Before this correction, byte-width-based slicing left an unexplained 18-byte
gap at the end of Pool A and made no sense of Pool B's uniform 66-byte
spacing at all. After it: zero gaps, zero overlaps, everywhere checked.

### Where this was fixed
- `include/cga.h` / `src/cga.c`: `blit_to_cga`, `save_from_cga`,
  `blit_masked`, `blit_transparent`, `copy_with_stride` all take
  `width_words` now (not `width_bytes`), multiplying by 2 internally.
- `tools/extract_walk_sprites.py`, `tools/extract_walk_frame_table.py`:
  slice using `w_words * 2 * h`.
- **Not yet applied**: nothing else in the codebase used width yet (the
  placeholder sprite in `main.c` draws pixel-by-pixel, bypassing this). Any
  future code that reads a dims table and calls a `blit_*()` function must
  pass the raw `width` field straight through (it's already in words) —
  don't double-convert.

---

## 5. Two separate cat animation pools — what's understood, what isn't

There are (at least) two distinct sprite pools plus one fixed single sprite,
all packed contiguously in DS `0x06d0`–`0x0f7a`:

1. **Pool A** (DS `0x06d0`–`0x09a6`, 730 bytes, 13 frames via
   `walk_sprite_ptrs`/`walk_sprite_dims`). Selected in `game_loop.asm`
   (~line 395-410) via a **bitfield computed from `scroll_direction` and
   `in_level_mode`** (`and bx,0x6` / `or bl,0x8` / `or bl,0x10` / `add bl,4`),
   giving at most ~7 distinct index values — this does NOT look like a
   smooth walk-cycle selector, more like "pick the right static pose for
   the current direction/mode combo." **Not yet fully decoded.**

2. **Fixed sprite** at DS `0x9da` (3 words × 14 rows = 84 bytes,
   dims hardcoded as `0x0e03` directly in `game_loop.asm`, not looked up via
   any table) — used specifically when `at_platform == 1` (i.e. the "cat
   entering/climbing through a window" pose). Sits immediately after Pool
   A's real dims table ends, with zero gap.

3. **Pool B** (DS `0x0bd2`–`0x0eea`, 12 frames × 66 bytes via
   `walk_frame_table`, read through `update_walk_frame` in `alley.asm`).
   Called from two sites in `game_loop.asm`: once during the
   window-entry countdown (`entry_steps`), once during general alley
   movement when not `in_level_mode` and not at a platform. This one DOES
   look like a genuine walk-cycle (`update_walk_frame` increments a 0-5
   counter and mirrors +6 for the opposite direction — a classic walk-cycle
   pattern) — **this is the one to wire up first** for visible walking
   animation.

4. There's a ~504-byte unaccounted-for gap between the fixed sprite (ends
   `0x0a2e`) and Pool B's first frame (`0x0bd2`), and a further ~144-byte
   gap between Pool B's last frame end (`0x0eea`) and the `walk_frame_table`
   pointer array itself (`0x0f7a`). Likely other sprites (`climb_sprite_*`,
   `enter_sprite_*`, `recoil_sprite_*` tables reference *something*) —
   **not yet investigated**.

### `draw_alley_foreground`'s blit call is `blit_masked`, not `blit_transparent`

This was noticed but not resolved: `draw_alley_foreground` (`alley.asm`)
calls `blit_masked`, which is a pure AND of source against destination — it
can only *clear* bits, never set new ones. For a sprite to render visibly
this way, the source data must be pre-encoded so that "ink" pixels are 0
(forcing those bits off/black regardless of background) and "transparent"
pixels are all-1s (AND leaves background untouched). This would mean the
cat renders as a **black silhouette** against whatever's behind it, which
is plausible for this game's look but **hasn't been visually confirmed** —
just inferred from the AND semantics. Open question for next session:
render one of the extracted Pool B frames through the corrected
`blit_masked` against a known background and visually check it looks like
a recognizable cat silhouette, not noise.

---

## 5b. Pool A frame-selection logic — DECODED, ported, and verified

`game_loop.asm` lines ~349-411 (unnamed block, no label) is a state machine
that picks which of Pool A's 13 frames to display. Traced label-by-label:

**Special cases (bypass the walk counter):**
- `immune_flag != 0` → index 8 (reuses the alley-idle pose — presumably
  combined with a flashing/blink effect elsewhere for hit-invincibility).
- `scroll_direction` or `in_level_mode` changed since last frame (i.e. the
  cat just turned around or transitioned alley↔level) → index 12 (the
  smallest frame, 2w×9h — a dedicated "turning" pose).

**Normal case** (direction/mode unchanged): increment the `walk_frame`
RAM counter (word), then branch on `cat_y` / `in_level_mode` /
(`anim_counter`, `speed_ramp`) into one of two cycles:

- **Horizontal branch** (scrolling left/right): `bx = walk_frame & 0x6`
  (cycles 0,2,4,6 → indices 0-3), `+8` if moving left (→ indices 4-7).
  Confirmed: indices 0-3 and 4-7 have *identical* dims sequences
  `(3,12),(3,13),(3,10),(3,13)` — same walk-cycle shape, separate
  pre-flipped bitmaps for each direction (not runtime-mirrored).
- **Vertical/idle branch**: `bx = (walk_frame & 0x2) | 0x10` → indices 8-9
  (idle bob in the alley); `+4` more if `in_level_mode` → indices 10-11
  (2w-wide climbing frames).

Ported faithfully in `src/animation.c` (`select_cat_sprite()`) as a
**label-for-label goto translation** rather than a "cleaned up" rewrite —
deliberate, since we have no running original to test control-flow
equivalence against, and goto-preserving translation is the standard safe
approach for this kind of port. Caught and fixed one translation slip
during this process: `jnc` (jump if **not** carry) was initially inverted
to mean "jump if less than" — it actually means "jump if >=" (no borrow).
Comments in the source call this out explicitly.

**Unit-tested** (`/tmp/test_anim.c`, not checked into the repo but easy to
reconstruct — see below) against all 6 branches by directly setting the
state globals and checking the returned frame lands in the documented
index range:

```
immune              -> idx 8   (expect 8)                    PASS
direction changed   -> idx 12  (expect 12)                   PASS
walk-right (6 steps)-> idx sequence 0,1,1,2,2,3               PASS (stays in 0-3)
walk-left  (4 steps)-> idx sequence 4,5,5,6                   PASS (stays in 4-7)
idle-in-alley        -> idx sequence 8,8,8,9                  PASS (stays in 8-9)
climbing              -> idx sequence 10,10,10,11             PASS (stays in 10-11)
```

The walk-right/left index progression (0,1,1,2,2,3,...) is a smooth
4-frame cycle held for ~2 ticks per frame — exactly what a walk animation
should look like, which is a good independent sanity check beyond just
"stayed in range."

**Gotcha hit while writing the test, worth remembering for future ports:**
`include/gen/*.h` originally defined sprite arrays as `static const` — fine
within one file, but it means **each translation unit that includes the
header gets its own separate copy** of the array. A test file doing
pointer arithmetic against `cat_walk_frames` compiled separately from
`animation.c` was comparing addresses in *two different arrays*, producing
garbage indices (22-26 instead of 0-12) that had nothing to do with actual
logic bugs. Fixed by splitting generated headers into an `extern` header +
a single companion `.c` file with the real definitions
(`src/gen_cat_walk_frames.c`, `src/gen_cat_alley_walk_frames.c`) — normal C
practice, and now the safe pattern to follow for any future generated
sprite pool.

**CONFIRMED** (visual test, see below): `draw_alley_foreground`'s
`blit_masked` (AND-only) call does produce a coherent walking cat sprite —
rendered against a solid white CGA background, frame 0 of Pool B shows a
clear, stable head/torso silhouette in the top rows and a leg region in the
bottom rows that visibly changes shape across frames 0→1→2→3, exactly the
alternating-leg pattern expected of a walk cycle. Tested against black
background too, which (correctly, and as predicted from AND semantics)
shows nothing — black-on-black is indistinguishable, an expected and
uninformative degenerate case, not a failure.

**Important structural consequence of `blit_masked` being AND-only:** since
AND can only ever *clear* bits, never set them, the cat drawn through this
path can **only ever end up black** (all-zero = CGA color index 0) —
never cyan/magenta/white. So whatever background color is behind the cat,
`draw_alley_foreground` always renders it as a pure black silhouette, with
zero color variation from the sprite data itself. This is a hard
consequence of the instruction semantics, not a guess — worth keeping in
mind if a later frame looks "wrong" for having no highlight colors; that's
expected, not a bug.

## 5c. Real rendering wired into `main.c`

`src/main.c` now drives the actual ported pipeline every frame instead of
the placeholder blob:

```
input_poll()                                 -- real SDL keyboard state
scroll_direction = input_horizontal          -- -1/0/1, matches original's encoding
in_level_mode    = (input_vertical<0) ? 1:0  -- hold UP to preview the climb pose
frame = select_cat_sprite()                  -- ported game_loop.asm state machine
blit_masked(frame->data, ..., NULL)          -- verified black-silhouette path
video_present()
prev_scroll_dir = scroll_direction; prev_vert_dir = in_level_mode; -- for next frame's "did it change" check
```

Screen background is filled with a visible color (not black) specifically
*because* of the §5b finding — `blit_masked` can only ever draw black, so
a black background would make the cat invisible. `cat_y` is pinned to a
mid-range constant (`0x60`) since we haven't ported ground-height/level
data yet — it exists here purely to keep `select_cat_sprite`'s branch
logic on the "normal ground movement" path rather than a level-specific
one. Actual on-screen movement uses a separate `screen_x`/`screen_y` pair
(the original's `cat_x`/`cat_y` are branch-decision inputs, not 1:1 with
screen pixel position, in the part of the code ported so far).

**Verified**, not just "builds": an integration test mirroring `main.c`'s
per-frame logic (`/tmp/test_integration.c`, reconstructable from this
description) confirms across several simulated frames that the rendered
shapes are stable, non-garbage silhouettes matching the expected sizes
(2x9 turn pose on the very first frame — correctly triggered because
`prev_scroll_dir` starts at 0 and differs from the first real input,
exactly per the decoded "direction changed" branch; 3x12 walk-pose shapes
on subsequent frames). The real binary (`build/alleycat`) was also run
for 5 real seconds under `SDL_VIDEODRIVER=dummy` with no crash.

**Known simplification, to revisit once `level_physics.asm` is ported:**
`speed_ramp`/`anim_counter` are hardcoded (100 / 0) to keep the state
machine on its "normal" branch; the original derives these from real
physics state we haven't ported yet.

## 5d. The two unaccounted-for gaps — investigated

### Gap 1 (DS `0x0a2e`–`0x0bd2`, 420 bytes): FULLY RESOLVED

This sits between the fixed climb/enter sprite (ends `0x0a2e`) and Pool B's
first frame (`0x0bd2`). Two small tables that come right *after*
`walk_frame_table` in the file — `enter_sprite_data` (5 pointers) and
`climb_sprite_ptrs` (1 pointer) — point directly into it, and
`recoil_sprite_ptrs` (8 pointers) references it too, **plus reuses two of
Pool B's own frames** (`0x0bd2` = Pool B frame 0, `0x0d5e` = Pool B frame
6 — i.e. the game's "recoil/knockback" animation is built from a couple of
brand-new poses stitched together with two walk-cycle frames it already
has, rather than storing a fully separate animation).

Cross-checked exactly the same way as Pool A/B (§4): every single pointer's
predicted end (`ptr + width_words*2*height`) lands **exactly** on either
the next pointer in the same table or a known Pool B frame start — zero
gap, zero overlap, across all 5+1+8 = 14 pointers (several duplicated
across tables; 6 genuinely distinct new sprites total: 2 "entering window"
poses unique to `enter_sprite_data`, 1 "climbing" pose, 3 more "recoil"
poses unique to `recoil_sprite_ptrs`). Sum of all distinct pieces plus the
two Pool-B reuses accounts for the full 420 bytes with no remainder.

### Gap 2 (DS `0x0eea`–`0x0f7a`, 144 bytes): partially resolved

This sits between Pool B's last frame and the `walk_frame_table` pointer
array itself. Searched every currently-split-out `src/*.asm` file
(`game_loop.asm`, `alley.asm`, `alley_drawing.asm`, `level_objects.asm`,
`enemy.asm`, `objects.asm`, `throw.asm`, `score.asm`, `ui.asm`,
`entry.asm`, `sound.asm`) for any hex literal or indexed reference landing
in this range — **no hits**. (A few `mov cx,0xf0N`-style false positives
turned up in `enemy.asm`/`level_objects.asm`, but those are enemy sprite
*dims* constants that happen to share bit patterns with addresses in this
range — confirmed unrelated by context, not a real reference.)

Inspected the raw bytes directly: **not padding** — 144 bytes, only 12
distinct byte values, dominated by extreme bit patterns
(`0x00, 0xff, 0xf0, 0x0f, 0xc0, 0x3f, 0xfc, 0x03, ...`). That's the
signature of a binary CGA **mask plane** (sharp opaque/transparent edges,
2 bits per pixel each fully 00 or 11) — structurally very different from
the walk-cycle color data resolved so far, which has much more varied
byte values.

**Working hypothesis, not yet confirmed:** this could be mask-plane data
for a `blit_transparent`-style two-plane draw of the cat (recall
`draw_alley_foreground` only uses AND-only `blit_masked`, forcing a flat
black silhouette — see §5b/5c) used by some *other*, not-yet-decoded call
site that draws the cat in color. No such call site has been found yet,
because the code files that would contain it (`level_objects.asm` at 3861
lines, plus whatever handles death/game-over animations) haven't been
searched in as much depth as the ones above. **Leaving this open rather
than guessing further** — revisit once `level_objects.asm` gets a real pass.

## 5e. Real movement physics — decoded, ported, and verified (correcting an initial wrong assumption)

**Important correction:** `level_physics.asm` is misleadingly named — despite
what its function names suggest, `apply_cat_gravity` and `update_cat_jump`
are **not** the cat's own movement physics. Per their own header comments
in the file, they handle (1) gravity on objects *thrown at* the cat
(fish/projectiles) and (2) enemy spawn/jump-arc behavior. The cat's real
per-frame movement — the thing this session actually needed — lives in
`game_loop.asm`, immediately before the `select_cat_sprite` block already
ported in §5b (they share the same state variables, which is how this was
found).

### What it actually is

Not classic platformer gravity/jump. It's a **momentum-ramp system**:
`speed_ramp` (horizontal) and `anim_counter` (vertical) are accumulators
that ramp up toward a ceiling while a direction is held, decay toward a
floor when released, and reset to a mid baseline when the direction
changes. The actual speed/dive-depth applied each frame is
`min(accumulator >> shift, per-difficulty cap)`, with the caps coming from
two small verified tables:

- `max_swim_speed` (DS `0x066c`, 6 words): `4, 6, 8, 10, 12, 12`
- `max_dive_depth` (DS `0x067c`, 6 bytes): `3, 3, 4, 4, 4, 4`

indexed by `difficulty_level`. Ported as `update_cat_movement()` +
`update_scroll()` in `src/movement.c`, again as a literal goto-based
translation (same rationale as §5b: no running original to test control
flow against).

### Bug 1 caught while porting: `in_level_mode` is not a boolean

Initially assumed (in §5b/5c) that `in_level_mode` was a plain 0/1 flag.
Porting this block showed it's actually assigned directly from
`input_vertical`, which is **tri-state** (`-1`/`0`/`1`) — so `in_level_mode`
really means "current vertical direction," not "am I in a level." Existing
code that only ever checked `==1` still behaves correctly either way, but
the type was corrected from `uint8_t` to `int8_t` throughout
(`cat_state.h`) to reflect this honestly, and `animation.c`'s comparison
against `prev_vert_dir` was simplified now that both are properly signed.

### Bug 2 caught while porting (same *class* of bug as an earlier one): unsigned `jc` on a signed-looking value

The original does `cmp al,0x1` / `jc` to decide between the "climbing
deeper" (`in_level_mode==1`) and "returning to alley" (`in_level_mode==-1`)
branches. `jc` is an **unsigned** byte comparison — with `in_level_mode`
holding `0xFF` (the byte pattern for `-1`), `0xFF` is unsigned-*above* `1`,
so `jc` is **not** taken, and execution correctly falls through to the
subtract branch. My first translation pass used a signed comparison
(`in_level_mode < 1`), which incorrectly caught the `-1` case too and
skipped the vertical movement adjustment entirely — the bug was caught
immediately by the physics unit test (§ below): holding UP produced zero
`cat_y` change across 10 frames, which was an obvious tell. Fixed by
explicitly casting to `uint8_t` before the comparison, with a comment
explaining why, mirroring the `jnc`-inversion bug caught in §5b. **General
takeaway reinforced again**: any `jc`/`jnc`/`ja`/`jbe` on a byte that can
represent a negative value needs the C translation to compare as
`uint8_t`, never as the signed type, however "obviously signed" the
variable's meaning looks.

### Unit-tested (`/tmp/test_movement.c`)

Five scenarios, all producing physically sensible results after the fix:
1. Holding RIGHT: `speed_ramp` ramps `0x20→0x32` (mild one-step overshoot
   past the `0x30` cap before clamping kicks in on the *next* check — this
   is faithful to the original's check-before-add ordering, not a bug).
2. Releasing input: `speed_ramp` decays smoothly back down.
3. Holding UP: `cat_y` decreases (moves up-screen) at 2-3px/frame, capped
   by `max_dive_depth[0]=3`, `in_level_mode` correctly settles to `-1`.
4. Holding DOWN: `cat_y` increases symmetrically, `in_level_mode` settles
   to `1`.
5. Forcing `cat_x` near the scroll-right boundary while holding RIGHT:
   correctly clamps to `scroll_right_bound - 1` every frame rather than
   drifting past it.

### Wired into `main.c`

`main.c` now calls `update_cat_movement()` once per frame (which also
handles the `prev_scroll_dir`/`prev_vert_dir` snapshot at the right point
in the sequence — the manual snapshot that used to live in `main.c` after
`select_cat_sprite()` was removed, since the ported function now owns that
timing exactly as the original does), then `select_cat_sprite()`, then
draws. `cat_x`/`cat_y` are real accumulated position now, not a
placeholder demo drift.

**Known stub, low priority:** the footstep-sound-sync check at the end of
this block (comparing the active sprite pointer against
`walk_sprite_ptrs[9]` to latch a tick for `sound.asm`) is ported as a
structural no-op — `sound.asm` isn't ported yet, so there's nothing
downstream to feed. Revisit once sound is tackled.

## 5f. Scene initialization — `setup_alley` / `setup_level` ported and verified

`game_loop.asm`'s first ~107 lines: `setup_alley` (spawns the cat at the
left or right edge of the alley depending on its current `cat_x`, resets
essentially every per-scene game-state flag) and `setup_level` (positions
the cat for level entry — from `saved_cat_x/y` for level 0, or from two
small per-level tables otherwise — and sets up the "entering the level"
sprite pose). Ported directly, not goto-based this time, since both are
mostly straight-line variable assignment with only one or two simple
branches each — no ambiguous control flow to preserve literally.

**Verified tables** (`level_start_x_table` DS `0x05d9`, `level_start_y_table`
DS `0x05e9`, both cross-checked against the resolved data segment):
```
x: 160, 144, 150, 88, 256, 240, 64, 152
y:  20,  96,   4, 96,  96,  96, 56, 175
```

**Nice payoff from earlier work:** `setup_level`'s vertical-entry sprite
pointer (`0xfb2`/`0xfbe`, hardcoded literals in the original) resolves to
`enter_sprite_data[3]` / `enter_sprite_dims[3]` — i.e. it's
`enter_sprite[3]` from the gap-1 investigation in §5d, already extracted
and available. Rather than replicate the original's raw 16-bit
DS-offset/dims-word pair (`vert_sprite_data`/`vert_sprite_dims`), which
doesn't map onto anything meaningful in this port's architecture (sprites
here are typed C arrays, not one flat DS blob), `vert_sprite` is a real
`const cat_walk_frame_t *`. Caught and fixed a first draft that tried to
truncate an actual host pointer into a 16-bit "offset" — meaningless and a
straightforward source of garbage if ever dereferenced; flagged and
corrected before it shipped.

**Known stubs:** `save_alley_buffer()` and `reset_window_state()` are
faithful no-op stubs, called in the right places so the ported control
flow stays structurally identical to the original — filling them in
requires the alley background-drawing pipeline (not ported) and the
window-animation state machine (not ported), respectively.

**Unit-tested** (`/tmp/test_setup.c`): 5 scenarios — left-edge spawn,
right-edge spawn, level-0 entry (from `saved_cat_x/y`), level-3 entry
(from the tables, checked against the exact expected values), and
level-2's special-case extras (`anim_counter`, `speed_ramp`,
`level2_rise`, `meow_timer`) — all produced exactly the expected values.

`main.c` now calls real `setup_alley()` on startup and on restart, instead
of hand-set demo values.

## 5g. IMPORTANT CORRECTION: §5b/§5e's ported block is level-2-specific, not general movement

While tracing `update_animation`'s full dispatch to wire in the real draw
call, found that the block ported in §5b (`select_cat_sprite`, Pool A
frame selection) and §5e (`update_cat_movement`, the momentum-ramp
physics) is **only reached from the level-2 "underwater/drowning"
special-case branch** of `update_animation` — not from the general
per-frame path used for normal alley walking, as originally assumed.

**How this was found:** `update_animation` dispatches on `level_number`:
- `level_number == 2` → `lab_0953`: a self-contained drowning-timer state
  machine (tick-threshold tables `level2_phase1_ticks`/
  `level2_death_ticks`/`level2_color1/2/3_ticks`, meow-sound timer,
  screen-border color warning via BIOS `int 0x10`) that **falls through**
  into exactly the code already ported in §5b/§5e (confirmed by the label
  `lab_09f6`, which has no other predecessor anywhere in the file).
- `level_number != 2` → `jmp lab_0bac`: an entirely different function —
  `check_dog_collision` gating, then an `entry_steps`/`at_platform` state
  machine (window-entry countdown using Pool B via `update_walk_frame`,
  matching what was already found in §5d/§3c about Pool B's real usage;
  then the fixed climb sprite draw at `at_platform==0`; then a further
  dispatch at `at_platform==1` on `in_level_mode` to `lab_0e23` — this is
  where genuine normal-alley-movement almost certainly lives, but it
  hasn't been examined yet).

**What this means concretely:** the ported code in `movement.c`/
`animation.c` (`update_cat_movement`, `select_cat_sprite`) is **real,
correctly-ported, verified code** — the branch logic, the momentum ramp,
the unit tests, all of that stands. What was wrong was the *label*
attached to it: it's the level-2 drowning-sequence's cat physics/pose
selector, not a general-purpose one. `main.c`'s current demo loop calls it
unconditionally every frame regardless of `level_number`, which produces a
working, verified demo — just not one that represents "the cat walking in
the alley" the way it was described. **This needs re-labeling/re-scoping,
not rewriting** — the underlying port is sound.

**Not yet ported, and now understood to be the actual missing piece for
general movement:** the `entry_steps`/`at_platform` state machine at
`lab_0bac` (window-entry sequence using Pool B, fixed climb-sprite draw,
then dispatch to `lab_0e23` for `in_level_mode==0`, i.e. plain alley
movement) — `lab_0e23` itself hasn't been read yet.

**Process note:** caught by deliberately tracing full control flow before
wiring `draw_alley_foreground`'s real call site into `main.c`, rather than
assuming the two previously-ported blocks were "the" movement system just
because they were self-consistent and testable in isolation. Self-
consistency and passing unit tests are necessary but not sufficient — they
don't verify a block is reached under the conditions assumed. Worth
remembering for the rest of this port: a function being well-behaved in
isolation says nothing about when the original actually calls it.

## 5h. The REAL general alley-movement path — found, ported, and verified

Continuing from §5g: traced forward from `update_animation`'s
`level_number != 2` dispatch (`lab_0bac`) through `check_dog_collision`
gating and the `entry_steps`/`at_platform` state machine to
`lab_0e78`→`lab_0f34`→`lab_0f63` — this is genuinely the plain "cat
walking in the alley" path, confirmed by `cat_sprite_dims` being
hardcoded to `0x0b03` right before the `draw_alley_foreground` call —
exactly Pool B's dims (3 words × 11 rows), closing the loop with §5d's
original guess about Pool B's purpose.

**Ported as `update_walk_frame()`** (`src/movement.c`) and
**`update_alley_movement()`** (new file, `src/alley_movement.c`):

- `update_walk_frame` — alley.asm's real function of that name (distinct
  from the `walk_frame` RAM counter used by §5b's level-2-specific code).
  Has its **own independent speed-ramp** (`scroll_speed`/
  `anim_accumulator`), separate from `update_cat_movement`'s
  `speed_ramp`/`anim_counter` — different callers feed `scroll_speed`
  differently depending which path they're on. Advances `walk_anim_frame`
  (0-5), mirrors +6 for leftward movement, returns the Pool B frame.
- `update_alley_movement` — the direction-assignment header
  (`lab_0e78`) plus the plain-alley body (`lab_0f34`/`lab_0f63`): calls
  `update_scroll()`, computes `cat_screen_pos`, and — if actually
  moving — calls `update_walk_frame()` and draws via the same verified
  `blit_masked` path as before.

**Faithfully NOT ported (documented, not guessed):** the climbing/
window-transition branches (`in_level_mode != 0`) and the
`check_level_collision`-gated ladder-entry logic above `lab_0e78` — those
need level geometry data from `level_objects.asm`, which hasn't been
touched. Calling `update_alley_movement()` with vertical input currently
just records `in_level_mode` and returns without drawing new geometry,
rather than guessing at unported logic.

**Stubs, consistent with the project's established pattern:**
`update_footprint`, `spawn_window_event`, `check_dog_collision`,
`check_enemy_activate` (each needs a file not yet ported —
`level_objects.asm`, `alley.asm`'s window state machine, and `enemy.asm`
respectively). `restore_alley_buffer` **is** structurally ported (a real,
correct 3-line function using `blit_to_cga`), but is currently inert
because `save_alley_buffer` (still a stub since §5f) never populates the
buffer it would restore.

**Unit-tested** (`/tmp/test_alley.c`): holding RIGHT for 8 frames shows
`cat_x` advancing with `scroll_speed` smoothly ramping `2→3→4→5→6`
(matches `update_walk_frame`'s own accumulator-gated ramp, distinct from
§5e's system) and the frame index cycling through the expected walk-cycle
positions; holding LEFT correctly reverses and clamps at the left scroll
bound (`0x8`); releasing to idle correctly halts movement.

**Demo-only adaptation, clearly flagged, not a silent deviation:**
`main.c` clears the whole screen every frame (a simplification from day
one of this port, since a working `save_alley_buffer`/
`restore_alley_buffer` cycle doesn't exist yet), which would make the cat
disappear whenever `update_alley_movement`'s idle/climbing branches
correctly *don't* redraw (matching the original's persistent-framebuffer
behavior, where nothing erases an idle sprite). Rather than let the
demo look broken, `alley_movement.c` remembers the last frame/position
actually drawn and re-blits it on idle/climbing frames — noted inline as
temporary, to be removed once real background persistence exists.

`main.c` now uses `update_alley_movement()` as the primary per-frame call,
replacing the earlier (correctly level-2-scoped, per §5g) demo.

## 5i. Sprite-category verification pass: `enemy_sprites.asm`

Applying the same rigor as the cat sprites (§3/§4) to the other sprite
categories, starting with `enemy_sprites.asm`. Results:

### `gravity_sprite_ptrs`/`gravity_sprite_dims_tbl` — FULLY VERIFIED

4-frame table (the "cat falling after being knocked off a ledge"
animation). Sorted by pointer value, every frame's predicted end
(`width_words*2*height`) lands exactly on the next frame's start, and the
last frame ends **exactly** at `gravity_sprite_ptrs`'s own start
(`0x17c9`) — zero gap, zero overlap, same pattern as the cat sprites.

### `death_sprite` — size corrected, and this caught a real bug

`draw_alley_foreground`'s call site (`alley.asm` line ~208) sets
`cx=0x1205` before calling **`blit_transparent`** — the first confirmed
real usage of that function anywhere in the codebase. Cross-checking this
against the raw dims (`cl=5` words wide, `ch=0x12=18` rows) forced a
re-read of `blit_transparent`'s actual assembly (`cga.asm`), which
revealed:

**`src/cga.c`'s `blit_transparent` implementation is wrong.** It was
written (early in this project, before the width-in-words finding even)
assuming an interleaved `(mask_word, color_word)` pair read per column —
but the real algorithm reads **one word per column** via `lodsw`, then
runs a data-dependent bit-twiddling loop (XOR against `0x33cc`, iterative
`test`/`or` against a `0x30c0` mask) to derive which 2-bit pixels count as
"transparent" from a *single* combined source word, not two separate
planes. This is a meaningfully different (and non-trivial) algorithm that
hasn't been correctly reverse-engineered yet.

**Impact, contained:** `blit_transparent` was already ARM'S-LENGTH from any
currently-wired rendering — `draw_alley_foreground` (the only draw path
wired into `main.c` so far) uses `blit_masked`, which **is** correctly
verified (§5b/§5c). So this bug hasn't corrupted anything currently
visible in the demo. But it means `blit_transparent` must be treated as
**broken/unverified** until properly re-derived — flagged with a loud
comment in `cga.h`/`cga.c`, not deleted, so the (correct) function
signature stays in place for when it's fixed.

**What this did resolve correctly:** since `blit_transparent` (like every
other blit function) consumes exactly one word per width-unit, `cx=0x1205`
does mean `death_sprite` is `5*2*18 = 180` bytes — not the 336 bytes the
old raw mechanical extraction reported. Verified: `death_sprite` (180B) is
followed, with **zero gap**, by the complete `gravity_sprite` 4-frame
chain (156B) which is immediately followed by `gravity_sprite_ptrs`
itself — a fully gap-free, cross-verified 336-byte span confirmed across
*two different sprite categories* at once.

### `jump_land_sprite_data` — verified as a lookup table, not a sprite

`level_physics.asm` indexes it as `((jump_draw_y - jump_y) << 3) +
jump_land_sprite_data`, then `rep movsw` for 4 words (8 bytes) — a
parameter table for the jump-landing effect, not raw sprite bitmap data.
Its total size (120 bytes) is an exact multiple of 8 (15 entries), and it
abuts `jump_spawn_x_table` with zero slack — structurally confirmed
correct, though the *semantic meaning* of each 8-byte entry hasn't been
decoded (probably CGA byte-pattern deltas for a dust/impact visual).

### `enemy_sprite_table` — same split-label bug as the cat sprites, confirmed

`update_enemy_sprite` computes a bitfield index (`and bl,0x6 / or bl,0x8`
or `and bl,0x2 / or bl,0x4`, max value 14) into `enemy_sprite_table` —
needing at least 8 word entries (16 bytes), but the auto-labeler split
this into `enemy_sprite_table` (1 byte) + `enemy_sprite_table_hi` (21
bytes). Re-read as one continuous 11-word table: all 11 values fall
within valid DS range (`0x1280`-`0x1550`), with the last 3 entries zeroed
(unused padding beyond the real max index of 14) — confirms this is
genuinely one table, mislabeled exactly like `walk_sprite_dims` was in
§3. **General lesson from §3/§7 holds across categories, not just cat
sprites** — same systematic labeling issue recurs independently in
`enemy_sprites.asm`.

**Not yet extracted:** the actual enemy sprite bitmap bytes at those 8
pointers (would need per-frame dims — `update_enemy_viewport` shows a
partially-fixed `cx=0xf04` dims value that gets adjusted by a runtime
factor rather than a full per-frame dims table, needs more reading).
Left for a future pass — this session's goal was verifying structure, not
completing every extraction.

## 5j. `blit_transparent` — fixed and verified

Following up on the bug flagged in §5i: re-traced `cga.asm`'s real
`blit_transparent` algorithm bit-by-bit (it reads one source word via
`lodsw`, then runs a data-dependent loop XORing a mask against `0x33cc`
and testing against `0x30c0`, applied via `test`/`or` to each byte of the
word — see the full instruction trace in the source history of this doc if
needed). Worked through what that loop actually computes per 2-bit CGA
pixel group, arrived at a simple closed-form description, and **verified
it by brute force over all 16 possible (source-pixel, dest-pixel)
combinations** rather than trusting the derivation by inspection alone:

```
orig=00 dest=XX -> result=dest   (all 4 dest values checked, all match)
orig=01 dest=XX -> result=01     (all 4 dest values checked, all match)
orig=10 dest=XX -> result=10     (all 4 dest values checked, all match)
orig=11 dest=XX -> result=11     (all 4 dest values checked, all match)
```

**In plain terms: black (CGA color index 0) is the chroma-key transparent
color.** Any black pixel in the source lets the background show through
unchanged; any non-black pixel (colors 1/2/3) is drawn opaquely, exactly
as stored, regardless of what's behind it. Much simpler than the original
(wrong) two-plane guess — and a very standard, recognizable technique once
correctly decoded, which is a good sign the derivation is right.

**Reimplemented** in `src/cga.c` directly from this closed-form
description (not by replicating the original's iterative bit-mask dance —
they're provably equivalent per the brute-force check above, and the
closed form is far more readable/maintainable).

**Visually verified**: rendered `death_sprite` (§5i, 180 bytes, 5w×18h)
through the fixed function against both a white and a cyan test
background. Against cyan, the result clearly shows a mix of preserved
background dots and real sprite pixels forming a structured, non-random
silhouette — consistent with the cat's falling/"splat" death pose,
confirming the fix produces sane output rather than garbage.

**Extracted properly**: `tools/extract_death_sprite.py` →
`include/gen/death_sprite.h` / `src/gen_death_sprite.c`, using the
verified 180-byte size. Not yet wired into any gameplay trigger (the
falling-death mechanic itself — `level_physics.asm`'s actual jump/fall
physics, separate from what §5g found `level_physics.asm`'s misleadingly-
named functions actually do — isn't ported).

## 5k. Sprite-category verification pass: the remaining categories

Continuing §5i's rigor across `object_sprites.asm`, `title_sprites.asm`,
`result_sprites.asm`, `extralife_sprites.asm`, `foreground_sprites.asm`.

### `extralife_sprites.asm` — FULLY VERIFIED, zero slack anywhere

Traced real usage in `sound.asm`'s extra-life celebration sequence:
- `extralife_sprites[bx]` (2 entries, `blit_to_cga`, `cx=0x4404` → 4 words
  × 68 rows = 544B): the two pointers are spaced **exactly** 544 bytes
  apart — not a small icon, a fairly large celebratory graphic (68
  scanlines ≈ ⅓ of screen height).
- `extralife_icon_data` (`cx=0x1004` → 4w×16h = 128B) ends **exactly** at
  `extralife_text_data`'s start.
- `extralife_text_data` (`cx=0x1506` → 6w×21h = 252B) ends **exactly** at
  `extralife_text_pos`'s start (itself a position-offset table, not
  sprite data — `extralife_text_pos[bx]` is read as a destination `di`).

Three consecutive zero-gap boundaries, one exact pointer-delta match — as
clean a verification as the cat sprites got.

**Bonus finding:** this pass required checking whether `blit_to_cga`
follows the same `cl=width_words, ch=height` convention as `blit_masked`/
`blit_transparent` — it does (confirmed directly against `cga.asm`'s real
implementation, which had never been independently cross-checked before,
only assumed by analogy). One less latent assumption in the port.

### `title_sprites.asm` — 5 of 6 pieces fully verified, zero slack

`ui.asm`'s `show_title_screen` draws 6 distinct graphics in sequence
(logo, subtitle, and UI decoration pieces), each with an explicit
`cx=width,height` before its `blit_to_cga` call. Predicted sizes for all 6
pieces, checked against the next piece's actual label offset:

```
dat_6152 (11w×29h=638B) -> ends exactly at dat_63d0        ✓
dat_63d0 (14w×22h=616B) -> ends exactly at dat_6638        ✓
dat_6638 ( 3w×12h= 72B) -> ends exactly at dat_6680        ✓
dat_6680 (14w× 8h=224B) -> ends exactly at dat_6760        ✓
dat_6760 (12w×11h=264B) -> ends exactly at dat_6868        ✓
dat_6868 ( 4w× 8h= 64B) -> predicted end leaves 480B before
                            the next real label (attract_start_tick)
```

5 consecutive zero-gap matches in a row is not coincidental — the sizing
convention and label resolution method are confirmed sound. The 6th
piece's predicted placement is internally consistent (doesn't overlap
anything), but there's 480 unexplained bytes before the next labeled
variable — likely more title/attract-mode graphics not referenced by
`show_title_screen` specifically (e.g. attract-mode-only content). Left
uninvestigated further — the goal of this pass (confirming the extraction
methodology generalizes) is already well-supported by the other 5/6.

### `object_sprites.asm` — partially verified

- `cycle_idle_sprite` (`cx=0x802` → 2w×8h=32B) ends **exactly** at
  `cycle_walk_sprite`'s start — zero gap.
- `cycle_walk_sprite`: code only clearly uses 4 distinct 32-byte poses
  (toggle-frame × direction, offsets 0/0x20/0x40/0x60 = 128B total), but
  the next real label sits 96 bytes further out than that predicts —
  likely more poses (a third "cycle" variant, or an idle-flip pose) not
  covered by the one call site read so far. Not fully resolved.
- `collision_sprite` (`cx=0x806` → 8w×6h=96B, drawn via the now-fixed
  `blit_transparent`) leaves an unexplained 28-byte gap before the next
  label. Not fully resolved.

### `result_sprites.asm` / `foreground_sprites.asm` — contain no sprite bitmaps at all

Both files' only labels turned out, on checking real usage, to be
**non-bitmap auxiliary tables** that happen to share a memory region with
actual sprites elsewhere, not sprite data themselves:
- `cga_palette_table` (`result_sprites.asm`) — a small color-index lookup
  table (`enemy.asm` reads it as `bl = [si + cga_palette_table]`).
- `diff_icon_cga_pos` (`result_sprites.asm`) — a screen-position constant
  used as a blit *destination* (`di`), not a source.
- `attract_timing` (`foreground_sprites.asm`) — a timing-value table for
  demo/attract-mode pacing (`ax = [attract_timing]`), not a bitmap.

**Useful negative result**: nothing further to extract from these two
files as "sprites" — the file names (inherited from the disassembly
repo's own memory-region-based split, not content-based) are misleading
for these two specifically.

## 5l. `object_sprites.asm`'s two gaps — resolved (one fully, one honestly left open)

Following up on §5k's two unresolved items:

### `cycle_walk_sprite`'s extra 96 bytes: FULLY RESOLVED — legitimate padding, not a bug

The disassembly repo's own `data/object_sprites.asm` already carries
ASCII-art comments (not written by this port, pre-existing in the
uploaded repo) confirming `cycle_walk_sprite` is a **4-frame mouse
walk-cycle**, each frame 16px × 8 rows — exactly matching the `2 words ×
8 rows = 32B` size already derived from the `blit_masked` call site.
Checked the bytes immediately after the 4th frame (offset +128 through
+224, the "missing" 96 bytes) directly: **all zero**. This is genuine
padding in the original binary, not missing/unaccounted sprite data —
fully resolved, no further action needed.

### `collision_sprite`'s 28-byte tail: characterized, deliberately left open

Checked the 28 bytes between the verified 96-byte `collision_sprite`
frame (from its one confirmed `blit_transparent` call site,
`cx=0x806`) and the next label (`dat_1dec`): **not zero** — real,
patterned data, with byte sequences that recur inside `dat_1dec`/
`dat_1e00` themselves. `dat_1dec`/`dat_1e00` aren't referenced by name
anywhere in the source (they're auto-generated labels for otherwise-
unlabeled bytes, same as `_leading` regions elsewhere in this port).
Most likely explanation: either an additional `collision_sprite` frame/
variant reached via a call site not yet found, or genuinely unused
leftover data in the original binary. **Not chased further** — narrow,
low-value rabbit hole relative to the rest of the roadmap; flagged
clearly in the extracted header rather than silently included or
silently dropped.

### Extracted

`tools/extract_object_sprites.py` → `include/gen/object_sprites_ex.h` /
`src/gen_object_sprites.c`: `collision_sprite_frame` (96B),
`cycle_idle_sprite_frame` (32B), `cycle_walk_sprite_frames[4]` (32B each).
ASCII-rendered frame 0 of the walk cycle to sanity-check: clear
non-random structure, consistent with a small 16×8 sprite (not a
pixel-exact match against the repo's raw-bit ASCII art comments, since
that art uses a different bit convention than `blit_masked`'s AND-based
output — but structurally sound). Compiled cleanly into the project;
not yet wired into any gameplay (the mouse/cycle enemy object and
collision-effect trigger aren't ported — that's `objects.asm` territory,
separate from this sprite-data verification pass).

## 5m. Climbing/window-transition pose selection — ported and verified

Following up on §5h's honest gap (the `in_level_mode != 0` branch of
`update_alley_movement` was a stub). Read `game_loop.asm`'s
`lab_0e91`→`lab_0f33` in full and ported the transition-pose-selection
and animation-timer setup as `update_climb_transition()`.

**Key discovery, verified by direct byte comparison:** the code indexes
`climb_sprite_ptrs`/`climb_sprite_dims` up to entry 5, but those tables'
own labels only span 1 entry (2 bytes) each. Read 6 entries anyway and
compared against the already-extracted `enter_sprite_data`/
`enter_sprite_dims` (§5d) — **entries [1..5] are byte-identical to
`enter_sprite_data[0..4]`/`enter_sprite_dims[0..4]`**. This is a
deliberate memory overlap in the original: two logically-different tables
share the same underlying bytes in different contexts, a common
space-saving trick in tightly-packed 1980s DOS games. Practical
consequence: **no new sprite bytes needed extracting** — `select_vertical_sprite()`
maps directly onto the 6 sprites already in `cat_gap1_sprites.h`
(`climb_sprite` for entry 0, `enter_sprite[0..4]` for entries 1-5).

**Ported:**
- `lab_0e91`/`lab_0eb1`: `in_level_mode==1` (descending into a level) —
  sets `anim_step=0x20`, `transition_timer=8`; if `cat_y` has reached the
  exit threshold (`>=0xb4`) instead snaps back to `in_level_mode=0` and
  returns to plain alley movement.
- `lab_0ec9`: `in_level_mode==-1` (climbing back out toward the alley) —
  decays `scroll_speed` by up to 2, computes `anim_step` via a bit-flip/
  shift (`(original_low_byte ^ 0x0F) << 4`).
- `lab_0ef1`: shared tail — picks the transition pose via the 6-entry
  lookup (direction × in/out state), sets `at_platform=0`,
  `l3_platform_id=0`, and (stubbed) `play_catch_sound()` on door contact.

**NOT ported** (needs level geometry, honestly stubbed rather than
guessed): the actual *trigger* for entering climb mode —
`check_level_collision`, which detects a ladder/pole at the cat's current
position, in the original code above `lab_0e78`. This port still derives
`in_level_mode` straight from `input_vertical` (as established since
§5h), so in this demo the cat can "climb" anywhere, not just at a real
ladder — a known, documented simplification, not a silent gap.

**Unit-tested** (`/tmp/test_climb.c`, `/tmp/test_climb2.c`): verified
`anim_step` against hand-computed expected values for both directions
(`0x20` for descending, matching the original's hardcoded literal;
`0xD0` for climbing out, matching the derived `(2^0x0F)<<4` bit math given
`setup_alley`'s initial `scroll_speed=2`) — both exact matches. Verified
the exit-threshold snap-back triggers exactly at `cat_y>=0xb4` and not
before, and that the `in_level_mode==-1` path (which has no such
threshold in the original) correctly does *not* snap back regardless of
`cat_y`. Real binary smoke-tested for 3 seconds with no crash.

**Demo integration:** `update_alley_movement()` now calls
`update_climb_transition()` for any non-zero `in_level_mode`, draws the
selected transition pose immediately (the original defers to a separate
draw dispatch this port doesn't have wired up yet — drawing right away
was chosen to keep the cat visible rather than match that exact
scheduling), and falls through to the plain-alley path on the same frame
the exit-threshold snap-back fires, so there's no visible drawing gap.

## 5n. `level_objects.asm` — real geometry-gated climb trigger, ported and verified

Went deep on the ladder/platform/door detection system per user request.
`check_level_collision` (the dispatcher `game_loop.asm` calls above
`lab_0e78`) fans out into several functions across `level_physics.asm`
and `level_objects.asm`:

### Data tables — verified

- `door_position_table`/`floor_first_door`/`floor_door_count`
  (DS `0x0ff0`/`0x1006`/`0x100e`): initially misread as (x,y) word pairs;
  re-derived byte-by-byte from `check_door_position`'s real assembly —
  it's **one byte per door** (bit 7 = floor-row flag, bits 0-6 = column),
  sentinel-terminated by a `0x00` byte, and `floor_door_count` isn't even
  consulted by this function (likely used elsewhere, e.g. randomly
  picking a target door). All 8 difficulty lists verified to terminate
  correctly within the table.
- `level_platform_index`/`platform_y_table`/`platform_type_table`/
  `platform_width_table`/`platform_x_left` (DS `0x1269`/`0x1029`/
  `0x1089`/`0x11a9`/`0x10e9`): a shared, index-linked parallel-array
  system — `level_platform_index[level]` gives the starting byte index
  into the other four arrays, scanned until `platform_y_table[i]==0`
  (sentinel). Walked all 8 levels' lists end-to-end (max index used: 94),
  confirmed each terminates cleanly. One adjacent label
  (`l4_platform_offset`, referenced elsewhere in `level_objects.asm` for
  a level-4-specific object Y-lookup) sits within this same byte range —
  consistent with the same deliberate-overlap pattern already established
  in §5m, not chased further since it's tangential to the collision
  trigger itself.

### Functions ported

- **`check_rect_collision`** — AABB overlap test (already commented in
  the disassembly repo itself); decoded register mapping
  (`A={ax,dl,si,cl}`, `B={bx,dh,di,ch}`) confirmed against
  `check_level_platform`'s real call site.
- **`check_door_position`** — level-0 door detection using the
  now-corrected single-byte door table.
- **`check_fence_collision`** — level 3's single hardcoded fence
  rectangle (no table lookup needed).
- **`check_level_platform`** — the main platform scanner. **First
  translation pass got the horizontal test wrong** (guessed a simple
  `x_left <= cat_x < x_left+width` range) — re-read the actual assembly
  after the Y-row match and found the real test is asymmetric: `cat_x`
  rounded down to a multiple of 8 must be `>= x_left`, AND
  `clamp(cat_x - platform_width, 0)` rounded down to a multiple of 4 must
  be `<= x_left`. Also initially miscalculated the level-4-specific
  `l3_platform_id` index by one (added an extra `+1` before the `-0x27`
  that shouldn't have been there) — caught and fixed before it shipped,
  by re-tracing the exact register value at that point in the original
  rather than trusting the first derivation.
- **`check_level_collision`** — the dispatcher. Level 0 (window-sill
  landing) and level 7 (stairs) are **not ported** — both need the
  window-open-state machine (`spawn_window_event`'s domain, still a
  stub) — honestly stubbed rather than guessed.

### The critical polarity correction

First-pass intuition: "finding platform geometry → enter climb mode
(like finding a ladder)." **This is backwards.** Tracing the actual
caller (`game_loop.asm` `lab_0e31`, `jc lab_0e43` after
`call check_level_collision`) shows the opposite: `check_level_platform`
returning true means **solid ground is present here** (stay on normal
footing), and it's the **absence** of a platform — a gap in the geometry
— that triggers automatic climb-mode entry, matching Alley Cat's actual
level design (levels are reached by falling/climbing through gaps
between window ledges, not by touching a dedicated "ladder" object).
Caught before wiring anything up, by deliberately re-reading the caller's
branch direction rather than assuming the first plausible-sounding
interpretation. Getting this backwards would have made the cat climb
everywhere except where it should.

### Verified

Unit tests (`/tmp/test_collision2.c`): 7 scenarios across doors,
platforms, and the fence, all matching hand-computed expected values
(including edge cases that initially failed due to test-input mistakes —
e.g. a door "tolerance zone" wider than assumed, an unaligned platform
edge — re-derived from the real algorithm rather than adjusted to force
a pass). Integration test (`/tmp/test_gated2.c`): standing on a level-4
platform correctly stays in alley mode; standing over a gap in the same
level correctly triggers automatic climb entry.

### Wired in

`update_alley_movement()` now calls `check_level_collision()` for levels
1-6 (excluding 0/7 per the above) before falling to the input-driven
path, replacing the "climb anywhere" simplification from §5h/§5m for
those levels. Levels 0 and 7 still use the input-driven fallback,
clearly documented as such.

## 5o. Dog enemy AI — ported and verified

Went deep on `enemy.asm`'s dog enemy per user request, after `enemy.asm`'s
`check_dog_collision` (referenced as a stub since §5h) turned out to be
misleadingly named — it's actually level-0-specific gravity-fall-landing
detection (using the already-extracted `gravity_sprite_ptrs` from §5i),
not literal cat-touches-dog collision. The real dog AI lives in
`update_enemies` and its helpers.

### A structural mistake caught and fixed mid-port

First translation attempt tried to "understand and restructure" the
state machine's control flow rather than translate it label-for-label —
the same risk flagged (and avoided) back in §5b's `select_cat_sprite`.
This time the risk materialized: the first draft **conflated two
different branches** (mistakenly treated `lab_1ec9`, reached only from
the exit-timer-still-counting-down path, as if it were the
"`enemy_active != 0`" case) and **completely missed** the real
`enemy_active != 0` branch (`lab_1ee2`'s active-case: decrementing
`enemy_chase_delay`, and once it hits zero, either locking `enemy_dir` to
match `enemy_active`'s sign or doing a 50/50 random direction wobble
while still counting down). Caught before compiling further, by
re-reading the complete raw assembly listing line-by-line and redoing the
port as a strict label-for-label goto translation (matching every single
`lab_XXXX` 1:1), the same disciplined method that worked cleanly for
`select_cat_sprite`/`update_cat_movement` back in §5b/§5e. **Lesson
reinforced**: for branchy, unnamed-label assembly with no running
original to test against, translating literally first and refactoring
later (if ever) is safer than trying to infer intent while translating.

### Functions ported (`src/enemy.c`)

- **`update_enemy_sprite`** — pose-index selection; confirms
  `enemy_sprite_table`'s full layout from §5i/§5k (indices 0-3 = 2-frame
  ×2-direction walk cycle, 4-7 = 4-frame "caught the cat" animation).
- **`update_enemy_viewport`** — positions the enemy sprite's draw offset;
  the addressing/positioning half is ported, the actual
  `copy_with_stride` bitmap copy is a no-op stub (enemy bitmap pixel data
  hasn't been extracted to C yet — flagged as future work back in §5i).
- **`draw_enemy`/`erase_enemy`** — background save/restore structure
  ported (reusing verified `blit_to_cga`), actual sprite pixel blit
  stubbed for the same reason.
- **`check_enemy_object_hit`** — throwable-object-vs-dog hitbox test,
  directly reusing the already-verified `check_rect_collision` (now
  exposed publicly in `level_collision.h` for this reuse, matching how
  the original itself shares the one function across systems).
- **`activate_enemy_chase`** — snaps the dog to a position between itself
  and the cat, sets the active/chasing state and computes
  `enemy_chase_delay` from the resulting distance.
- **`check_enemy_activate`** — proximity gate that triggers
  `activate_enemy_chase`.
- **`update_enemies`** — the full per-tick dispatcher: BIOS-tick-gated
  timing, exit-timer countdown (dog leaving after a catch, deducting a
  life), spawn roll (`enemy_spawn_chance`, difficulty-scaled, gated on
  `cat_y`/ground proximity), approach-timer lead-in, chase-vs-patrol
  movement with a difficulty-scaled random chase-trigger roll
  (`enemy_chase_chance`), and boundary-triggered exit-timer arming.

### Verified tables

`enemy_spawn_chance`/`enemy_chase_chance` (DS `0x1cd1`/`0x1cd9`, both 8
bytes, one per difficulty level): `2,4,8,12,16,24,32,64` and
`16,32,48,64,80,96,112,128` respectively — both scale up smoothly with
difficulty, as expected (higher difficulty → dog appears more often and
chases more readily).

### Known simplifications, clearly flagged

- `int 0x1a` (BIOS tick) substituted with a host clock read, same pattern
  used for `cga.c`'s RNG seeding.
- `check_vsync` (original busy-waits for vertical retrace) has no
  equivalent in this port's externally-paced SDL render loop — treated
  as always ready.
- One unidentified single-byte flag (`[0x558]`, checked in several
  spawn/chase gates) is treated as always 0, consistent with this port's
  existing handling of other not-yet-named flags elsewhere.
- Enemy sprite *rendering* is a structural stub — the AI/state machine
  runs for real, but nothing visibly draws yet, since enemy bitmap pixel
  data hasn't been extracted (unlike the cat sprites, which went through
  the full pointer-verification treatment in §3/§4).

### Unit-tested (`/tmp/test_enemy.c`)

Ran `update_enemies` across simulated real time (30 ticks at max
difficulty) and confirmed the dog eventually enters the
approach/chase state with sane resulting position/direction; verified
`check_enemy_object_hit` correctly returns false when the dog isn't
engaged and true when a thrown object overlaps its hitbox while chasing.

### Wired in

`main.c` now calls `update_enemies()` once per frame alongside
`update_alley_movement()` — the state machine runs for real, and (per
§5p below) the dog is now actually visible on screen too.

## 5p. Dog enemy sprite extraction and real rendering

Continuing directly from §5o: extracted the dog's actual sprite bitmaps
and wired real drawing, closing out the "structural stub" noted there.

### Sprites extracted and verified

`enemy_sprite_table`'s 8 meaningfully-used entries (word-offsets 0-14,
already resolved in §5i) resolve to 7 unique frames — slot 7 reuses the
same pointer as slot 5. Size cross-checked exactly against
`update_enemy_viewport`'s base dims (`cx=0xf04` → 4 words × 15 rows =
120B): sorting all unique pointers and diffing consecutive values gives
**exactly 120 for every single gap**, zero exceptions — as clean a
verification as any sprite table in this project. Extracted via
`tools/extract_enemy_sprites.py` → `include/gen/enemy_sprites_ex.h` /
`src/gen_enemy_sprites.c`.

### A second `blit_transparent` bug found and fixed

While wiring `draw_enemy` (which the original passes `bp=enemy_save_buf`
to), re-checked the real assembly and found the §5j fix, while correct
about the *transparency color-key logic* (verified by brute force at the
time), had **dropped a real side effect**: the original also writes each
original destination word into the caller-supplied buffer at `bp` before
overwriting it (`mov word[ds:bp+0],bx`, `add bp,2` each column) — the
exact same background-save pattern `blit_masked` already had via its
`mask_save` parameter. `blit_transparent`'s signature never had an
equivalent parameter. Added `uint16_t *mask_save` to `blit_transparent`,
matching `blit_masked`'s convention (pass `NULL` if unused). No existing
call sites needed updating (nothing in the project called it yet).
Documented as a correction *to* §5j rather than folded silently into it,
since §5j's brute-force verification of the color-key math was and
remains correct — this was a separate, independently-missed detail.

### Real rendering wired in

- **`draw_enemy`** — the `enemy_active==0` (normal walk/chase) path now
  calls the fixed `blit_transparent` with the real extracted sprite data
  and `enemy_save_buf`. The `enemy_active!=0` ("caught the cat"
  celebration) path is **not** ported — it saves/restores a hardcoded
  screen region (`[es:0x1cbd]`) not yet identified in this port; a rare
  state, documented rather than guessed.
- **`erase_enemy`** — restores from `enemy_save_buf` via the
  already-verified `blit_to_cga`.
- **`update_enemy_viewport`** — positioning logic ported faithfully;
  the original's byte-offset-into-bitmap "partial reveal" cropping
  (dog appears to slide/emerge gradually during its approach animation,
  by adding the approach-timer countdown directly into the sprite data
  pointer before a partial `copy_with_stride`) is **not** ported — this
  demo always shows the full, uncropped frame during approach/exit
  states instead. `enemy_sprite_dims` was initially left computed from
  the now-unported crop formula, which would have left it inconsistent
  with the uncropped frame actually drawn — caught and fixed to a fixed
  full-frame value before it could cause a subtle rendering mismatch.

### Verified

Rendered 3 walk-cycle frames through the fixed `blit_transparent` against
a cyan test background: clear, recognizable dog silhouettes with
structural leg-pattern variation frame-to-frame (matching a walk cycle),
not noise. Separately verified the `erase`/restore round-trip: drawing a
frame then restoring from the save buffer reproduces the original
all-white background exactly, confirming the newly-added `mask_save`
side effect works correctly end-to-end with `blit_to_cga`.

### Demo

`main.c` bumped `difficulty_level` to 5 (from 0) so the dog's spawn
chance is visible within a short interactive session, and updated its
intro text accordingly.

## 5q. `check_enemy_activate` wiring bug fixed; `level_objects.asm` fully surveyed

Continuing from §5p, picking up item 11 (`game_loop.asm` remaining dispatch
+ rest of `level_objects.asm`).

### Real bug found and fixed

`alley_movement.c` had its own local `static bool check_enemy_activate(void)
{ return false; }`, left over from before `enemy.asm` was ported (§5o/§5p).
Because it's `static`, it silently shadowed the real, already-ported
`check_enemy_activate()` in `enemy.c`/`enemy.h` (wired correctly into
`update_enemies()`, but NOT into `update_alley_movement()`'s own call to
what it thought was the same function). Net effect: the dog's
"cat-walked-into-me" activation could only ever fire from inside
`update_enemies()`'s internal check, never from the main per-frame alley
dispatch's own `check_dog_collision()`/`check_enemy_activate()` pair
(`game_loop.asm` lab_0f63 area) — silently dropping one of the two call
sites the original has. Fixed: removed the shadow, `alley_movement.c` now
`#include "enemy.h"` and calls the real function. `check_dog_collision`
(the OTHER function in that pair) is correctly left as a stub — per §5o
it's actually unrelated level-0 gravity-fall-landing detection, not dog
collision, and is still genuinely unported. Build verified clean after
the fix (no other call sites referenced the old stub).

### `level_objects.asm` surveyed in full (494 labels, 3861 lines)

Previously only the collision-detection slice used by §5n had been read.
Read through the entire file this session to scope what's actually in it,
rather than guessing from the label list alone. It breaks into five mostly
-independent subsystems, none of which share much code:

1. **Thrown-object mechanics** (`spawn_thrown_object`, `tick_level_thrown_objects`,
   `tick_thrown_objects`, `draw_thrown_sprite`, `erase_thrown_sprite`,
   `check_thrown_cat_hit`, `init_thrown_objects` — lines ~41-628). This is
   the general "cat throws a bottle at the dog" object physics used across
   several levels. Overlaps conceptually with `throw.asm` (not yet
   surveyed) — likely the two need to be ported together rather than
   `level_objects.asm`'s half in isolation.
2. **Level-2 collectible "objects" system** (`check_level_objects`,
   `init_level2_objects`, `erase_level_object`, `reset_caught_objects` —
   lines ~689-900ish). This is the love-scene level's heart/collectible
   pickup logic. Hard-depends on **sound.asm** (`start_tone`, `reset_noise`,
   `update_noise` are called inline, including a real-time noise-burst loop
   gated on the BIOS tick — lines 750-771) and on data tables not yet
   extracted (`l2_obj_x/y/active/hit`, `l1_anim_sprite_c`). Not portable in
   isolation without at least a sound.asm stub-with-timing.
3. **`draw_love_scene_bg`/`draw_bg_tile`** (lines ~209-301): level-2
   background tile rendering, self-contained-ish but needs its own sprite
   table extraction (level-2 background tiles haven't been through the
   §3/§4 rigor pass).
4. **`check_stairs_collision`** (level 7, line 302) — already known-blocked
   on the unported window-state machine (§5n), confirmed still true after
   reading it: it reads `window_open_state`/`current_floor` directly.
5. **Level-7 "love scene" minigame + victory sequence** (`check_l7_cat_hit`
   through `draw_march_frame`, lines ~3258-3861, roughly 600 lines): a
   large, fully self-contained epilogue sequence (cupid, paired-cat
   animation, victory march with its own note-sequencer). Genuinely
   separable from everything else — could be ported standalone once the
   main game loop is solid, without blocking on sound.asm's core square-
   wave synth (it has its own `play_march_note`/`play_victory_march`).
6. **`update_footprint`/`draw_footprint_tile`** (lines 649-687) — small and
   already stubbed as a call site in `alley_movement.c`, but not actually
   portable standalone either: needs `l1_bg_sprite`/`l1_obj_sprites`
   (level-1 footprint decal tiles, unextracted) and `dat_1e00`/`dat_32b6`
   (unidentified data-segment addresses, need resolving via
   `resolve_data_segment.py`'s label list first).

**Conclusion, matching §7's lesson:** none of these six pieces is a quick
mechanical port in isolation — each either needs a new sprite-table
extraction pass (following the §3/§4 methodology) or depends on sound.asm
plumbing that doesn't exist yet. Rather than half-port one and leave it
silently wrong, this session stopped at the survey + the one concrete,
verified fix above. See §6 below for the recommended order to tackle these.

## 5r. `entry.asm` read in full — the REAL master game loop, and a scope correction

Continuing from §5q's `level_objects.asm` survey: while scoping out where
`level_objects.asm`'s thrown-object system (`tick_thrown_objects` /
`check_thrown_cat_hit` / `check_enemy_object_hit`) gets called from, traced
it to `entry.asm`, which turned out to be the actual master game loop —
**not** `game_loop.asm`. `entry.asm` is fully self-commented already in the
disassembly repo (its own header block lays out the whole structure), and
reading it end-to-end resolves a real architectural gap in this port.

### The real structure

`entry.asm` is the top-level loop: hardware init → title/attract screen →
new-game setup → **the alley loop** (`lab_0155`) → on death, a weighted
random level picker → a per-level jump table (`lab_0238`) into 8 near-
identical per-level blocks (init calls, then a `process_keyboard` /
`poll_joystick` / `play_sound` / `update_animation` / level-specific-update
/ exit-flag-check loop), each `jmp`-ing back to the shared alley setup on
exit. This confirms `update_animation` (ported in this project as
`movement.c` + `alley_movement.c`, §5g/§5h) really is the shared per-frame
"cat movement + climb dispatch" called from every single level, including
the alley itself — that part of this port's architecture is correct.

### The real scope correction: level 0's alley has TWO separate mechanics

**This is the important find.** The alley loop (`lab_0155`, entry.asm
lines 143-179) does NOT just call `update_animation`/`update_enemies`
(what this port currently treats as "the whole alley"). Every 4th frame
(or every frame when an enemy is active) it *also* calls a completely
separate chain this port hasn't touched at all:
`update_thrown_objects` → `update_cat_jump` → `apply_cat_gravity` →
`animate_falling` → `cycle_animations` → `draw_lives`.

These live in **`throw.asm`** (`update_thrown_objects` — the classic
"boots/shoes thrown out of windows" hazard the cat has to dodge) and
**`level_physics.asm`**/**`objects.asm`** (`update_cat_jump`,
`apply_cat_gravity`, `animate_falling`, `cycle_animations`,
`check_jump_collision`, `init_player`, `reset_jump`) and **`score.asm`**
(`draw_lives`). None of this is the same code as `update_alley_movement`
— it's a second, independent per-frame system specific to the alley scene:
**jumping over/dodging obstacles**, which is core original Alley Cat
gameplay this port hasn't implemented at all yet. `check_jump_collision`
(already referenced as a stub in `game_loop.asm`'s lab_0e43, per §5n) is
part of this same system, in `objects.asm`, not level_objects.asm — that's
a location correction to earlier notes.

Also clarifies the earlier "thrown-object" survey in §5q: there are
genuinely **two unrelated thrown-object systems**, not one:
- `throw.asm`'s `update_thrown_objects` — shoes/boots falling from
  windows onto the cat in the alley (level 0 only, tied to the window
  state machine for spawn position).
- `level_objects.asm`'s `tick_thrown_objects`/`check_thrown_cat_hit` —
  a bouncing prop (milk bottle?) used as an obstacle *inside* the levels
  1,3,4,5,6 sub-games, already collision-tested against both the cat and
  the enemy (`check_enemy_object_hit`, already ported in `enemy.c` but
  currently has no caller since nothing spawns/moves `thrown_obj_x/y` yet).

`check_window_landing` (`level_physics.asm`) was also read this session:
confirmed it genuinely is fully readable/simple in isolation (a bitmask
test against a per-row window-open bitmap), but it reads `window_column`/
`current_floor`/`window_row_offset`, which are only ever written by the
still-unported window spawn/animation state machine — so §5n's "blocked
on window state machine" characterization for levels 0/7 stands, now
confirmed by reading the actual function rather than inferring it.

### Not ported this session (scoped, not guessed)

Given the size of what §5r uncovered (`level_physics.asm` 547 lines,
`objects.asm` 449 lines, `score.asm` 382 lines, `throw.asm` 282 lines —
four more files, on top of the six `level_objects.asm` subsystems from
§5q), no new C code was written this session beyond §5q's bug fix.
Writing the jump/gravity mechanic without reading `objects.asm`'s
`cycle_animations`/`check_cycle_cat_collision`/`check_cycle_gravity_hit`
(which look related — possibly a bicycle/motion-obstacle system riding
alongside the jump mechanic, not yet distinguished) risked exactly the
kind of half-understood, rushed port §7 warns against.

## 5s. Alley's real per-frame systems disambiguated; patrol-object system (`objects.asm`) ported and verified

Continuing from §5r's item 17(a)/(b): read `level_physics.asm` (546 lines)
and `objects.asm` (462 lines) in full to finally disambiguate the systems
§5r had bundled together under one "jump/gravity/obstacle-dodge" label.

### Correction: it's 3 unrelated systems, and the cat never jumps

§5r's phrasing implied the *cat* jumps. Reading the actual code shows
that's wrong — the cat has no jump action in the alley at all. What's
really there, all called from `entry.asm`'s alley loop (`lab_0155`) and
nowhere else:

- **`update_cat_jump`/`apply_cat_gravity`** (`level_physics.asm`) — despite
  the name, this is an **enemy**: a fish-creature that leaps from a ledge
  and throws a projectile in an arc at the cat (`gravity_x`/`gravity_y`
  track the projectile, not the cat). Its sprite data
  (`gravity_sprite_ptrs`/`gravity_sprite_dims_tbl`) was already extracted
  and verified back in §5i under `enemy_verified_sprites.h`, unrecognized
  at the time as belonging to this system.
- **`animate_falling`/`check_jump_collision`** (`level_physics.asm`) — a
  real "obstacle-dodge" mechanic: objects fall from window height to a
  random door/ledge target (`pick_random_target`) and the cat must avoid
  them. `fall_sprite`'s dims are computed dynamically per-frame (`mov
  bx,dat_1b02`/`mov bx,0x2` then `add`/`sub bh,[fall_counter]` — the
  *label's own numeric address* is used as a base value, then modified),
  not read from a lookup table, so this needs more care before porting —
  **left unported this session**, scoped honestly rather than guessed.
- **`init_objects`/`cycle_animations`** (`objects.asm`) — **the alley's 3
  ground-patrol objects (rats/mice** — the walk-cycle sprite's silhouette,
  ASCII-rendered below, confirms small quadruped, matching §5l's original
  read of `cycle_walk_sprite`). Fully independent of the other two systems
  above (no shared state) — **ported and verified this session**, see
  below.

### `objects.asm` ported: `init_cycle_objects`/`update_cycle_objects`

New files: `include/cycle_objects.h`, `src/cycle_objects.c`. Literal,
label-for-label translation of `init_objects` and `cycle_animations`
(round-robin per-tick dispatcher across 3 object slots), plus
`check_cycle_cat_collision`, `check_cycle_gravity_hit`, and
`erase_cycle_sprite`.

Behavior: each of the 3 slots independently idles, patrols left/right, or
(when at a platform and near the cat's height) breaks into a faster chase
toward the cat, with randomized direction reversal. On cat contact:
- If `at_platform` is set: the cat gets knocked back — the object flips
  to fleeing rightward, plays its dedicated hit-reaction sprite briefly,
  awards points (`obj_score`), and a tone (score/sound not ported yet, so
  both are named stubs, same convention as the rest of this port).
- If not at a platform: arms the level-2 climb-transition state
  (`in_level_mode`/`transition_timer`) and draws the "collision" splash
  sprite — this reuses `check_level_collision`'s already-verified
  `collision_sprite` (§5n/§5l).

**Bug caught during translation**: an early draft of the "not at a
platform" branch re-blitted `collision_sprite_frame.data` to restore the
background, instead of restoring from the buffer that
`blit_transparent`'s `mask_save` parameter actually saved — that would
have just redrawn the splash sprite on top of itself instead of erasing
it. Fixed by giving that branch its own real backing buffer
(`collision_save_buf`), same pattern as `obj_save_buf`/`enemy_save_buf`.

### New sprite extraction — and it resolves §5l's open question for real

`tools/extract_obj_hit_sprites.py` extracts `obj_hit_sprite[3]` — a
dedicated 32-byte (2w×8h) knockback pose per patrol object, drawn via
`blit_transparent` on a hit. Its pointer table (DS `0x1f5f`, 3 words)
resolves to `0x1e10`/`0x1df0`/`0x1dd0` — **exactly the region §5l flagged
as `collision_sprite`'s unexplained "28-byte tail."** Sorting those 3
pointers plus `cycle_idle_sprite`'s start address shows each gap is
*exactly* 32 bytes with zero slack, running straight from
`collision_sprite`'s verified end (`0x1dd0`) to `cycle_idle_sprite`'s
start (`0x1e30`) — the same disassembler-mislabeled-boundary pattern as
every other case in this project (§3/§5i/§7), now fully closed rather
than "deliberately left open."

While cross-checking this, also caught a stale reading of `obj_save_buf`:
it isn't a single 12-byte/6-pointer table overlapping `obj_hit_sprite` as
first read — re-deriving from the call site's `shl bl,1` (word-indexed,
3 slots) shows `obj_save_buf` is only the *first* 6 bytes at DS `0x1f59`
(3 words: `0x1ed0`/`0x1ef0`/`0x1f10` — literal DOS scratch-RAM addresses,
not meaningful in this port, so real backing arrays are used instead),
and `obj_hit_sprite`'s own table starts immediately after at `0x1f5f` —
again a clean, zero-gap boundary once corrected.

Also verified against the resolved data segment and hardcoded directly
(all 3 are tiny, fixed tables): `obj_y` initial values `{0x00, 0x20,
0x40}`, `obj_score` `{9, 7, 5}`, and `obj_chase_table` (indexed by
`difficulty_level`) `{8, 32, 64, 128, 160, 192, 208, 240}`.

### Verification

- Build is clean (`make`, zero warnings), binary runs without crashing.
- New standalone unit test (`/tmp/test_cycle.c`, 6 scenarios): spawn-side
  selection by `cat_x`, 500-tick free-run bounds check (objects never
  leave `[0, 0x12e]`), a forced knockback-hit scenario (`at_platform=1`),
  a forced climb-transition scenario (`at_platform=0`), and confirming
  `transitioning != 0` fully gates the whole update (matching the
  original's early-return). All 6 pass.
- ASCII-rendered both `cycle_walk_sprite_frames[0]` (via `blit_masked`)
  and the newly-extracted `obj_hit_sprite_frames[0]` (via
  `blit_transparent`) against a test background — both show coherent,
  non-garbage silhouettes, not noise.
- Wired into `main.c`: `init_cycle_objects()` is called alongside
  `setup_alley()` (matching `entry.asm`'s `lab_0140`, where `init_objects`
  is called right after `setup_alley`/`setup_level`), and
  `update_cycle_objects()` runs every frame in the main loop alongside
  `update_alley_movement()`/`update_enemies()`.

### Still open

- **`update_cat_jump`/`apply_cat_gravity`** (fish-jump enemy + thrown
  projectile) — surveyed and disambiguated this session, not yet ported.
  Its sprite data is already extracted (§5i); the AI/collision logic
  (`check_fish_collision`, `check_trashcan_near`,
  `decode_enemy_params`) still needs a full read.
- **`animate_falling`/`check_jump_collision`** (falling window objects) —
  surveyed, not yet ported; `fall_sprite`'s dynamically-computed dims
  need more work before this can be done with the same rigor as
  everything else in this project.
- `score.asm`/`sound.asm` — still stubbed everywhere they're called from
  (including the new `cycle_objects.c` stubs added this session).

## 5t. Fish-jump/gravity-toss enemy — ported and verified

Followed up directly on the roadmap item flagged in §5s/earlier as "next
up, not yet ported": `level_physics.asm`'s `apply_cat_gravity`/
`update_cat_jump` plus `enemy.asm`'s `check_fish_collision`/
`check_trashcan_near`/`decode_enemy_params`. This is a **third**, separate
enemy-like system from the dog (`enemy.c`) and the 3 ground-patrol
objects (`cycle_objects.c`): a creature that leaps from a ledge near one
of the patrol objects, pauses, then throws a falling projectile at the
cat.

### Functions ported (`src/jump_gravity.c`)

- **`decode_enemy_params`** — splits a packed spawn-parameter byte into
  an (x,y) spawn position via `jump_spawn_x_table`/`jump_spawn_y_table`
  (4 entries each, verified: x=`24,104,184,264`, y=`24,56,88,24`).
- **`check_fish_collision`** — reuses the already-verified
  `check_rect_collision`; also latches `door_hit_flag` (the original's
  unnamed `byte[0x551]`) under specific extra conditions once a collision
  is found.
- **`check_trashcan_near`** — tests the fish's landing spot against
  `obj_x[1]` or `obj_x[2]` (never `obj_x[0]`, matching the original
  exactly) depending on a bit of the spawn parameter — this is what ties
  the fish's jump progression to a nearby patrol object being present.
- **`update_cat_jump`** — the main per-tick dispatcher: random spawn roll
  (rarer than the dog's, and further gated by an "idle too long" boost
  using a partially-identified BIOS-tick reference), jump-arc animation,
  and — once parked near a patrol object long enough (a real-time delay
  from `jump_pause_by_diff`, verified: `45,36,27,18,9,18,1,18` ticks,
  scaling down with difficulty) — arming the toss.
- **`apply_cat_gravity`** — animates the thrown projectile: horizontal
  drift toward/away from the cat, accelerating vertical fall toward a
  per-toss random target height (`gravity_height_table`, verified:
  `97,100,94,94`), landing (restoring the background via the
  already-verified `blit_to_cga`) once the target is reached. Draws via
  the now-fixed `blit_transparent` using the `gravity_sprite[4]` frames
  already extracted back in §5i.

### Documented simplifications (not guessed)

- The original's `check_dog_collision` call inside `apply_cat_gravity` is
  — per §5o's finding — actually level-0-specific gravity-fall *landing*
  detection, not the dog. Not ported; this port always proceeds with the
  normal fall/landing logic instead, correct for the vast majority of
  cases.
- The fish's own jump-arc sprite (as opposed to the thrown projectile's
  `gravity_sprite` frames, which are drawn) hasn't been extracted — a
  distinct sprite category from death/gravity/enemy/cycle. The animation
  state (`jump_anim_counter`, `jump_draw_y`) progresses correctly, but
  nothing draws for the fish itself yet, only the projectile once tossed.
- Two unidentified single-byte flags (`byte[0x418]`, `byte[0x556]`) are
  treated as always 0, consistent with this port's handling of other
  not-yet-named flags elsewhere.

### Verified

Unit-tested in stages, since the state machine has two independent
timing systems (a call-count gate `jump_tick_delay`, and a wall-clock
gate for the toss delay) that made naive tight-loop testing initially
misleading:
1. Confirmed spawning fires at the expected rough rate and
   `decode_enemy_params` produces correct table values.
2. Confirmed `check_trashcan_near` correctly blocks progression when no
   patrol object is nearby (initially looked like a stuck/broken
   animation counter — turned out to be entirely correct behavior once
   traced through, matching the original's design of tying fish progress
   to patrol-object proximity).
3. With a targeted test forcing the exact pre-toss state (bypassing spawn
   randomness), confirmed the toss arms correctly:
   `gravity_y` set to exactly `jump_y`, `gravity_x` randomized near
   `jump_x` with the expected spread, drift direction correctly pointing
   toward the cat.
4. With real timing (`usleep` between ticks, matching the tick-gated
   design), confirmed the full fall arc: `gravity_y` accelerates smoothly
   (`41→42→44→46→49→52→56→60→65→70→76→82→89`), `gravity_x` drifts at a
   constant rate toward the cat, and it lands cleanly (`gravity_y` resets
   to 0) exactly when the target height is reached — a complete,
   physically sensible spawn→jump→toss→fall→land cycle.

### Wired in

`main.c` now calls `update_cat_jump()` then `apply_cat_gravity()` once
per frame, after the dog and patrol-object systems.

## 5u. Falling-object dodge mechanic — ported and verified

Followed up on roadmap item (c), previously flagged as needing "more
care" because `fall_sprite`'s dims looked dynamically computed. Turned
out to be one of the cleanest ports in this whole project once traced
correctly — `objects.asm`'s `reset_jump`/`animate_falling`/
`erase_jump_sprite`/`check_jump_collision`, plus `level_physics.asm`'s
`pick_random_target`.

### The `dat_1b02` red herring

`mov bx,dat_1b02` initially looked like a memory read of a table value.
It isn't — in NASM, `mov bx,label` (no brackets) loads the label's own
**address** as an immediate constant, not the data stored there. The
disassembler had generated a spurious label at that exact byte offset
simply because the numeric literal `0x1b02` happened to look like a
valid in-segment address. Decoded as a plain constant: low byte
(width) = `0x02`, high byte (height) = `0x1b` = 27 — exactly matching
`fall_counter`'s spawn value. Worth remembering alongside the earlier
"width is in words" and "labels don't reliably mark boundaries" lessons:
**not every label is real data — some are the disassembler
misinterpreting an immediate operand as an address.**

### The height-varying draw — simpler than feared

The actual mechanic: `fall_sprite` is a **fixed** 2-word-wide sprite;
what varies frame-to-frame is only the **row count** passed to the blit
(1→13 while "rising" into view, 13→0 while "falling" back out), always
reading from the same fixed base pointer. This is mechanically much
simpler than the dog's unported partial-reveal effect (§5p), which
offsets the source *pointer* into the bitmap — here only a plain height
parameter changes, no pointer arithmetic into sprite data needed.
Verified `fall_sprite`'s total extent (52 bytes = 2 words × 13 rows, the
max height ever used) against the next label with **zero slack**.

### Verified

Unit-tested (`/tmp/test_fall.c`): forced a spawn via the special-ledge
`cat_y` trigger, then watched the full arc — height climbs
`1,2,...,13` then descends `13,...,1,0` in perfect mirror symmetry, with
`fall_cur_y` tracing a matching symmetric rise-then-fall
(`158→146→159`), a convincing parabolic-looking drop. Collision
detection (`check_jump_collision`, reusing the already-verified
`check_rect_collision`) correctly fires when overlapping and correctly
doesn't when far away, with `fall_hit` latching as expected both ways.

### Wired in

`main.c` now calls `animate_falling()` once per frame, after the fish-
jump/gravity-toss system.

## 5v. Score/lives HUD — ported and verified

`score.asm`'s core visible-HUD subsystem: BCD score storage/arithmetic,
the shared digit sprite sheet, and the lives counter. The larger
level-background-tile-drawing functions in the same file
(`draw_level_background`/`draw_block_pair`/`draw_door_frame`/
`draw_platform`/`draw_ledge`/`draw_level_border`/`draw_vertical_line`)
are a separate, bigger subsystem not covered by this pass.

### Two labeling issues found and resolved

- **`draw_score`/`draw_high_score`'s names are swapped relative to their
  content.** `update_high_score`'s own comment clearly states the score
  layout (`current` at one offset, `high` at another); cross-checking
  against that, the function *labeled* `draw_score` actually reads the
  high-score buffer and draws to the high-score screen position, and
  vice versa for the one *labeled* `draw_high_score`. This port's C
  function names (`draw_current_score`/`draw_high_score_display`) follow
  the **content**, not the possibly-swapped original labels — documented
  explicitly in `score.h` rather than silently perpetuating the mix-up.
- **`add_score`'s `lives_display`-based indexing is an address-arithmetic
  artifact, not really about lives.** `lives_display` (a real, distinct
  byte used elsewhere for the lives-counter redraw cache) sits exactly
  one byte before the current-score buffer in the original's memory
  layout; `add_score` uses it purely as a convenient base pointer with an
  offset of 1-6, landing squarely on the real score buffer. This port
  just operates on `current_score` directly — no need to replicate the
  memory-adjacency trick since this port doesn't share one flat address
  space between unrelated variables.

### A second self-caught BCD arithmetic bug

Also found: the CGA screen-position "labels" (`lives_cga_pos`,
`high_score_cga_pos`, `current_score_cga_pos`) are the same
address-as-immediate-constant pattern already identified for `dat_1b02`
in §5u (`mov di,label` with no brackets loads the label's own address,
not data at that address) — confirmed in-range for `CGA_MEM_SIZE` and
used directly as plain offset constants in this port, no data table
needed.

While porting the BCD carry-propagation loops (`add_score`/
`add_bcd_scores`), a first draft tried to hand-approximate x86's `AAA`
instruction by checking "is the low nibble > 9," missing that `AAA`'s
real trigger condition also includes the **auxiliary carry flag** — which
fires for cases like `9+9+1=19`, whose low nibble (3) isn't `>9` despite
genuinely needing a carry. Caught by direct unit testing (§ below) rather
than by inspection. Since these buffer bytes are **unpacked decimal
digits** (0-9 each, not packed hex nibbles with other bits present), the
correct and much simpler equivalent is just "if sum ≥ 10, subtract 10 and
carry 1" — verified against the exact 19-case and several
multi-digit-cascade cases.

### Verified

Unit-tested (`/tmp/test_score.c`), 8 scenarios: single-digit add,
single-carry add, multi-digit cascade (`0000099+9=0000108`), the
specific `9+9=19` edge case that the flawed first draft would have
gotten wrong (`0000099+0000099=0000198` — passes), a 6-digit cascading
carry (`0999999+0000001=1000000` — passes), `update_high_score` correctly
updating when current > high and correctly leaving high unchanged when
current < high, and `draw_lives`'s change-detection cache.

### Extracted

`digit_sprites` (10 shared glyphs, 0-9, 1 word × 8 rows = 16B each, used
by both the score display and the lives counter) via
`tools/extract_digit_sprites.py`. A further 224 bytes follow before the
next real label with no clean explanation found yet (possibly additional
glyphs like blank/dash, not consulted by any code read so far) — not
chased further, low value relative to the rest of the roadmap.

### Wired in

`main.c` initializes `lives_count=3` and clears both score buffers at
startup, then draws the HUD (lives, current score, high score) every
frame. Score stays at 0 throughout this demo since no gameplay event
currently calls `add_score()` — none of the scoring triggers (defeating
an enemy, collecting an item, etc.) are wired up yet, which would need
those respective systems' collision/pickup logic completed first.

## 5w. Level room background system — ported and verified for 2 of 7 levels

Went deep on `draw_level_background` per user request, after finding it's
not (as the roadmap had it) simple score-bar decoration — it's the full
per-level room background renderer (border, doors, platforms, ledges,
decorative tiles), plus a completely separate randomized block-puzzle
background for level 2 and a delegation to `draw_love_scene_bg` (the
level-7 victory epilogue, roadmap item (e)) for level 7.

### A different, more efficient extraction strategy

`draw_block_list` (the core tile-blitting primitive, in
`alley_drawing.asm`) addresses its source tile data via **arbitrary
absolute DS offsets** baked directly into each level's tile-list tables —
not through a clean per-sprite pointer table like every other sprite
category in this project. Given the source offsets are scattered
throughout the segment, extracting each tile individually by name isn't
practical. Instead: embedded the **entire verified data segment**
(the same 28,976-byte byte-exact reconstruction from
`resolve_data_segment.py`, used throughout this whole project as ground
truth) as one C array (`tools/embed_data_segment.py` →
`include/gen/ds_pool.h`), and `draw_block_list` indexes into it with the
exact same absolute offsets the original assembly uses. This sidesteps
needing dozens of new per-tile extraction scripts while still being
byte-exact — a different, less labor-intensive verification strategy
than the "resolve pointer table → cross-check deltas → extract named
sprite" pattern used everywhere else, appropriate because this data
genuinely isn't sprite-shaped the same way.

### A new blit primitive: `blit_bytes_to_cga`

`draw_block_list` uses `REP MOVSB` (byte copy) rather than `REP MOVSW`
(word copy) — the first place in the whole codebase where this shows up.
Every other `blit_*` function in this project takes a WORD count (§4);
this one genuinely needs a raw BYTE count, since background tiles don't
need CGA's word-aligned sprite conventions. Added `blit_bytes_to_cga` as
a byte-granular sibling to `blit_to_cga` in `cga.c`, clearly documented
as the one exception to the width-is-words rule established in §4 — so
future readers don't assume it's a bug.

### Two more labeling issues found and fixed mid-port

- **A whole function (`draw_block_pair`) was missing from the first
  draft**, and its real job (`block_pair_list_a`/`block_pair_list_b`) had
  been incorrectly assigned to `draw_door_frame` instead. Re-reading the
  actual assembly showed `draw_door_frame` really indexes `[si+draw_temp]`
  — nothing to do with `block_pair_list_a`/`_b` at all.
- **`draw_door_frame`'s real list-pointer array has no name of its own.**
  It's 4 words living immediately after `draw_temp` in memory
  (`0x2636`-`0x263c`), never given a separate label by the disassembler —
  verified as plausible in-range DS offsets before trusting this
  (consistent with the same kind of unlabeled-adjacent-data pattern seen
  repeatedly throughout this project, e.g. §5m's `climb_sprite_ptrs`
  overlap). Both mistakes were caught by re-reading the raw assembly
  line-by-line rather than trusting an initial skim — the same lesson as
  ever: verify against the actual instructions, not a paraphrase or a
  first impression of what a function "should" do by name/position.

### Functions ported (`src/level_background.c`)

`draw_block_list`, `draw_vertical_line`, `draw_level_border`,
`draw_block_pair`, `draw_door_frame`, `draw_platform`, `draw_ledge`, and
the top dispatcher `draw_level_background` — fully wired for **levels 2,
5, and 6**. Level 2's randomized block-puzzle background (40 tiles picked
from 4 types, never repeating the immediately-previous type, using the
shared RNG) is ported in full. Levels 0, 1, 3, 4 were not reached in this
pass (the dispatcher's branches for them exist in the original but
weren't read/ported yet) and level 7 correctly no-ops (delegates to the
separately-scoped victory epilogue).

### Verified

Rendered levels 5 and 6's backgrounds and inspected the raw CGA
framebuffer: clear, structured, non-random content in both — a
decorative corner/checkerboard motif, a vertical divider line, a solid
platform bar, and a repeating brick/window tile pattern, exactly what a
room background should look like (not noise). Non-zero byte counts
(1632 and 2200 respectively) confirm real content was drawn, not an
empty/failed pass.

### Wired in

`setup_level()` now calls `draw_level_background()` at level entry. This
integration point is this port's own reasonable choice, not a directly-
traced original call site — `score.asm` itself never calls
`draw_level_background`; something in the not-yet-fully-read parts of
`game_loop.asm`/`alley.asm` does, and that exact call site wasn't
separately confirmed in this pass.

## 6. Immediate next steps (in order)

1. ~~Decode Pool A's bitfield frame-selection logic~~ — **done, see §5b.**
2. ~~Confirm the `blit_masked` silhouette hypothesis~~ — **done, confirmed:
   pure black silhouette, no sprite-driven color, walk-cycle-consistent
   leg movement across frames. See §5b.**
3. ~~Wire real rendering into `main.c`~~ — **done, see §5c.**
4. ~~Investigate the two unaccounted-for gaps~~ — **gap 1 fully resolved
   (recoil/entering-window/climbing sprites, see §5d); gap 2 characterized
   as likely mask-plane data but consumer not yet found — open, revisit
   with `level_objects.asm`.**
6. Real movement physics: ~~done, see §5e~~ — **CORRECTION per §5g: this
   turned out to be level-2 "drowning" physics specifically, not general
   movement.** `level_physics.asm`'s actual content (thrown-object
   gravity, enemy jump arcs) is separately still unported — revisit
   alongside `enemy.asm`.
7. ~~Find and port the REAL general-movement dispatch chain~~ — **done,
   see §5h.** `update_walk_frame`/`update_alley_movement` ported, verified,
   and wired into `main.c` as the primary demo path.
8. ~~Port the climbing/window-transition branches~~ — **done, see §5m.**
9. ~~Port the real `check_level_collision`-gated ladder-entry trigger~~ —
   **done for levels 1-6, see §5n.** Levels 0/7 still use the
   input-driven fallback (need the unported window-state machine).
   Still needed: implement `save_alley_buffer`/`restore_alley_buffer` for
   real (currently both stubs/inert — see §5f/§5h), which will let the
   last-frame-redraw demo workaround in `alley_movement.c` be removed.
10. ~~`check_dog_collision`/`check_enemy_activate` (`enemy.asm`)~~ —
    **done for the AI/state-machine side, see §5o.** Enemy sprite bitmap
    data still not extracted (rendering is a stub). `update_footprint`/
    `spawn_window_event` (`level_objects.asm`/`alley.asm`) still stubbed.
    Level 0's `check_jump_collision`/window-landing semantics and level
    7's `check_stairs_collision` also still open (§5n) — both really
    `check_dog_collision`'s ORIGINAL literal name turned out to refer to
    a different, level-0-specific gravity-fall mechanic (§5o), not the
    dog at all.
11. `game_loop.asm`'s remaining core dispatch (scoring/collision triggers,
    level-transition logic beyond what §5h/§5n covered) and the rest of
    `level_objects.asm` (3861 lines total — only the collision-detection
    slice used by §5n has been read so far), the rest of `enemy.asm`
    (enemy sprite bitmap extraction, `decode_enemy_params`/
    `check_fish_collision` not yet read), `sound.asm` (PC speaker → SDL
    audio square-wave synthesis), `ui.asm`, `score.asm`, `throw.asm`.
12. ~~Sprite-category verification across all 6 remaining categories~~ —
    **done, see §5i/§5k.** Extraction to C headers still pending for:
    enemy sprites (8 pointers resolved, bitmap bytes not yet pulled),
    `extralife_sprites`/`title_sprites` (fully verified, ready to extract
    — same script pattern as `extract_death_sprite.py` applies directly),
    `cycle_idle_sprite`/`cycle_walk_sprite`/`collision_sprite` (mostly
    verified, two small unexplained gaps to resolve first).
13. ~~Fix `blit_transparent` for real~~ — **done, see §5j.** Correctly
    understood as black-is-transparent color-keying, verified by brute
    force, `death_sprite` extracted and visually confirmed.
14. ~~Resolve the two remaining small gaps from §5k~~ — **done, see §5l.**
    `cycle_walk_sprite`'s 96 bytes confirmed as legitimate zero padding;
    `collision_sprite`'s 28-byte tail characterized but deliberately not
    chased further (low value). All object sprites now extracted.
15. ~~Port the real `check_level_collision`-gated ladder-entry trigger~~ —
    **done for levels 1-6, see §5n.** Levels 0/7 still use the
    input-driven fallback (need the unported window-state machine).
    Still needed: implement `save_alley_buffer`/`restore_alley_buffer` for
    real (currently both stubs/inert — see §5f/§5h), which will let the
    last-frame-redraw demo workaround in `alley_movement.c` be removed.
16. ~~Port the dog enemy AI state machine~~ — **done, see §5o.** ~~Extract
    enemy sprite bitmap data~~ — **done, see §5p.** ~~Wire real rendering
    into `draw_enemy`/`erase_enemy`/`update_enemy_viewport`~~ — **done,
    see §5p.** ~~Fix `check_enemy_activate` main-loop wiring~~ — **done,
    see §5q.**
17. **CORRECTED per §5r, then per §5s**: `throw.asm`'s `update_thrown_objects`
    and `level_objects.asm`'s `tick_thrown_objects` are NOT the same system —
    see §5r for the split. §5r's item (a)/(b) ("jump/gravity/obstacle-dodge
    mechanic") turned out to bundle 3 unrelated systems — see §5s for the
    full disambiguation and corrected scope:
    (a) ~~`objects.asm`'s `init_objects`/`cycle_animations` (the alley's 3
    ground-patrol rats/mice)~~ — **done, ported and verified, see §5s.**
    (b) ~~`level_physics.asm`'s `update_cat_jump`/`apply_cat_gravity`~~ —
    **done, ported and verified, see §5t.** A fish-creature enemy that
    jumps from a ledge near a patrol object and throws a projectile at
    the cat. The fish's own jump-arc sprite still isn't extracted (only
    the thrown projectile draws); real-time-gated toss logic verified
    end-to-end.
    (c) ~~`objects.asm`'s `animate_falling`/`check_jump_collision`~~ —
    **done, ported and verified, see §5u.** Turned out simpler than
    feared — the "dynamic dims" were just a variable row-count, not
    pointer arithmetic; also caught a labeling red herring (`dat_1b02`
    was a disassembler artifact from an immediate operand, not real data).
    (d) ~~`level_objects.asm`'s `tick_thrown_objects`
    prop system (needs a new sprite extraction pass for `l1_sprite_data`/
    `dat_3260`'s pointer table, `l1_bg_sprite`, `l1_obj_sprites`,
    `l1_anim_sprite_a/b/c` — none yet through the §3/§4 rigor pass)~~ —
    **done, see §5z** (turned out `l1_anim_sprite_a/b/c` belong to a
    different, still-unported system; no separate extraction pass was
    actually needed, same as §5x/§5y — just direct `ds_pool` offsets);
    (e)
    ~~the level-7 victory/love-scene epilogue (self-contained, ~600 lines)~~
    — **done, see §6a-§6d** (ported incrementally in 5 checked-in chunks);
    (f) level-2 collectibles + bg tiles (needs sound.asm); (g) ~~footprint
    decals~~ **done, see §5z** (turned out to share state with (d) rather
    than being separable); (h) `check_stairs_collision`/`check_window_landing`, both
    confirmed (§5r) genuinely blocked on the unported window spawn/
    animation state machine (`spawn_window_event`, `alley.asm`) rather than
    on anything unclear in their own logic. (i) ~~level 3's own bird-enemy
    subsystem (`check_fence_collision`/`init_level3_enemy`/
    `update_level3_enemy`, same file, separate from the fence-tile
    *background* which §5x now covers)~~ — **done, see §5y.**
18. ~~`score.asm`~~ — **the core score/lives HUD (`draw_lives`,
    `draw_current_score`/`draw_high_score_display`, `add_score`/
    `add_bcd_scores`) is done, see §5v.** `draw_level_background` turned
    out NOT to be simple score-bar chrome as originally thought here — it's
    the full per-level room background system (border/doors/platforms/
    ledges/tiles), ported for levels 2/5/6, see §5w. ~~Levels 1/3/4~~ —
    **done, see §5x.** The level-7 victory epilogue (`draw_love_scene_bg`)
    still remains.
19. ~~`sound.asm`~~ — **done, see §6e.** Full port plus an emulated PC
    speaker/PIT and an SDL2 audio backend; every stubbed sound call site in
    the project is wired to the real routine. This unblocks §17 item (f)
    (level-2 collectibles), which was waiting on nothing else. Remaining
    external blocker for item (h) is still `alley.asm`'s window state
    machine. `render_sprites` (a drawing routine that happens to live in
    sound.asm) deliberately left for the alley-drawing work — see §6e.
20. ~~`alley.asm`~~ — **done, see §6f.** The background save/restore
    pipeline is real in all four files that stubbed it, the per-frame screen
    wipe and the last-frame-redraw demo hack are gone, and
    `spawn_window_event`/`enter_building`/`handle_cat_death` are ported.
    **Corrects items 9/15/17(h) above**: those said item (h) was blocked on
    `spawn_window_event`. It is not — it is blocked on `throw.asm`
    (`current_floor`/`window_column`) and `ui.asm` (`window_open_state`).
    `update_viewport` is the only unported routine left in `alley.asm`.

## 5x. `draw_level_background` — levels 1, 3, 4 ported; source repo re-acquired

The `alleycat-disassembly-main` source repo (this whole project's ground
truth) had gone missing from the working directory between sessions — it
had to be re-uploaded before any further work could safely proceed,
per §7's rule about never porting logic without verifying against the
real assembly. Re-ran `resolve_data_segment.py` against the re-uploaded
repo and confirmed it reproduces the exact same 28976-byte data segment
already embedded in `include/gen/ds_pool.h`/`src/gen_ds_pool.c` (byte-exact
diff-checked) — so no data re-extraction was needed, only new offset
constants and new control-flow logic, straight off `ds_pool`.

**Correction to §5w's framing:** `draw_level_background` is never called
with `level_number==0` in the original at all — the alley isn't one of
score.asm's dispatched cases. `entry.asm`'s level-select jump table routes
BOTH indices 0 and 1 to a single `level_number=1` handler; the alley (level
0) uses an entirely separate `setup_alley` path. So "level 0" was never
actually a remaining case — the only real gap was level 1 (the dispatcher's
default/fallthrough branch), plus levels 3 and 4. Added a defensive
`level_number<=0` early-return to `draw_level_background` and corrected the
long-standing "uncertain call site" comment in `game_setup.c`'s
`setup_level` now that `entry.asm` confirms every one of its 7 per-level
branches calls `draw_level_background` right after `level_transition`.

**Level 1** — pure reuse of already-ported primitives
(`draw_level_border`/`draw_block_list`/`draw_door_frame`/`draw_platform`/
`draw_ledge`/`draw_block_pair`), just new offset constants. No new data.

**Level 3** — same reused primitives, plus a new `draw_level3_bg`: a
16-col x 16-row randomized fence-tile grid (`level_objects.asm`'s routine
at the end of the level-3 branch). Verified tile-table size (7 variants x
16 bytes = 0x70) exactly matches `dat_3730`'s span to the next label, and
column-0/last-column are fixed end-cap tiles while interior columns pick
from a small RNG-driven set. Ported as a literal control-flow translation
(not restructured) specifically to preserve the exact `random()` call
sequence per column — the original's branch structure calls `random()`
either 0 or 1 times before potentially calling it again, so a "simplified"
rewrite would desync the shared LFSR stream from everything else that
consumes it. `check_fence_collision`/`init_level3_enemy`/
`update_level3_enemy` (a separate, not-yet-ported level-3 bird-enemy
subsystem living in the same file) intentionally left untouched — out of
scope for background-only work.

**Level 4** — reused primitives plus a new `init_level4_bg`, ported only up
through its background-drawing portion (17-row randomized floor pattern +
4 static block-lists + one small masked decorative sprite). Deliberately
NOT ported: the routine's tail, which seeds a per-window light/gap random
state table (`dat_3ce3`/`dat_3ce4`/`dat_3cf3`/`dat_3cf4`) from
`[difficulty_level]` — that state is only read by a level-4 object-update
routine at `level_objects.asm:1762` that isn't ported yet, so seeding it
now would be dead code. Same deferral pattern already used for
`init_level5_objects`/`init_level6_objects` elsewhere in this dispatcher.
Confirmed `[0x8]` in `mov bx,[0x8]` is `difficulty_level` (labeled in the
resolved data segment), not an unexplained low-memory read as it might
look out of context.

**Unrelated build fix found while verifying:** `-std=c11` with the
sandbox's glibc needs `_POSIX_C_SOURCE=199309L` for `clock_gettime` (used
by `cga.c`/`fall_object.c`/`cycle_objects.c`/`jump_gravity.c`/`enemy.c`'s
RNG-seeding/timing code) — without it, `make` fails before ever reaching
SDL2. Added `-D_POSIX_C_SOURCE=199309L` to the `Makefile`'s `CFLAGS`. Not
caused by anything in this session's changes; just surfaced while
re-verifying the build.

**Verification performed:** every touched file, plus every other non-SDL
`.c` file in the project, compiles cleanly (`-std=c11 -Wall -Wextra`, zero
warnings) against the real headers. Could NOT verify the SDL2-dependent
files (`main.c`/`video.c`/`input.c`) or a full link/run in this sandbox —
`libsdl2-dev`'s transitive dependencies (`libasound2-dev`/`libdrm-dev`/
`libgbm-dev`/`libudev-dev`) are stuck on an apt mirror version mismatch
here (404s / unmet-dependency errors), unrelated to the project itself.
Should build fine in a normal, up-to-date Ubuntu environment.

## 5y. Level-3 bird-enemy subsystem — ported (`level_objects.asm` ~1518-1715)

New file `src/level3_enemy.c` (+ `include/level3_enemy.h`), wired into
`main.c`'s demo loop gated on `level_number==3`. `check_fence_collision`
turned out to already be done (§5n-era work, living in
`level_collision.c` and already wired into `check_level_platform`) — this
session's actual remaining scope was `init_level3_enemy`,
`update_level3_enemy`, `draw_level3_enemy`/`erase_level3_enemy`, and
`check_l3_enemy_thrown`/`check_l3_enemy_cat`.

**No new sprite extraction needed** — both sprites the bird uses
(`dat_38bc`: two 84-byte wing-flap frames toggled by `l3_door_toggle`;
`dat_37c0`: the 252-byte "buzz away" escape sprite) come straight out of
the wholesale-embedded `ds_pool` at their real DS offsets (`0x38bc`/
`0x37c0`), same pattern as §5x. Confirmed the state-variable block's exact
byte layout (`dat_3964` word x, `dat_3966` byte y, `dat_3967` dive-state,
`dat_396a` undrawn-flag, `l3_door_toggle` at `0x396b`, `dat_396d`/`dat_396e`
frame/wobble counters, `dat_396f` an 84-byte background-save buffer,
`dat_39c3`/`dat_39c5` revert-on-hit snapshot, `dat_39c6` cached speed,
`dat_39c8` last-tick, `dat_39ca` next-draw address, `dat_39cc` an 8-entry
word speed table `2,4,6,8,10,12,14,16` per difficulty) directly against
the raw `db 0x00...` runs in `cat.asm`, including the neat sanity check
that `dat_39cc`'s table ends exactly at the already-known `l3_platform_id`
label.

**Movement/AI logic** (literal port, register-traced instruction by
instruction): every 2 BIOS ticks, if a thrown object currently overlaps
the bird it freezes for that tick; otherwise it either (a) chases the cat
horizontally at the difficulty-scaled speed while at rest height (`y==8`)
until its x-column aligns with the cat's, then (b) dive-bombs down toward
the cat's y with a small 4-phase wobble added to the descent, bailing back
upward (clamped at `y==8`) once it overshoots the cat's y, gets too far
horizontally (>0x30px), or hits the 0x9f depth cap. A second
thrown-object check after the position update reverts the move (and
returns without redrawing) if a throw lands mid-update; a cat-collision
check (both before AND after movement) triggers the "buzz away" escape
instead of normal movement.

**One deliberate deviation, clearly flagged**: the original's escape
sequence is a *synchronous* ~9-tick (`int 0x1a`) busy-wait spin loop
(playing a buzz tone via `init_buzz_sound`/`update_buzz_sound`) that would
block this port's externally-paced SDL frame loop for ~0.5s if translated
literally. Re-implemented as non-blocking state spread across per-frame
calls (`l3_bird_escaping`/`l3_bird_escape_start_tick`) — same style of
simplification already established for `int 0x1a` itself and for
`check_vsync` (see §5o). `init_buzz_sound`/`update_buzz_sound` are
no-op stubs either way since `sound.asm` isn't ported.

**Two unmapped shared flags** (`[0x552]`/`[0x553]`, read/written by this
routine and also referenced from `enemy.asm`/`sound.asm`/other
not-yet-ported `level_objects.asm` "enemy escaping" sequences for levels
4/5/6): exposed as `l3_bird_escaped`/`enemy_escape_active` globals in
`level3_enemy.h` rather than guessed at fully, so a future port of those
other sequences can wire into the same shared state if that turns out to
matter. Currently `enemy_escape_active` is never set by anything else, so
it can never block this subsystem's escape trigger — a documented
simplification, not a bug.

**Verification**: full project builds clean (`make`, zero warnings) —
`libsdl2-dev` installed successfully this session (`apt-get install
--fix-missing` recovered from the transitive-dependency 404s that blocked
§5x's session), so this is a real, linked, `-lSDL2` build, not just a
per-file compile check. Wrote a standalone harness
(`/tmp/test_l3bird2.c`) driving `update_level3_enemy()` across ~3.6s of
simulated real time with a stationary cat/no thrown object — ran to
completion with no crash, confirming the tick-gating and state machine
are at least structurally sound; didn't specifically engineer a
cat-collision scenario to exercise the escape path end-to-end.

## 5z. Thrown-object "prop rain" + footprint decals — ported (`level_objects.asm` ~41-687)

New files `src/level_objects.c` / `include/level_objects.h`, wired into
`main.c`'s demo loop (`init_thrown_objects()` at level setup,
`tick_thrown_objects()` every frame) and into `alley_movement.c` (real
`update_footprint()` replacing the old TODO stub). Also added a genuinely
new CGA primitive, `blit_or` (`cga.c`/`cga.h`) — `draw_thrown_sprite`
turned out to use a third pixel-combine mode (`src | dest`, saving the
old dest word first) distinct from both `blit_masked`'s AND and
`blit_transparent`'s chroma-key logic, re-derived directly from
`cga.asm`'s `mov bx,[es:di] / mov [bp],bx / lodsw / or ax,bx / stosw`
loop.

**No new sprite-extraction tooling needed after all** — despite what §17
item (d) predicted, everything resolved to plain `ds_pool` offsets, same
wholesale-embedding trick as §5x/§5y:
- `dat_3260` (`0x3260`): a 12-entry-plus-terminator word pointer table.
  Chased every pointer by hand against `cat.asm`'s raw `db` runs and
  found they're all spaced exactly 120 bytes (`0x78`) apart across only
  **7 unique addresses** (`0x2ea0`...`0x3170`) — the table walks up
  through all 7, back down through 6, and loops. That 7×120-byte region
  sits immediately before `l1_sprite_data`, confirming the byte count
  with no slack.
- `l1_sprite_data` (`0x31e8`, 120 bytes): despite the name, this is
  **not** sprite pixel data — it's all-zero in the original `.data`,
  i.e. the runtime background-save buffer for the OR-blit. Modeled as a
  plain `static uint16_t[60]`, same treatment as `l3_bird_mask_save`.
- `l1_obj_sprites` (`0x32b8`, 50 bytes = 5×10-byte frames): the real
  footprint-decal tile bitmaps (wear levels 0-4).
- `dat_32f2` (`0x32f2`): a 7-entry word rate table, confirming this
  system only covers **levels 0-6** — level 7 has its own separate
  `spawn_thrown_object`/`l7_obj_*` system (line ~41, part of the
  still-unported victory epilogue, see item (e)), which is why the two
  "thrown object" systems in this file don't share a rate table.
- Every offset cross-checked by chaining declared sizes from one raw
  `db` block to the next and confirming the running total lands exactly
  on the next named label (`dat_39cc`→`l3_platform_id`-style anchor
  check, repeated for the whole `l1_sprite_data`...`dat_32f2` chain) —
  high confidence, no guessing.
- `l1_anim_sprite_a/b/c` (§17's original note) turned out to belong to
  something else entirely — they sit right after `dat_32f2`, unreferenced
  by anything in this system's actual pointer table. Left alone for
  whatever ports `check_level_objects`/level-2 collectibles later (item
  (f), still blocked on `sound.asm`).

**Logic** (literal, register-traced port): rate-limited per level via
`dat_32f2`; each tick, if the current target footprint column already
carries a mark, the object steers toward that column's x and keeps
descending, and on reaching the ground fully erases itself, "splashes"
into the footprint tile (wearing it down by one level), and waits:
un-drawn until the next redirect. If the column is fresh, the cursor
moves on and a new bounce direction gets picked — sometimes forced
downward, sometimes fully random, sometimes biased back toward the cat
— gated by a noisy accumulator (`dat_32ea`/`dat_32eb`, updated by
XOR-ing in `counter & random()` every tick) checked against an
elapsed-time-decayed magnitude threshold. **Caught and fixed a real bug
mid-session**: my first draft had the "column already marked" and
"fresh column" branches swapped — re-read the raw disassembly a second
time line-by-line and corrected it before wiring anything in.

**One unresolved reference, latched rather than guessed**: `[0x410]`, a
tick reference read (not written) by this routine to compute an
elapsed-time decay, must be set elsewhere in the original (never found
by grepping the whole disassembly for a write to it). Latched once in
`init_thrown_objects()` as "tick at level entry" — the most plausible
reading given it's only ever used as a slow drift baseline — flagged
as a documented simplification rather than presented as fact.

**Verification**: full project builds clean (zero warnings). Standalone
harness (`/tmp/test_objs.c`) ran `tick_thrown_objects()` +
`update_footprint()` across ~4.8s of simulated time with a moving
target cat position — no crash, and the object's x/y visibly bounced
around rather than sitting static or diverging, consistent with the
intended behavior.

**Also fixed in this session, unrelated near-miss**: an `str_replace`
mistake briefly deleted `copy_with_stride`'s body while adding
`blit_or`'s declaration — caught immediately by recompiling `cga.c` in
isolation before moving on, restored, then re-verified clean.

## 6a. Level-7 victory epilogue — chunks 1-2 of ~5 (`level_objects.asm` ~3096-3327)

Started the incremental port of the level-7 "love scene" epilogue (§17
item (e)), per papi's explicit request to split it into small,
checked-in pieces rather than one big pass. New files
`src/level7_epilogue.c` / `include/level7_epilogue.h`, wired into
`main.c` gated on `level_number==7`.

**Important process note — the "label name = address" shortcut broke
here.** Every `dat_XXXX`-style label checked so far this project (§5x,
§5y, §5z) has had its own hex offset as its name, and chaining declared
sizes across a run of them always landed exactly on the next label. That
pattern does NOT hold for meaningfully-named labels like `l7_cat_x` or
`l7_save_buf_ptrs` — only the disassembler's own auto-generated
`dat_XXXX` names encode their address; hand-named labels don't. Caught
this by cross-checking `dat_4500`'s content length (72 bytes, counted by
hand) against the byte-gap implied by treating `l7_cat_x` as if it were
at `0x4530` (48 bytes) — they disagreed, which should never happen if
the assumption were valid. Re-ran `tools/resolve_data_segment.py` (a
real linear walk of `cat.asm`'s `.data`, already in the repo but unused
so far this session) to get true offsets instead of guessing further:
`l7_cat_x` is actually at `0x4548`, not `0x4530`. **Every address in
this subsystem is taken from that tool's output, not from label-name
pattern-matching** — see the header comment in `level7_epilogue.c`.

**Chunk 1 — sprite/slot primitives**: `init_level7_objects`,
`save_l7_slot`/`load_l7_slot`, `draw_l7_sprite`/`erase_l7_sprite`/
`clear_l7_sprite`, `setup_l7_sprite`. Up to 7 small "heart cat" sprites
each get a 12-byte state slot (position, facing, animation phase),
swapped in and out of a single scratch "current cat" struct — modeled
directly as a small C struct array rather than chasing the original's
raw byte-copy, since alignment/padding makes `memcpy`-ing a C struct
unsafe here (copied field-by-field instead, matching this project's
established style elsewhere). `draw_l7_sprite` turned out to reuse
`blit_or` (a mode-for-mode match with `tick_thrown_objects`'
`draw_thrown_sprite`, added in §5z) — no new CGA primitive needed.
`clear_l7_sprite` fills a fixed `0x5555` dither pattern rather than
restoring a saved background (confirmed correct: the epilogue's backdrop
is a flat area there, not something with real content to preserve).

**Chunk 2 — `check_l7_cat_hit` + `update_level7_objects`**: the real
per-tick round-robin update (only one of the 7 slots advances per
call, staggered by index) and the cat-catches-a-heart hit detection.
Kept unusually close to the original's raw `goto`/label structure rather
than restructured into clean nested `if`/`else` — deliberately: a
"cleaner" rewrite of a similarly tangled branch is exactly what produced
a real, caught-and-fixed direction-branch swap bug in §5z's
`tick_thrown_objects`, and this function's branches interleave even more
than that one did, so literal control-flow fidelity was judged the safer
translation here.

**Two functions stubbed on purpose for this checkpoint**:
`check_l7_cupid` (always reports "no hit") and
`check_l7_object_overlap` (no-op) — both genuinely not yet read/ported.
Without them, `update_level7_objects`'s three call sites into them would
leave dangling references; stubbing lets this checkpoint build clean
with zero warnings and be fully exercised end-to-end, same precedent
`cycle_objects.c` already set for `restore_alley_buffer`/`start_tone`
(duplicated local stubs rather than a shared header, see the comment
above `l7_restore_alley_buffer` in `level7_epilogue.c`). **Next chunk
(3) replaces these two stubs with real ports** — until then, the
epilogue's heart-cats will walk, animate, and register a cat-hit, but
never react to "cupid" or to each other.

**`l7_heart_sprite_table`'s real size is `0x499` (1177) bytes**, not the
16 bytes this chunk's `[bx+table]` timing lookup implies — it's almost
certainly the start of a larger block holding real cupid/heart sprite
bitmaps consumed by the still-unported `check_l7_cupid`. Only the first
16 bytes (8 per-difficulty word timing values) are used so far; flagged
rather than assumed away.

**Verification**: full project builds clean (zero warnings). Standalone
harness ran `update_level7_objects()` across ~4.8s of simulated time
with a fixed cat position — no crash.

**Remaining for the level-7 epilogue** (§17 item (e)): chunk 3
(`check_l7_cupid`, `check_l7_object_overlap`, `check_l7_all_objects`);
chunk 4 (the victory-wave/pair-animation sequence — not yet read at
all); chunk 5 (`play_march_note`/`play_victory_march`/`draw_march_frame`
— the self-contained PC-speaker music sequencer).

## 6b. Level-7 victory epilogue — chunk 3 of ~5: cupid + falling-heart overlap

Replaced chunk 2's two temporary stubs with real ports:
`check_l7_cupid`, `check_l7_object_overlap`, and `check_l7_all_objects`
(a natural third function that only calls the other two — reads and
ports cleanly alongside them).

**`check_l7_cupid`** is a genuine literal port, not a stub — but it
depends on `cupid_active`/`cupid_x`/`cupid_y`, which belong to `ui.asm`'s
entirely separate flying-arrow "cupid" enemy (`update_cupid`/
`draw_cupid`/`erase_cupid`, seen in this project's very first
document dump but not yet touched). Declared as local statics
defaulting to `cupid_active=0`, so the port is honest and complete: it
correctly always reports "no hit" *because* nothing sets that flag yet,
not because the function itself is faked.

**`check_l7_object_overlap`** is the level-7-specific "catch a heart
falling from a window" check, working against `l7_obj_x`/`l7_obj_y`/
`l7_obj_active` (8-entry arrays, real DS offsets `0x2b5a`/`0x2b6a`/
`0x2b72` — confirmed with `resolve_data_segment.py`, not guessed). Those
arrays are themselves populated by `spawn_thrown_object`/
`tick_level_thrown_objects` (top of `level_objects.asm`, level 7's own
counterpart to the general levels-0-6 thrown-object rain from §5z) —
still unported, so for now the arrays stay all-zero and this function,
while completely and faithfully translated, never finds anything active
to catch. `l7_obj_erase_sprite` (`0x2b7a`, 90 bytes) is real sprite data,
read straight from `ds_pool` as usual. Confirmed the original only ever
processes **one** hit per call (falls straight through to a shared tail
with no loop-resume) and mirrored that exactly with an early `return`
rather than restructuring it.

**Verification**: full project builds clean (zero warnings). Re-ran the
existing level-7 smoke harness (`update_level7_objects()` across ~4.8s
simulated) — still no crash, unaffected since the newly-active code
paths require `l7_obj_active`/`cupid_active` to be nonzero, which
nothing sets yet.

**Remaining for the level-7 epilogue** (§17 item (e)): chunk 4 (the
victory-wave/pair-animation sequence — not yet read at all); chunk 5
(`play_march_note`/`play_victory_march`/`draw_march_frame` — the
self-contained PC-speaker music sequencer). Both still fully open.

## 6c. Level-7 victory epilogue — chunk 4 of 5: the victory-wave cutscene

Ported `position_victory_cat`, `animate_victory_pairs`,
`init_victory_wave`, `move_victory_object`, and the top-level
`run_victory_sequence` — the actual "you won" cutscene: the cat gets
carried up-and-centered by a cupid pair, then three waves of bouncing
cupid pairs animate across the screen.

**Deliberately blocking, unlike every other subsystem ported this
session.** Documented at length in `level7_epilogue.c` (search "DELIBERATE
EXCEPTION"): this is a genuine one-shot, non-interactive victory screen
that the original blocks the whole game on by design — not an artifact
of DOS's cooperative scheduling the way `check_vsync`'s busy-wait or
`tick_thrown_objects`' rate-limiting are. Converting it to a frame-driven
state machine would mean threading three nested loop levels (the outer
"keep waving until something bounces" loop, the inner 8-object pass, and
`position_victory_cat`'s own step/settle loop) through shared mutable
counters — real risk for a cutscene that only ever runs once per level-7
completion and takes a few seconds. Uses real host sleeps
(`l7_sleep_ms`, ~5ms granularity) paced against the same BIOS-tick
arithmetic used everywhere else in this port, rather than a spin loop.

**All addresses re-verified with `resolve_data_segment.py`** (not
assumed from names, per the lesson from chunks 1-3 this session) — and
a good thing too: `l7_cupid_x_dir`/`l7_cupid_y_dir` turned out to hold
**real preset per-slot direction data** in the original `.data` (not
runtime scratch like `l7_cupid_x`/`l7_cupid_y`, which genuinely are
all-zero) — caught by actually looking at the raw `db` bytes rather than
assuming everything past `l7_cupid_active` was BSS-style state. Read
directly from `ds_pool` rather than modeled as C state.

**One faithfully-preserved oddity**: `init_victory_wave` only ever
*draws* the "lead" slot (slot 7, checked via a literal `cx==8` in the
original) each pass — the other 7 trailing positions move but are never
drawn, and nothing gets erased between draws, so the lead's positions
accumulate into an on-screen trail. Ported exactly as-is rather than
"fixed" — this is very plausibly the intended visual effect for a
victory-wave animation, not a bug.

**Stubbed, clearly marked**: all the victory/swoop/melody sound calls
(`sound.asm`, unported) and `handle_level_complete` (a separate,
still-unported score-bar/HUD subsystem). Two of `handle_level_complete`'s
own outputs — `[0x414]`/`[0x412]`, read/written by `run_victory_sequence`
itself too — are exposed as new `l7_completion_counter`/
`l7_completion_tick` globals in the header rather than left as dead
local state, so that future subsystem has real values to pick up.

**Verification**: full project builds clean (zero warnings). Ran
`run_victory_sequence()` standalone under a 30s timeout — completed on
its own in well under that, with `cat_x` settling at the expected
horizontal center (`0x80`/128) and `cat_y` at the expected stopping
height (`0x54`/84), `lives_count` and `difficulty_level` both
incrementing as intended. Confirms the bounce/wave termination
conditions are correct and this doesn't hang.

**Remaining for the level-7 epilogue** (§17 item (e)): chunk 5 only —
`play_march_note`/`play_victory_march`/`draw_march_frame`, the
self-contained PC-speaker music sequencer that plays over the level
completion transition (separate from this cutscene's own sound, which
is fully stubbed pending `sound.asm`).

## 6d. Level-7 victory epilogue — chunk 5 of 5: the march-music sequencer (DONE)

Ported `play_march_note`, `draw_march_frame`, and `play_victory_march` —
the marching-background animation/sound that plays during the level-7
completion transition (called from `level_transition`, right after
`run_victory_sequence`). **This closes out the level-7 epilogue
subsystem — §17 item (e) is fully done.**

Same "genuine one-shot cutscene, blocking is correct" reasoning as
chunk 4 (see the long comment there) — `play_victory_march` blocks for
a fixed ~40-BIOS-tick duration (paced via `l7_sleep_ms`), same as the
original.

**`l7_bg_pattern_tbl`'s targets are unlabeled data**, confirmed
deliberately rather than assumed: `resolve_data_segment.py` shows no
named label between `l7_bg_xor_flag` (`0x5016`) and `l7_bg_pattern_tbl`
(`0x52ae`) — a 662-byte gap almost certainly holding the actual
`{dst_offset, pose_flag}`-pair pattern lists `l7_bg_pattern_tbl`'s
per-difficulty pointers resolve into. Didn't need to hand-decode that
structure to port correctly: the C code dereferences through
`l7_bg_pattern_tbl` → pattern-list entries using the exact same
pointer-chasing the original assembly does, against the same
wholesale-embedded `ds_pool` bytes — correct regardless of whether every
intermediate byte's role is understood.

PIT/speaker frequency programming (the actual "make a sound" half of
`play_march_note`) is `sound.asm` territory and stays unmodeled, same as
every other sound call ported this session; `silence_speaker_stub` (the
"turn it off" half) is reused from earlier in this file.

**Verification**: full project builds clean (zero warnings). Ran
`play_victory_march()` standalone under a 15s timeout for both the
normal path (`difficulty_level>=2`) and the early-exit path
(`difficulty_level<2`) — both returned promptly with no crash.

**The level-7 victory epilogue (§17 item (e)) is now fully ported**,
across this session's five chunks: sprite/slot primitives, the per-tick
heart-cat walk/hit logic, cupid/falling-heart collision, the
victory-wave cutscene, and this march-music sequencer. Remaining
`level_objects.asm` work is items (f) (level-2 collectibles, blocked on
`sound.asm`) and (h) (`check_stairs_collision`/`check_window_landing`,
blocked on the window state machine) — both external blockers, not
scope still to investigate.

## 6e. `sound.asm` — fully ported, with a real emulated PC speaker

The last big external blocker. `sound.asm` (940 lines, ~40 routines) was the
thing §17 item (f) was waiting on, and the reason ~30 call sites across
`enemy.c`/`level_collision.c`/`alley_movement.c`/`cycle_objects.c`/
`level3_enemy.c`/`level7_epilogue.c` were carrying local no-op stubs. All of
it is ported and wired in; the game has sound.

**Ground truth re-acquired first.** The `alleycat-disassembly` source had
gone missing from the working tree again (same as §5x). Re-cloned from
`gmegidish/alleycat-disassembly`, re-ran `tools/resolve_data_segment.py`,
and confirmed it reproduces the exact 28976-byte data segment already
embedded in `src/gen_ds_pool.c` — byte-exact, diff-checked — before writing
a line of C. Per §7, nothing gets ported against a remembered table.

### The architecture: model the ports, not the notes

`sound.asm` never calls a "play a note" API. It writes hardware directly,
in three distinct ways, and every routine mixes them freely:

1. **tone mode** — `out 0x43,0xB6` (ch2, lo/hi, mode 3 square wave), two
   `out 0x42` writes for the divisor, then `in 0x61 / or al,3 / out 0x61`.
2. **direct PWM** — `in 0x61`, XOR/AND/OR with bit 1, `out 0x61`, with the
   gate bit left clear so the cone follows the data bit itself. This is what
   `play_explosion_effect`, `play_hiss_sound`, `update_noise` and
   `update_buzz_sound` actually are. Their pattern tables
   (`explode_pattern`, `buzz_pattern`) contain nothing but `0x00` and
   `0x02` — i.e. speaker-data-bit values — which is the confirmation.
3. **sub-tick timing** — `read_pit_timer` latches PIT channel 0 and reads
   its 16-bit *down*-counter, used as a ~1.19 MHz clock by
   `play_timed_tone`/`play_falling_sound`/`play_crash_sound`.

So the port models the three ports and lets everything audible fall out of
that, instead of trying to recognize each routine's musical intent:

- `src/speaker.c` + `include/speaker.h` — **pure C, no SDL**: `pit_out_43`,
  `pit_ch2_out`, `port61_in`, `port61_out`, `read_pit_timer`, plus a
  timestamped ring buffer of port writes and a square-wave renderer that
  replays them. The timestamping is what makes the direct-PWM effects
  audible at all: a "read the current state once per audio callback" design
  would flatten a 110 ms explosion into one constant level.
  Speaker output is modelled as `data_bit AND (gate ? ch2_square : 1)`,
  which is what makes both mode 1 and mode 2 fall out of one generator.
- `src/audio.c` — the only SDL-aware piece: opens a 44.1 kHz mono S16
  device with a 512-sample (~11.6 ms) buffer and pumps `speaker_render()`.
- `src/sound.c` — literal, label-for-label translation of the assembly,
  writing the same byte values to the same port numbers in the same order.

Keeping `speaker.c` SDL-free is deliberate: it makes the whole sound path
unit-testable with no audio device (see Verification).

### Data tables: straight off `ds_pool`, and here's why

The sound parameter block (DS `0x59c2`–`0x5b12`) is the densest field of
**split labels** in the whole data segment. `ambient_note_mask` has a
2-byte label span but is indexed `[si + ...]` with si up to 3;
`ambient_base_dur` (4-byte span) is indexed `[di + ...]` with di up to 6 and
runs straight into `ambient_accent_dur`; the same for
`ambient_rhythm_base`, `ambient_pitch_step`, `ambient_octave_mask`. A
per-label extraction pass would have produced exactly the §3/§7 failure this
project keeps catching. Reading through `ds_pool` at resolved offsets makes
the adjacency a non-issue — no new extraction was needed at all, same as
§5x/§5y/§5z.

**Zero-gap extent verifications performed** (each table's size derived from
how the code indexes it, then checked against the next label's offset):

| Table | Derivation | Next label | Slack |
|---|---|---|---|
| `extralife_sprites[0]` @0x5b20 | 4 words x 68 rows = 544 B | 0x5d40 = `[1]` | 0 |
| `extralife_sprites[1]` @0x5d40 | 544 B | 0x5f60 | 0 |
| `extralife_icon_data` @0x5f68 | 4 words x 16 rows = 128 B | 0x5fe8 | 0 |
| `extralife_text_data` @0x5fe8 | 6 words x 21 rows = 252 B | 0x60e4 | 0 |
| `ambient_rhythm_pattern` @0x59c2 | deepest reachable read 0x2f | 0x59f2 (48 B) | 0 |

That last one is the nicest: `ambient_rhythm_base` is `{0, 0x10, 0x200,
0x600}`, but only si=0 and si=1 can ever reach the rhythm-gate code (si=2
returns early via the `gravity_y` branch, si=3 always diverts to the
footstep-note branch). si=0 masks the note position to ≤ 0x0f with base 0;
si=1 masks it to ≤ 0x1f with base 0x10. Deepest byte touched: 0x10+0x1f =
0x2f — the 48th and last byte of the table. The code's own reachability
proves the table's extent.

### Findings and corrections

**1. `sound_enabled` was the wrong type, and it mattered.** This port
declared it `bool sound_enabled = true`. The original inits it with
`mov byte [sound_enabled],0xff` (entry.asm:71) and toggles it with
`not byte [sound_enabled]` (input.asm:180) — it is a 0xFF/0x00 **byte**.
That is not cosmetic: `update_noise` and `play_hiss_sound` do
`and dl,byte [sound_enabled]` where `dl` holds the speaker **data bit**
(0x02). With a boolean 1, `0x02 & 0x01 == 0` — both effects would have been
silent forever while every check of "is sound on?" still passed. Changed to
`uint8_t` with the original's semantics, and the toggle in `input.c` is now
the literal `~` plus the `silence_speaker()` call input.asm makes when the
toggle turns sound off (which this port was also missing). There is a
regression test for exactly this.

**2. `init_sound` is not a sound.asm function.** Despite the name and
despite being called 7 times from `entry.asm`, it lives in `enemy.asm:359`:
it resets `enemy_chasing`/`enemy_tick_counter`/`enemy_approach_timer`/
`enemy_exit_timer`/`enemy_active`/`enemy_y_pos` and *then* calls
`init_chase_sound`. `enemy.c` had it as a no-op stub, so every level entry
was silently skipping an enemy-state reset. Now ported for real.
sound.asm's own reset entry point is `init_music`.

**3. `start_tone` takes two different frequencies, on purpose.** It stores
BX into `[tone_freq]` but programs the PIT with **AX**. The first tick of a
tone therefore sounds at AX, and every later tick — reprogrammed by
`play_sound`'s tone-tail branch from `[tone_freq]` — sounds at BX.
`play_random_chirp` (BX, then AX = BX+0x1e) and `play_meow_sound` rely on
this. The C signature is `start_tone(ax_freq, bx_freq)` with the quirk
documented at the declaration, and there's a test asserting both halves.

**4. Two labels are used as frequencies, not pointers.** `play_meow_sound`
does `mov bx,meow_sound_data` and `level_objects.asm:3303` does
`mov bx,l5_sprite_ptr` — in both cases the label's own DS **offset**
(0x1312 and 0x123b) is the tone value, with no dereference anywhere. There
is no such thing as "meow sound data"; the label name is a disassembler
guess. Ported as the literal constants, with the resolution noted at both
sites. (`level7_epilogue.c`'s `l7_start_tone(0xce4, 0)` stub had been
passing a placeholder 0 for this; now correct.)

**5. A stale-flags artifact in the ambient generator.** At `lab_5653` the
original ends a block with `mov`/`mov`/`jnz lab_568d`. Neither `mov` touches
flags, so the `jnz` is testing the `cmp byte [auto_walk],0` from three
instructions earlier — on that path it is an unconditional jump. Translated
as such, with the reasoning in a comment so nobody "fixes" it later.

**6. `wipe_sound_start` is a bare `ret`** in the original — a hook that was
never filled in. Ported as an empty function rather than quietly dropped,
so the call site in `enemy.asm` still has something to point at.

### Documented simplifications (modeled, not guessed)

- **Busy-wait loop rate.** On a 4.77 MHz 8088 the *execution rate of the
  loop was the timbre* — which is precisely why the game checks `rom_id`
  and halves/doubles its loop counts for the PCjr. That rate is not
  reproducible on a modern CPU, and running those loops flat out would
  toggle the speaker far above the audio Nyquist rate and produce nothing
  but aliasing. So they are paced against an emulated 8088 cycle clock
  (`speaker_spin_cycles`), with the per-iteration cost written out as a
  named sum of instruction costs at each call site. The `int 0x1a` figure
  (400 cycles) is an estimate, flagged as such in `speaker.h` — it sets the
  timbre of the explosion/hiss/crash effects and is the one number here that
  is judgement rather than evidence.
- **`rom_id` added** (`cat_state.c`, value 0xFF = PC/XT). It was missing
  from the port entirely even though `sound.asm`, `game_loop.asm` and
  `enemy.asm` all branch on it. Not PCjr, so all the `0xFD` branches take
  their non-PCjr path — but they are ported, not elided.
- **The explosion's CGA border flash** (`mov ah,0xB / int 0x10`, border to
  red then back to black) has no counterpart: `cga.c` models the
  framebuffer only, with no border/palette register. Left as an explicit
  commented gap rather than silently dropped.
- **PIT channel 0 is modeled from the host clock**, not from a real free-
  running counter, so `read_pit_timer` is a 16-bit down-counter at
  1193182 Hz derived from `CLOCK_MONOTONIC`. The original's `prev - now`
  elapsed-time arithmetic works unchanged against it.
- **Blocking is preserved where the original blocks.** `play_timed_tone`
  blocks for `[ambient_duration]` PIT ticks (0x1000–0x2000 ≈ 3.4–6.9 ms),
  and `play_tone`/`play_hiss_sound`/`play_crash_sound`/
  `play_explosion_effect`/`play_swoop_sound` are genuine one-shots — same
  reasoning as §6c/§6d's cutscenes. Measured cost of the per-frame path
  below.
- **A one-pole low-pass** sits on the renderer output, modelling a speaker
  cone that cannot reproduce a mathematically sharp square edge. It also
  takes the worst of the aliasing off the direct-PWM effects. This is a
  modelling choice, not something derived from the original.

### Not ported from sound.asm (deliberately)

`render_sprites` is in `sound.asm` but has nothing to do with sound: it
blits the alley's decorative foreground sprites (a difficulty-indexed
position list, RNG-picked variants from `sprite_variant_table`/
`sprite_dims_table`/`sprite_data_ptrs`). It needs its own §3/§4 rigor pass
on those three pointer tables, and bundling a sprite-extraction job into a
sound port is how tables get mislabeled. Left for the alley-drawing work,
flagged in §0's todo.

> **CORRECTION (added in §6f):** this paragraph originally also guessed that
> `render_sprites` "is very likely the real `draw_alley_foreground` that
> three files currently stub". That was wrong. `draw_alley_foreground` is a
> real, separate, 10-line routine in `alley.asm`, now ported (§6f). The two
> are unrelated: one draws the *cat* over a saved background, the other
> draws *scenery*. The guess was made without reading `alley.asm` — exactly
> the shortcut §7 warns about.

`show_extra_life` **was** ported (it is half cutscene, half
`play_result_note` driver, and its four sprite extents all verified zero-gap
above), so the only sound.asm routine left is `render_sprites`.

### Verification

- **Full project builds clean**: `make` with `-std=c11 -Wall -Wextra -O2`,
  zero warnings, and for the first time in this project's history the SDL2
  link and run actually work in the sandbox — `libsdl2-dev` 2.30.0 installs
  fine now, so §5x's "could not verify the SDL-dependent files" caveat is
  lifted. `sound.c`/`speaker.c` are also clean under `-Wshadow`.
- **18-assertion test suite** (`/tmp/test_sound.c`, links only the pure-C
  files — no SDL, no audio device), all passing:
  - port programming: `silence_speaker` clears gate+data;
    `start_tone` programs AX and sets both bits; `play_sound`'s tone tail
    then reprograms with BX (finding 3, asserted end to end).
  - `play_music_note` walks `title_music_seq` correctly, including the
    byte-offset-into-a-word-table indexing and the `0` = rest case —
    checked against `title_music_freqs` values pulled independently from
    `ds_pool`.
  - `play_victory_note` reproduces the first four notes of
    `victory_melody` exactly (0x1800, 0x1562, 0x142e, 0x11fa).
  - **regression test for finding 1**: with `sound_enabled = 0xFF`,
    `update_noise` drives the data bit high on 384/400 samples; with a
    boolean `1` it is 0/400. The bug is pinned.
  - **end-to-end audio**: program divisor 0x400, capture 300 ms, render to
    PCM offline, count zero crossings → **1166.0 Hz measured vs 1165.2 Hz
    expected** (1193182/1024), 0.07% error, peak amplitude 7000. The whole
    chain — port writes → event log → square-wave renderer — is verified
    numerically, not by ear.
  - a longer capture (hit sound + death melody + 12 victory notes) is
    written to `/tmp/alleycat_sound_demo.wav` for listening.
- **Frame-budget check** (`/tmp/test_frametime.c`): `play_sound()` in the
  per-frame alley configuration costs **0.01 ms average, 0.43 ms worst**
  over 200 calls, against a 33 ms frame budget. The ambient tempo gate means
  `play_timed_tone`'s blocking wait fires rarely, so preserving the
  original's blocking here costs nothing.
- **Smoke run**: `./build/alleycat` under dummy SDL drivers runs to
  completion with the audio device open, no crash.

### Wired in

`main.c` now calls `audio_init()` + `init_music()` + `init_sound()` at
startup and `play_sound()` once per frame from the master loop (matching
`entry.asm`'s 7 `play_sound` call sites), and `silence_speaker()` +
`audio_shutdown()` on exit. Every local sound stub in
`level_collision.c` (`play_hit_sound`), `alley_movement.c`
(`play_catch_sound`), `cycle_objects.c` (`cycle_start_tone`/
`cycle_play_random_noise`/`cycle_silence_speaker`), `enemy.c`
(`silence_speaker`/`init_sound`), `level3_enemy.c` (`init_buzz_sound`/
`update_buzz_sound`) and `level7_epilogue.c` (`play_victory_note`/
`play_swoop_sound`/`init_victory_melody`/`play_full_victory`/
`silence_speaker`/`start_tone`, plus `play_march_note`'s PIT programming,
which §6d had left unmodeled) now forwards to the real routine. The
distinctly-named wrappers in `cycle_objects.c` and `level7_epilogue.c` are
kept rather than renamed at every call site, so the §5q static-shadowing
hazard stays impossible.


## 6f. `alley.asm` — the background-persistence pipeline, finally real

`alley.asm` is 247 lines. It had been referenced as a blocker since §5f
("`save_alley_buffer`/`restore_alley_buffer` still stubs"), §5h, §5n, §5q
and §6e, and four different files were carrying their own inert copy of
these functions. Reading it took about as long as writing this paragraph.
**The lesson is §7's, again: a routine nobody has read is not a big routine,
it is an unknown routine.**

### What it actually contains

Four of the seven routines are the "dirty rectangle" pipeline the entire
alley scene is built on — ~50 call sites across `game_loop.asm`,
`level_objects.asm`, `objects.asm`, `enemy.asm` and `ui.asm` use them:

| Routine | What it does |
|---|---|
| `save_cat_background` | recompute `cat_draw_pos` from `cat_x`/`cat_y`, then ↓ |
| `save_alley_buffer` | `save_from_cga` that rectangle into `alley_save_buf` |
| `draw_alley_foreground` | `blit_masked` the cat over it, saving what it covers |
| `restore_alley_buffer` | `blit_to_cga` the saved background back, erasing the cat |

Plus `spawn_window_event`, `enter_building`, `handle_cat_death`. All ported
into `src/alley.c`; every duplicate stub in `alley_movement.c`,
`game_setup.c`, `cycle_objects.c` and `level7_epilogue.c` now forwards to
the one real implementation.

### Findings and corrections

**1. `buffer_size` is not a byte count.** `cat_state.h` documented it as
"total bytes in the current alley_save_buf snapshot". It is actually a
packed **CX dims pair** — high byte = rows, low byte = width in WORDS —
handed straight through to `save_from_cga`/`blit_to_cga` as the original's
CX, exactly like every other dims word in this codebase (§4). The proof is
in `draw_alley_foreground` itself: `mov cx,[cat_sprite_dims] /
mov [buffer_size],cx`, and `cat_sprite_dims` is unambiguously a dims pair
(`mov cx,0xb03 / sub cl,al` → 11 rows, 3-minus-al words). The old
`if (buffer_size == 0) return;` guard in `alley_movement.c` was treating a
dims word as a length. Comment corrected, code fixed.

**2. §17 item (h) was never blocked on `spawn_window_event`.** Since §5n/§5r
this document has said `check_stairs_collision`/`check_window_landing` are
"genuinely blocked on the unported window spawn/animation state machine
(`spawn_window_event`, `alley.asm`)". Two unrelated things were being
called "the window state machine":

- `spawn_window_event` (alley.asm) is **cosmetic**: while the cat stands
  still it blits a random window sprite pair over the cat's position, rate-
  limited to every 8th attempt. 36 lines. It touches no window *state*.
- What item (h) actually reads is `window_open_state[]`, `current_floor`
  and `window_column` — and those are maintained by **`throw.asm`**
  (`current_floor`/`window_column`, lines 31-172), `level_objects.asm:246`
  and `ui.asm:715` (`window_open_state` toggling), initialised in
  `alley_drawing.asm:82`. `alley.asm` writes none of them.

So porting `alley.asm` does **not** unblock item (h); `throw.asm` plus a
slice of `ui.asm` does. §0's todo updated accordingly. This is the same
class of error §5s caught and fixed for the alley hazard systems: a label
name doing the work that reading the code should have done.

**3. `render_sprites` is not `draw_alley_foreground`.** §6e guessed it
probably was. It isn't — see the correction inserted in §6e above.

**4. `enter_building` uses `enter_sprite[0]`, not the table.**
`mov ax,[enter_sprite_data]` loads the first *entry* of that table, not the
table's address — so the pose is `enter_sprite[0]`, unlike `setup_level`'s
`enter_sprite[3]` (§5f). Easy to get backwards; noted at the call site.

### Two layout facts that verify each other

`spawn_window_event` blits a top window sprite at `cat_draw_pos` with mask-
save buffer `alley_save_buf` (DS 0x5fa), then a bottom one at
`cat_draw_pos + 0xf0` with mask-save buffer `0x612`, then sets
`buffer_size = 0xc02` — a *single* 12-row restore for both halves.

- `0x612 - 0x5fa = 0x18` = 24 bytes = exactly the first blit's 6 rows x 2
  words. The two mask-save buffers are adjacent with zero gap.
- `+0xf0` = 240 bytes = 3 x 80, which in CGA's even/odd bank interleave is
  exactly **6 rows** down. So rows 6-11 of the 12-row restore land precisely
  on the bottom sprite.

Neither fact was assumed: each is implied by the other, and the test below
checks the consequence (walking away erases all 12 rows in one pass, with
no residue).

`window_sprite_top` (DS 0x0f92, span 16 = 8 word pointers) and
`window_sprite_bot` (0x0fa2, span 8 = 4 pointers) are read with
`and bx,0xe` and `and bx,0x6` — 8 and 4 entries exactly. Zero slack, extent
confirmed by the masks that read them rather than by the label boundary.

### Changes this forced elsewhere

- **`alley_save_buf` is now `uint16_t[128]`, not `uint8_t[256]`.**
  `blit_masked` saves whole destination *words* into it (the original's
  `bp`), while `save_from_cga`/`blit_to_cga` read it back bytewise. The
  original's buffer sits at DS 0x05fa with 114 bytes to the next label; the
  largest real use is the cat's own 11 x 3 words = 66 bytes.
- **`cat_sprite_ptr` + `cat_sprite_dims` added.** The original keeps a DS
  offset in `cat_sprite_data`; this port keeps a real pointer plus the
  original's packed dims word, per the "real backing arrays, not DOS
  scratch-RAM offsets" convention. `update_alley_movement` now sets both and
  calls `draw_alley_foreground()` instead of calling `blit_masked` itself
  with a NULL mask-save — which is what made the pipeline inert.
- **`main.c` no longer clears the screen every frame.** It clears once at
  startup (and on restart). The original never wipes per frame; it relies
  entirely on this save/restore pair. That also let the
  `g_last_frame`/`g_last_draw_pos` "keep redrawing the last pose so the cat
  doesn't vanish while idle" demo hack be **deleted** — an idle cat now
  stays on screen because nothing erases it, exactly like the original.
- **`handle_cat_death`'s `bp = 0x000e`** is a DS scratch area (the same one
  `update_viewport` uses). Modeled as a real 5 x 18-word array.

### Not ported

`update_viewport` (level-2 viewport reset) copies sprite bytes *into* that
DS 0x000e scratch area and then points `cat_sprite_data` at it. Modeling
that needs a writable scratch region and a decision about how it interacts
with the real-pointer convention above — a small job, but a separate one.
Flagged in §0.

### Verification

- **Full project builds clean** (`-std=c11 -Wall -Wextra -O2`, zero
  warnings; `alley.c` also clean under `-Wshadow`), links and runs.
- **Headless pipeline test** (`/tmp/test_alley.c`, real `cga_mem`, ASCII
  dump + pixel counting — the §5i-style visual check), all passing:
  - baseline: 0 black pixels on a clean background.
  - after **24 frames of walking**: **111** black pixels — one cat
    silhouette's worth (the sprite's bounding box is 12 x 11 = 132). A
    leaking restore would have left ~24 sprites' worth of trail. This is the
    check that proves the pipeline works at all.
  - idle for 3 frames: `buffer_size` goes `0x0b03` → `0x0c02`, i.e.
    `spawn_window_event` fired and armed the 12-row buffer.
  - walking away again: back to **108** black pixels, no window residue —
    the single 12-row restore erased both halves, confirming the `+0xf0`
    contiguity above.
  - ASCII dumps of the affected screen band inspected by eye at each step.
- **Smoke run**: `./build/alleycat` under dummy SDL drivers runs clean.


## 7. General lesson for this whole project

**Never trust a same-file label as a data region's end boundary, and never
assume a "width" field's units without cross-checking against real pointer
deltas.** Both mistakes were caught only because we verified against
independently-derived ground truth (the full resolved data segment +
pointer-table math) before writing any "final" rendering code. Keep doing
this for every remaining sprite category and every remaining data table —
it's slower but it's the only way to avoid quietly shipping corrupted
graphics that "compile and look plausible" but aren't actually faithful.
