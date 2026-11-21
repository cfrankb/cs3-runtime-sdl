# Extended FX (`fx_t`) Specification

Status: **Implemented** (2026-11-19: data model + `CGame` state v0x02000012,
hardware and software renderer passes; 2026-11-20: S5 derived poison-aura
draw entries; 2026-11-21: S1 spawn alignment modes, and 1.4 hitbox damage
mechanics consumed via the boss-skill registry — see
[`boss-skills.md`](boss-skills.md)).

Complements the classic `sfx_t` (16px tile-aligned, `src/gamesfx.h`), which is
**frozen**. Extended FX adds:

- varying frame sizes (32x32, 32x48, 48x48, ...) taken from the frames themselves,
- multi-frame animation sequences on a dedicated sprite sheet,
- up to two layers per sequence: **back** (drawn before actors) and
  **front** (drawn where classic sfx draw today),
- pixel-granular world coordinates,
- state owned by `CGame` so the software (`CGameMixin`) and hardware
  (`EngineHW`) renderers share one data path and one view query.

## 1. Data structures

### 1.1 `fx_t` — per-instance, fixed 12-byte POD, raw-serialized

| field      | type       | meaning                                   |
|------------|------------|-------------------------------------------|
| `x`, `y`   | `int16_t`  | world **pixels**, anchor = frame center   |
| `ox`, `oy` | `int16_t`  | origin point in **tile coords** (map units); `-1` = not available (pixel-granular spawn) |
| `effectID` | `int16_t`  | index into the `g_fxData` table           |
| `timer`    | `uint16_t` | ticks alive, counts up from 0             |

`fx_t` is the wire record itself: serialized with a raw `_W`/`_R` of
`sizeof(fx_t)`, identical in style to `sfx_t`. No callbacks or non-POD
members live here — end-of-sequence behavior hangs off `fxData_t` (1.3), so
save/load carries no runtime state.

Invariants:

- I1. `timer` is incremented by `CGame::tickFx()`, called next to
  `purgeSfx()` in the same `!timeFrozen` tick block. End-of-life per effect
  class:
  - **one-shot** (`FX_ONESHOT`): ends when the animation has played once:
    `timer >= frames * speed` where `frames = max(back.count, front.count)`.
  - **repeatable** (`FX_REPEATABLE`): loops; **`ttl` in the table entry
    determines the end**: ends when `timer >= ttl`.
  On end: the **table entry's** `onEnd(*this, fx)` fires (if set), then the
  instance is erased.
- I2. Frame size is **not** stored per-instance nor per-layer. `CFrame` carries
  its own w/h; the table's `w`/`h` below are max-frame extents for culling only.
- I3. No frame index, no layer state is stored per-instance (I4/I5 derive them).
- I4. Frame index is derived. One-shot:
  `idx = min(timer / speed, layer.count - 1)`; repeatable:
  `idx = (timer / speed) % layer.count`. `frameID = layer.base + idx`.
  Both layers share the one `timer` and `speed`.
- I5 (sequence index location). The sequence index is **not stored** in
  `fx_t`, `fxData_t` or any side table: it is computed from `timer` per I4
  inside `getFxToDraw()` and materialized only as `fxDraw_t.frameID` for the
  renderer. Save/load needs no extra field because `timer` fully determines
  it. Storing a per-instance `frame` becomes necessary only if an effect
  needs per-instance pause or a mid-life speed change (see Skipped).

### 1.2 `fx_layer_t` — one layer's sequence (static)

```cpp
struct fx_layer_t
{
    uint16_t base;  // relative frameID on the fx sheet
    uint16_t count; // 0 = layer unused
};
```

### 1.3 `fxData_t` — static definition table (`g_fxData`)

```cpp
struct fxData_t
{
    uint16_t id;       // Fx enum value
    uint16_t sheet;    // fx sheet index (single sheet for now)
    uint8_t  speed;    // ticks per frame (lower = faster, boss speed_anime style)
    uint8_t  flags;    // FX_ONESHOT / FX_REPEATABLE
    uint16_t ttl;      // lifetime (ticks) for FX_REPEATABLE entries
    uint16_t w, h;     // max frame extents, culling only
    uint8_t  align;    // FX_ALIGN_CENTER / FX_ALIGN_BOTTOM spawn anchor (S1)
    fx_layer_t back;   // drawn before actors
    fx_layer_t front;  // drawn after actors
    // --- provisions for hitbox / damage / splash mechanics (later) ---
    int16_t  damage;         // reserved, unused today
    uint8_t  splashRadius;   // reserved, unused today
    const fx_hitbox_t *hitboxes; // per-frame box set (fx_hitbox_t below)
    uint8_t  hitboxCount;
    FxEndFn  onEnd;          // fired by tickFx when an instance of THIS
                             // effect ends; per-effect, table-owned, never
                             // serialized (the table itself is not state)
};
```

`Fx` enum ids are a **new id space**, independent of `Sfx` (`MIN_SFX = 0xf0`);
fx never travels the `sprite_t`/tileID pipeline.

### 1.4 `fx_hitbox_t` — reserved mechanics provision

```cpp
struct fx_hitbox_t
{
    int16_t  dx, dy; // offset from frame center anchor
    uint16_t w, h;
    int      type;   // box role, mirrors BossData::HitBoxType
                     // (MAIN / ATTACK / SPECIAL1 / SPECIAL2)
};
```

Shape mirrors the boss `hitbox_t` precedent. The table fields above
(`damage`/`splashRadius`/`hitboxes`/`hitboxCount`) remain **declared, no
consumer** (kept per compatibility rule). The lightning-strike-style damage
mechanic shipped through a different binding: per-frame boxes live on the
boss-skill registry (`bossSkillDef_t::frames`, loaded from `data/fx/*.json`)
and `tickFx()` hit-tests them against the player — see
[`boss-skills.md`](boss-skills.md) §4. Nothing in this spec requires the
table fields.

### 1.5 `fxDraw_t` — renderer-facing draw entry (never serialized)

```cpp
struct fxDraw_t
{
    int16_t  x, y;    // world px, center anchor (copied from fx_t)
    uint16_t frameID; // fx-sheet-relative; renderer resolves pixels in its own fx set
};
```

`CGame` does not own renderer pixel data, so `fxDraw_t` carries a **frameID,
not a `CFrame *`** (same split as the sheet's `obLT` rect metadata vs
per-renderer `m_tileset_fx` / `m_fxSheet`).

## 2. Ownership and API (all on `CGame`)

```cpp
std::vector<fx_t> m_fx;                  // beside m_sfx
std::vector<fx_t> &getFx();
int  spawnFx(int effectID, int tileX, int tileY);
int  spawnFxPx(int effectID, int px, int py);
void tickFx();
void getFxToDraw(const fxView_t &view, bool front,
                 std::vector<fxDraw_t> &out) const;
```

`FxEndFn` = `std::function<void(CGame &, const fx_t &)>`, a `fxData_t`
member. Spawns carry no callback: behavior is per-effect, not per-spawn.

- **S1 (spawn helpers).** `spawnFx` is the tile->pixel conversion point and
  records the **origin point** (`ox/oy` = tile coords, always the true tile).
  Anchor per the table entry's `align` mode (2026-11-21): `FX_ALIGN_CENTER`
  (default) `x = tileX * TILE_SIZE + TILE_SIZE / 2`, same for y;
  `FX_ALIGN_BOTTOM` (tall ground-flushed effects — pillars, columns) keeps
  `x` tile-centered and sets `y = (tileY + 1) * TILE_SIZE - h / 2`, flushing
  the frame bottom with the tile bottom. `spawnFxPx` takes world
  pixels directly and leaves `ox = oy = -1` (origin not available). Both
  return the instance index, or `-1` for an unknown `effectID`.
- **S2 (cull).** `fxView_t` is four world-px bounds (left/top/right/bottom).
  An instance is in view when its center lies inside the view rect inflated by
  `(w/2, h/2)` of its table entry. Culling needs no pixel data, so it lives in
  `CGame` and runs once for both renderers.
- **S3 (emit).** For each live instance, emit one `fxDraw_t` for the requested
  layer side if that side's `count > 0`.
- **S4 (clear).** `m_fx` clears wherever `m_sfx.clear()` happens on new game.
- **S5 (derived draw entries — poison aura, 2026-11-20).** `FX_POISON_AURA`
  spawns **no** `fx_t` instance: `getFxToDraw()` emits one draw entry per
  layer call iff `S_POISON != 0`, anchored on the player
  (`pos * TILE_SIZE + w/2`, center anchor like any fx) and culled with the
  same view inflation as S2. Its frame cycles off a dedicated free-running
  clock, `CGame::m_fxTick` (incremented in `decTimers()`), not a per-instance
  `timer`: `frameID = layer.base + (m_fxTick / speed) % layer.count`.
  Entries are emitted per frame, never instantiated, never serialized —
  `m_fxTick` is not part of the savegame. Emission is suppressed while
  `g_fxSheet_data` is too short to cover both layer ranges, so the code
  merges before the sheet assets grow. Behavior note: the clock advances
  wherever `decTimers()` runs; pause paths that skip it freeze the aura —
  cosmetic.

## 3. Renderer contract (both renderers, minimal)

Fx frames live on a dedicated fx sprite sheet PNG, but **loading recycles the
existing boss/sheet code**: the load call belongs in `preloadAssets()` of
*both* renderers (`EngineHW::preloadAssets`, engine_hw.cpp L123;
`CRuntime::preloadAssets`, runtime.cpp L1269 — the `CGameMixin` pure-virtual
filled per environment), next to the existing `sheet0`/`sheet1` loads, using
the same loaders (`TileSet::load(renderer, filename, vector<tileinfo_t>)` on
the hardware side, PNG + `tileinfo_t` rects -> `CFrameSet` on the software
side). **`preloadAssets()` is environment-dependent** (the software
`m_sheet0/m_sheet1` are not even assigned in `src/`): the fx load call ships
as **commented-out boilerplate** only — the maintainer finalizes and enables
it per environment. Until then the set is empty/null and every fx draw pass
no-ops; `getTile()`/`operator[]` are already null-safe.

- R1. There are **four** scene passes: static and dynamic viewports, in each
  renderer (`CGameMixin::drawViewPortStatic` / `::drawViewPortDynamic`,
  `EngineHW::drawViewPortStatic` / `::drawViewPortDynamic`). Every one of them
  draws fx twice: `getFxToDraw(view, /*front=*/false)` right after the tile
  layers (where monsters/sfx overlay today), `front=true` at the current sfx
  draw point — before the foreground layer overlays of the static pass.
- R2. View origins differ per viewport:
  - **Dynamic**: parity origin — `viewLeftPx = mx * TILE_SIZE + (ox ? 8 : 0)`
    with `mx = m_cx / 2`, `ox = m_cx & 1`; fx and tiles must consume the same
    origin or big frames jitter at odd scroll parity. The origin is the true
    world left edge: the tile loop draws column `mx` at `-8` when `ox`, and
    `drawBossses(m_cx, ...)` consumes `m_cx * 8` (review fix: the original
    minus sign desynced fx from the grid by 16 px at odd parity).
  - **Static**: no parity — `viewLeftPx = mx * TILE_SIZE` (its sprite pass
    already calls `gatherSprites` with `ox = oy = 0`; `mx/my` are the
    player-centered clamped values).
  Screen position in both: `screenX = x - frameW/2 - viewLeftPx` (same for y).
- R3. Frames may exceed 16px and hang across tiles/map edges: normal clipping,
  no special casing.

## 4. Serialization

- N1. New save section immediately after the classic sfx section (count
  `uint16_t`, then raw 12-byte `fx_t` records, same `_W(&fx, sizeof(fx))`
  convention as the sfx block, see `docs/audits/game.cpp.audit.md` L40).
  `fx_t` is pure POD, so there is no runtime state to restore.
- N2. Classic `sfx_t` section and its read path are **untouched**.
- N3. `ENGINE_VERSION` (`src/game.cpp` L58, bumped to `(0x0200<<16)+0x0012`
  with this implementation) and the mirrored `EXPECTED_VERSION` (`tests/t_game.cpp` L52) are bumped
  together, per AGENTS.md BUG-H12 rule. Old saves stay rejected by the existing
  version gate (established behavior, `t_runtime.cpp` L37).

## 5. Frame sheet

- F1. One dedicated fx sheet PNG for now; `fxData_t.sheet` carries the same
  meaning as `bossData_t.sheet` and keeps room for more sets.
- F2. Per-frame rects are **not** a hand-maintained `g_fxSheet_data` table
  (superseded during implementation): they travel inside the sheet PNG as a
  private `obLT` metadata chunk (`u32 reserved, u32 version==2, u32 count,
  count × {u16 x, y, w, h}` LE, record order = imageID order).
  `parseFxSheetMeta()` (`src/gamesfx.cpp`) decodes it; the loaders
  (`TileSet::loadFx`, software boilerplate) cut the frames from the atlas.
  All fx declarations live in `src/gamesfx.h`, global storage (`g_fxData`)
  in `src/gamesfx.cpp`.
- F3. FrameID ranges: `back.base .. back.base+back.count-1` and
  `front.base .. front.base+front.count-1` are sheet-relative and disjoint per
  effect.

## 6. Compatibility

- C1. `sfx_t`, the `Sfx`/`SfxTimeout` enums, `CAnimator` special-case path, and
  the classic draw paths are not modified.
- C2. `fx_t` is additive state; nothing is removed or retyped from existing
  serialized structures.

## 7. Open items

- O1. ~~`FX_LOOP` semantics~~ Resolved 2026-11-19: `FX_REPEATABLE` entries
  loop (modulo wrap) and end on `ttl`; one-shots end on sequence length (I1/I4).
- O2. ~~Real art~~ Closed 2026-11-19: sheet + `obLT` rects in the private
  asset tree (`pixels/sfx.png`, 65 sequences from `pixels/sfx.idx`); table
  entries seeded from the `sfx.idx` runs; loader `TileSet::loadFx` wired,
  preload boilerplate ships commented per environment (section 3).
- O3. Whether any existing classic sfx migrate to fx later (per-effect
  decision; not required by this spec).
- O4. Telegraph ground-marker art (2026-11-21): every skill registry row
  carries `telegraphFx = -1`; the cast path spawns the marker iff `>= 0`, but
  no marker strip exists on the fx sheet. Placeholder shipped: a scheduled
  skill strike (`CGame::scheduleFx`) tints its target tile 25% red for the
  row's `telegraphTtl` warning window before the fx spawns (front pass,
  `fxDraw_t::telegraph`). Task:
  [`docs/plans/telegraph-art-task.md`](../plans/telegraph-art-task.md).
