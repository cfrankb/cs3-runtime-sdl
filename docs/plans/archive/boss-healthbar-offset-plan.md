# Plan: `healthbar_offset` for bosses

Status: **implemented 2026-11-23** (M1 parser + M2 renderers; 112/112 ctest,
incl. comma/negative parser fixture in `t_bossdata` and gauge-shift pixel
check in `t_gamemixin`. No docs/spec boss-ini module exists, so no hash
bump. Hardware path verified by compile + twin-of-software; manual
in-game eyeball still open.)

## Request

New key in `bosses.ini`:

```ini
healthbar_offset   32, 32    # x_offset, y_offset
```

Values are in pixels, may be negative, and are applied to the boss
healthbar draw in `drawBossses()` in both the software
(`CGameMixin::drawBossses`) and hardware (`EngineHW::drawBossses`)
renderers.

## 1. `src/bossdata.h` — extend `bossData_t`

Add next to `color_hp`/`color_name`:

```cpp
int hpbar_off_x = 0; // healthbar/name x offset (px, may be negative)
int hpbar_off_y = 0; // healthbar/name y offset (px, may be negative)
```

In-class defaults keep `bossData_t d{}` in `build()` and hand-built test
structs (`tests/unit/t_boss.cpp`) valid — no diff at other construction
sites.

No `ENGINE_VERSION` bump: `bossData_t` is ini-derived runtime data, not
savegame-serialized state, and is not `CActor`/`CMap`/`CLayer`.

## 2. `src/bossdata.cpp` — parser

- **Tokenizing**: `tokenize()` splits on whitespace only, so
  `healthbar_offset 32, 32` yields `["healthbar_offset", "32,", "32"]`
  — the comma sticks to token 1. The existing inline
  comma/negative-tolerant number scanner used for `hitbox` in
  `BossAccum::build()` (L696–719) already handles this. Extract it as a
  file-local `parseNumberList(const std::string &) -> std::vector<int>`
  and use it in the `hitbox` block **and** the new field (reuse, not a
  second parser).
- **INI accept** (new branch in the boss-config chain next to
  `hitbox`/`flags`, ~L899):

  ```cpp
  if (item == "healthbar_offset")
  {
      if (e.size() < 2)
      {
          LOGW("Warning: 'healthbar_offset' on line %d is missing a value: %s\n",
               lineNo, t.c_str());
          continue;
      }
      seq.fields["healthbar_offset"] = joinSpace(e, 1);
      continue;
  }
  ```

- **`build()`** (next to the hitbox block): parse with
  `parseNumberList`; take `v[0]`/`v[1]`, absent → 0. Negatives accepted
  (scanner already takes `-`). Garbage → LOGW per the existing
  convention; more/fewer than 2 numbers → LOGW, take what is there.

## 3. Render sites — identical change in both twins

`src/gamemixin.cpp` `CGameMixin::drawBossses` (~L1157) and
`src/engine_hw.cpp` `EngineHW::drawBossses` (~L1182), Hp Rect
construction:

```cpp
const bossData_t *bd = boss.data();
const rect_t hRect{
    .x = bRect.x + bd->hpbar_off_x,
    .y = bRect.y - HP_BAR_HEIGHT - HP_BAR_SPACING + bd->hpbar_off_y,
    // width/height unchanged
```

Notes:

- The **boss name** is anchored to `hRect` (`hRect.x`,
  `hRect.y - FONT_SIZE`) in both renderers, so the name shifts with the
  bar: the offset applies to the whole details block. Open question at
  review: keep this, or pin the name to the boss.
- Offsets are raw pixels in 8-px-grid units **before** `SCALE` (the hw
  path multiplies `hRect` by `SCALEF` afterwards), consistent with all
  other `drawBossses` coordinates.
- Clipping untouched: `betweenRect(hRect, sRect)` already
  rejects/clips offset bars going off-screen, negative or positive.

## 4. Test — `tests/unit/t_bossdata.cpp`

Extend the existing inline ini fixture (mirror-fixture block, ~L200):

- `[MirrorOK]` gets `healthbar_offset 32, -8` → assert
  `hpbar_off_x == 32`, `hpbar_off_y == -8` (pins comma handling **and**
  negatives in one fixture line).
- One boss with no key → both fields 0 (default preserved).
- Optional: `healthbar_offset x` garbage → 0 + LOGW path.

## 5. Verify & docs

- No new files, so no `bin/refresh.py` run needed.
- `cmake -B build && cmake --build build && ctest --test-dir build`;
  run the bossdata and gamemixin suites.
- Manual: add the key to a live `bosses.ini` boss, check both render
  paths.
- Check `docs/spec/README.md` for a boss-data ini-keys module; if one
  exists, add the key there and bump the module hash + compliance
  checkpoint rows only after tests pass.

## Deliberately skipped

Separate name-vs-bar offsets, per-renderer overrides, clamping offsets
to screen. Add when a mod actually needs them.
