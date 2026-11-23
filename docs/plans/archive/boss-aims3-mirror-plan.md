# Plan: `aims 3` + required `mirror left|right`

Date: 2026-11-23
Status: complete 2026-11-23 — §1-§7 (mirror key + validation, CBoss
frame mapping + hitbox reflection, HW/SW flip draws, no-live-boss
decision, spec, anchor warning), §8 (hitbox JSON out-of-block drop +
`LOGE`, `hb_mirror4.json` hostile fixture), §9 (boss sheet 2 via
obLT png metadata + Arachine import). 112/112 ctest green.
Archived.

## Problem

Boss frame layouts support `aims 1` (all directions share frames) and
`aims 4` (frames per direction). New art ships only UP, DOWN plus ONE
horizontal direction; the other horizontal direction is a horizontal
mirror. Saves 25% of boss sheet storage. Hitboxes authored for the
provided horizontal direction must serve the mirrored one reflected.

## Ini representation

```ini
aims            3          # directions: UP, DOWN, one horizontal
mirror          left       # REQUIRED with aims 3: LEFT omitted, mirrored from RIGHT

# animations
moving          4          # frames per direction: 12 physical in sheet, 16 logical
```

- `mirror left` → sheet holds UP, DOWN, **RIGHT**; LEFT is drawn mirrored.
- `mirror right` → sheet holds UP, DOWN, **LEFT**; RIGHT is drawn mirrored.
- Slot order for `aims 3` is **UP=0, DOWN=1, provided-horizontal=2**.
  Deviates from `JoyAim` enum order; pinned by tests and spec text.
- `aims 3` **without a valid `mirror` entry is a load error**
  (`LOGE`, fall back to `aims = 1` per the existing garbage convention
  at `bossdata.cpp:632-639`).
- `mirror` with `aims != 3` → `LOGW` + ignored.
- Only horizontal directions may be mirrored (`left`/`right` tokens;
  any other value is a load error like the missing case).

## Existing flow (verified)

- `CBoss::currentFrame()` (`src/boss.cpp:311`):
  `base + lenght * (m_aim % aims) + framePtr`. With aims=3 the modulo
  would wrongly map RIGHT(3)→slot0. Must be replaced for mode 3.
- Consumers, all via `currentFrame()`: HW draw `engine_hw.cpp:1118`,
  SW draw `gamemixin.cpp:1081`, hitbox test `boss.cpp:89`, range guard
  `gamemixin.cpp:1084` (needs nothing; physical frame numbers stay in range).
- Parsing: `bossdata.cpp:631` reads `aims`; scalar whitelist
  `bossdata.cpp:897-908` copies `aims` into `seq.fields`; sheet
  accounting `frame += count * aims` (`bossdata.cpp:984`) already yields
  12 for aims=3. Mode 3 currently parses and renders wrong; this plan
  makes it correct and adds `mirror`.
- Secondary hitboxes (`boss.cpp:93-98`): `hb.x = m_x + c.x - hbMain.x`,
  i.e. author space is sprite-anchored, result is boss-center-relative
  (half-tiles). Main hitbox box is placed at `(m_x, m_y)` directly
  (`boss.cpp:84`) — aim-independent, untouched.
- HW blit: `drawTile(tile, x, y)` (`engine_hw.h:88`) → SDL_RenderCopy.
- SW blit: `drawTile(bitmap, x, y, frame, rect, ...)`
  (`gamemixin.cpp:1123`, decl `gamemixin.h:322`).

## Changes

### 1. `src/bossdata.{h,cpp}` — new `mirror` key + validation

- `bossData_t` gains `int mirror` = the derived JoyAim
  (`AIM_LEFT`/`AIM_RIGHT`) or `-1` for none. Not serialized (ini-derived
  runtime data; `CBoss::write` persists only actor state) → no
  `ENGINE_VERSION` bump.
- Add `"mirror"` to the scalar whitelist (`bossdata.cpp:899`) and parse
  near `d.aims` (`bossdata.cpp:631`): `left`→AIM_LEFT,
  `right`→AIM_RIGHT, else −1 + `LOGW`.
- Validation after both are read:
  - `aims == 3 && mirror < 0` → `LOGE` + `aims = 1`.
  - `aims != 3 && mirror >= 0` → `LOGW` + `mirror = -1`.
- Sequence tally (`bossdata.cpp:977-984`): same aims-3-requires-mirror
  check on the per-sequence copy, so a rejected boss tallies its blocks
  as `count * 1` consistently with the fallback.

### 2. `src/boss.h` / `src/boss.cpp` — single mapping point

- `currentFrame()`: when `aims == 3`, map aim via table
  `{AIM_UP:0, AIM_DOWN:1, AIM_LEFT:2, AIM_RIGHT:2}` — identical for
  `mirror left` and `mirror right`; both horizontal aims share the one
  horizontal block, the flip flag distinguishes them. Other aims keep
  the existing modulo path unchanged.
- `AIM_NONE` handling in mode 3: normalise to **UP (slot 0)**, explicitly
  in the table — not via modulo coincidence. It cannot be discarded:
  `m_aim` defaults to `AIM_NONE` (`boss.h:135`) and it is a legal
  on-disk value for a boss restored before its first aim
  (`boss.cpp:486`), while `currentFrame()` must always return a frame.
  `isMirrored()` stays false for `AIM_NONE`.
  Note the existing modulo maps `AIM_NONE` to slot `255 % aims`
  (RIGHT at aims=4); that legacy behavior is left untouched for
  aims 1/2/4 and pinned as-is by test 3 below.
- Add `bool CBoss::isMirrored() const`:
  `m_bossData->aims == 3 && m_aim == m_bossData->mirror`.
- `testHitbox1()` (`boss.cpp:89-100`): when `isMirrored()`, reflect each
  secondary box in **boss space** (no frame-width lookup needed):
  `hb.x = m_x - (c.x - hbMain.x) - c.width` (normal path keeps
  `m_x + c.x - hbMain.x`). y/width/height unchanged.
- `getHitboxes(sheet, currentFrame())` needs no change: the mirrored
  aim resolves to the provided aim's physical spriteID, so the boxes
  authored for the provided direction serve both horizontal aims.
  JSON authoring contract + loader validation: §8 (scheduled LAST).

### 3. HW renderer — `src/engine_hw.{h,cpp}`

- `drawTile(const Tile *tile, int x, int y)` gains
  `SDL_RendererFlip flip = SDL_FLIP_NONE` (default keeps all other
  callers byte-compatible); pass it to the RenderCopy.
- Boss loop (`engine_hw.cpp:1118-1148`): when `boss.isMirrored()`,
  mirror the sprite rect about the boss centre:
  `bRect.x = GRID_SIZE * (boss.x() + hitbox.x) - tile->rect.w`
  (normal: `GRID_SIZE * (boss.x() - hitbox.x)`), and draw with
  `SDL_FLIP_HORIZONTAL`. Free: no extra texture memory; full disk+VRAM saving.

### 4. SW renderer — `src/gamemixin.{h,cpp}`

- `drawTile(CFrame &bitmap, int x, int y, CFrame &tile, const rect_t &rect, ...)`
  (`gamemixin.h:322`) gains trailing `bool flipX = false`; the blit loop
  maps source column to `rect.x + rect.width - 1 - col` when set.
  Do **not** use `CFrame::flipH()` — it mutates the shared sheet frame.
- Boss loop (`gamemixin.cpp:1096-1127`): same mirrored `bRect.x` formula
  with `frame.width()`, pass `flipX = boss.isMirrored()`.
- Existing BUG-C8 range guard unchanged.

### 5. Data — `data/bosses/bosses.ini`

- `data/bosses` is a symlink to the external asset repo (not in git
  here). No shipped boss (Mr. Demon/Ghost/Harpy `aims 1`, Beholder
  `aims 4`) provides 3-direction art, and flipping a live boss would
  shift every following sheet block against art that physically has
  four blocks. **Decision: switch no live boss**; the commented
  `aims 3` / `mirror left` template block already sits in the ini for
  when 3-direction art ships.

### 6. Docs

- DONE 2026-11-23: spec is `docs/spec/gamespec/boss-aims.md`
  (layouts table, slot table, mirror contract, renderer flip, hitbox
  mirror + JSON contract pointer). `docs/spec/bosses/` is declared
  historical/superseded by its own README; its `bossdata_runtime.cpp`
  mirror copy had already diverged before this feature and was left
  untouched. Compliance regenerated with
  `itools/gen_compliance.py` + `gen_coverage.py` (hashes refreshed,
  `engine_hw` rows stay unchecked — headless-unsafe, no direct suite).
- AGENTS.md version rules: no `ENGINE_VERSION` bump — ini is not
  serialized save/recorder state, `bossData_t` is not persisted, and no
  `CActor`/`CMap`/`CLayer` member changes. Stated in the spec file.

### 7. Data contract / warnings

- Main hitbox doubles as sprite anchor (`bRect.x`); for mode 3 the
  sprite anchor must sit so the mirrored rect still covers the sprite.
  The `LOGW` for an asymmetric anchor (`hb.x != frameW_half - hb.x -
  hb.width`) needs the frame width, which `bossdata` parsing does not
  know — deferred to a renderer milestone (draw loops have `frame.width()`
  / `tile->rect.w`). Warn only, don't reject.

### 8. Hitbox JSON mirroring — contract + loader validation (LAST milestone, done 2026-11-23)

Scheduled last: nothing in §1-§7 depends on it — runtime reflection
already serves the phantom direction — and existing JSONs stay correct
once authored per this contract.

- Membership mechanics (`readHitboxJson`/`readHitboxFile`,
  `bossdata.cpp:444-567`): boxes are placed in sheet-strip pixel space;
  `col = x / frameWidth`, `rawId = row * cols + col`,
  `frame_id = baseFrame + rawId`; the stored `x = offX / grid` is
  **cell-relative** half-tiles. The runtime boss-space reflection (§2)
  is the exact mirror of the renderer flip transform (body point `p`
  renders at `m_x + hb.x - p`; reflected box `[m_x - (p - hbMain.x) -
  c.width, ...)`), so provided-direction boxes mirror correctly with
  **no per-box JSON change** — the JSON never authors the phantom
  direction.
- Authoring contract for `aims 3`: `frame.cols` must match the sheet
  strip grid, and boxes appear only in the UP / DOWN /
  provided-horizontal blocks. A `cols 4`-style JSON (or a box placed in
  the phantom block) maps `rawId`s past the sequence's
  `count * 3`-frame block; sheet blocks are contiguous, so those boxes
  would silently land in the **next boss's** frame IDs (hitbox bleed).
- Loader validation (`handleSeq`): move the `readHitboxFile` call after
  the `count`/`aims` parse, then drop + `LOGE` every entry whose
  `frame_id` falls outside `[base, base + count * aims)` — catches
  phantom-direction boxes and misaligned `cols` at the source.
- Test (`t_bossdata_hostile`): MirrorOK gains
  `moving 4 hb_mirror4.json` with `cols 4` and a box in the 4th
  physical block (`rawId 13` ≥ block size 12) → dropped + warning;
  check `g_hitboxCount` and the surviving entry's `spriteID`.
- Sync `docs/spec/bosses/bossdata_runtime.cpp` and compliance hashes
  for this change, same rule as §6.

### 9. Boss sheet 2 + Arachine import (2026-11-23, done)

First real aims-3 boss ships as **sheet 2** (art:
`private_tools/sheet/out/arachine/archine.png`, 3 directions N/S/E =
UP/DOWN/RIGHT, 104x96 cells, 60 frames = walk 5, attack 5, death 5,
damaged 4, idle 1 x 3 aims):

- `ASSET_SHEET2` appended after `ASSET_FXSHEET` (append-only manifest
  indices); load is **optional** like the fx sheet — manifests without
  the row skip it with `LOGW`.
- Frame rects travel inside the png's private `obLT` chunk (same v2
  8-byte records as `sfx.png`), loaded via `TileSet::loadFx` — no
  `g_sheet2_data` tileinfo table to maintain.
- `sheet == 2` selection branch in both boss draw loops
  (`engine_hw.cpp`, `gamemixin.cpp`; SW branch null-guarded for the
  optional asset). `>>>sheet N` parsing needed no change (resets the
  per-sheet frame counter, accepts any int).
- `bosses.ini`: `[Arachine]` under `>>>sheet 2`, `aims 3` +
  `mirror left`; ini animation order MUST match the png's obLT record
  order (walk, attack, death, damaged, idle) — pinned in
  `test_bossdata` via seq bases 0/15/30/45/57 and hitbox JSON `cols`.
- `BOSS_ARACHINE 0xb4`, `BOSS_COUNT` 4 -> 5 (`src/bossdata.h`, test
  roster expectation only).
- Asset side (external): copy `archine.png` into `pixels/` and append
  the `ASSET_SHEET2` manifest row; hitbox JSONs already in
  `data/bosses/data/`.

## Tests (`tests/unit/t_bossdata.cpp`, `t_boss*.cpp`)

1. Fixture boss `aims 3` + `mirror left`, `moving 4`: seq base offsets
   step by 12; next sheet entry starts at +12 (not +16).
2. `currentFrame()` mode 3: UP→slot0, DOWN→slot1, LEFT→slot2,
   RIGHT→slot2; `AIM_NONE`→slot0 (UP). `isMirrored()`: true only for
   the declared mirror aim.
3. Legacy pinned: aims 1/2/4 frame numbers unchanged (modulo path).
4. Mirrored secondary hitbox: box at boss-space `+dx` tests at
   `-dx - width` when aim == mirror aim.
5. Validation: `aims 3` without `mirror` → fallback aims=1 (+ warning);
   `mirror` with aims=4 → ignored; `mirror bogus` → treated as missing.
   `mirror right` fixture: LEFT is physical, RIGHT is the mirrored one.
6. Existing suites `t_bossdata`, boss frame-layout tests still green.

## Build/verify

`cmake -B build && cmake --build build && ctest --test-dir build`
(one runner at a time per AGENTS.md).

## Skipped (add when needed)

- Mirroring UP/DOWN (vertical flip) — no art needs it.
- Baked flipped SW copies — only if a flip-aware blit proves impossible.
- aims clamping above 4 — pre-existing hole, unrelated.
