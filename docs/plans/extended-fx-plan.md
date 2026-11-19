# Implementation plan — Extended FX (`fx_t`)

Spec: [extended-fx.md](../spec/gamespec/extended-fx.md) (2026-11-19).
Pixel-coordinate, multi-frame, two-layer effects owned by `CGame`.
**Hardware renderer (`EngineHW`) is implemented first**; software renderer
follows. Classic `sfx_t` is untouched.

## Phase 1 — data model + `CGame` state + tests

`src/gamesfx.h` (**existing** — add declarations; keep the frozen classic
`sfx_t`/`Sfx`/`SfxTimeout` block byte-identical, extend below it; add the
`<vector>`/`<functional>` and `sheetdata.h` includes): `Fx` enum, `FxFlags`
(`FX_ONESHOT` / `FX_REPEATABLE`), `using FxEndFn`, `fx_layer_t {base, count}`,
`fx_hitbox_t {dx, dy, w, h, type}` (declared-only provision; `type` mirrors
`BossData::HitBoxType`), `fxData_t {id, sheet,
speed, flags, ttl, w, h, back, front, damage, splashRadius, hitboxes,
hitboxCount, onEnd}`, pure-POD `fx_t {x, y, ox, oy, effectID, timer}` (12
bytes, its own wire record), `fxView_t`, `fxDraw_t`, extern `g_fxData` +
`g_fxSheet_data` (`tileinfo_t` rects), accessors `fxDataAt()`,
`fxFrameIndex()`.

`src/gamesfx.cpp` (**new** — global storage): define `g_fxData` (**empty
until art lands**; tests push synthetic entries into it) and
`g_fxSheet_data`, plus `fxDataAt()` bounds-checked lookup and
`fxFrameIndex()` (one-shot: clamp; repeatable: modulo per spec I4; spec I5:
the sequence index exists nowhere else). Picked up by `bin/refresh.py` into
`src/CMakeLists.txt`.

`src/game.h` / `src/game.cpp`:

- `game.h` includes `gamesfx.h` (classic sfx header; currently `game.h` only
  forward-declares `sfx_t` — the include is now required for the complete
  `fx_t` type). Member `std::vector<fx_t> m_fx;` after `m_sfx`
  (game.h L234); API after `purgeSfx()` (game.h L168):
  `getFx()`, `spawnFx(effectID, tileX, tileY)` (sets
  `ox/oy`, px = tile center), `spawnFxPx(effectID, px, py)`
  (`ox = oy = -1`), `tickFx()`, `getFxToDraw(view, front, out) const`.
  Spawns take no callback — `onEnd` is a `fxData_t` member.
- File-local `constexpr int FX_TILE_SIZE = 16` (game.cpp has no TILE_SIZE;
  engine.h L160 / gamemixin.h L105 own their copies).
- `tickFx()` next to `purgeSfx()` (game.cpp L1795): `timer++`; end when
  repeatable `timer >= ttl` else one-shot `timer >= max(back.count,
  front.count) * speed`; dangling `effectID` ends at once; fire
  **`def->onEnd(*this, fx)`** before erase.
- `getFxToDraw()`: skip layer `count == 0`; cull `fx.isWithin()` against the
  view rect inflated by table `w/2, h/2` (spec S2); emit `{x, y,
  layer.base + fxFrameIndex()}`.
- `m_fx.clear()` beside `m_sfx.clear()`: new-game game.cpp L521; load block
  clears its own.
- Save/load, directly after the sfx blocks — write game.cpp L1530-1536, read
  game.cpp L1413-1422: `uint16_t` count + raw `_W`/`_R` of `sizeof(fx_t)`
  records, mirroring the sfx loop line-for-line (`fx_t` is POD; the callback
  lives in the table, so load restores full behavior with no runtime state).
- **Bump `ENGINE_VERSION`** (game.cpp L58, `(0x0200<<16)+0x0011` -> `+0x0012`)
  **and** `EXPECTED_VERSION` (tests/t_game.cpp L52) in the same commit
  (AGENTS BUG-H12 rule).

`src/gamemixin.cpp`: `game.tickFx();` immediately after `game.purgeSfx();`
(L1411, inside the `!timeFrozen` block) — single tick call shared by both
renderers.

`tests/t_fx.h` / `tests/t_fx.cpp` (harness style of `t_game.cpp`:
`CGame::getGame()` singleton, `resetWorld()` mini-map; registered in
`tmain.cpp`, picked up by `python bin/refresh.py` into `tests/CMakeLists.txt`
— never hand-edit generated files):

- `test_fx_spawn`: `spawnFx` px conversion (`tile*16+8`), `ox/oy` set;
  `spawnFxPx` leaves `ox = oy = -1`; unknown `effectID` -> `-1`.
- `test_fx_lifetime`: entry is one-shot; ends at `frames*speed`, its table
  `onEnd` called exactly once, instance erased; repeatable entry survives
  past frame count and ends at `ttl`, `onEnd` called once.
- `test_fx_frame_index`: one-shot clamp at `count-1`; repeatable modulo wrap;
  back/front `base` offsets honored.
- `test_fx_cull`: in-view, margin case (center outside raw rect but inside
  `w/2` inflation), out-of-view zero draws; `front=false/true` select the
  right layer side.
- `test_fx_save_roundtrip`: spawn two fx (incl. one of a callback-bearing
  entry), `write()` to `FileMem`, mutate live list, `read()` back, compare all
  six POD fields incl. `ox/oy`; restored instance still ends through the
  table `onEnd`; classic sfx section unaffected.
- Synthetic `g_fxData` entries only — no `data/` asset reads.

Gate: `cmake -B build && cmake --build build && ctest --test-dir build`.

## Phase 2 — hardware renderer (`EngineHW`) — first renderer

- `src/engine_hw.h`: `ASSET_FXSHEET` added **before `MAX_ASSET`** (L143-158
  enum; append-only so existing manifest indices don't shift); member
  `TileSet m_tileset_fx;` beside `m_tileset_sheet0/1`; declare
  `void drawFx(const bool front, const int viewLeft, const int viewTop);`.
- `EngineHW::preloadAssets()` (engine_hw.cpp L123, after sheet1 load
  L196-199): **the load call ships as commented-out boilerplate, not the
  final version** — `preloadAssets` is environment-dependent (asset rows
  come from the private manifest `[assets]` section, `runtime.cpp` L1577-1581,
  one row per enum index). Boilerplate mirrors the boss/sheet0 line:
  `TileSet::load(m_renderer, AssetMan::getPrefix() + "pixels/" +
  m_assetFiles[ASSET_FXSHEET][IDX_FILENAME], g_fxSheet_data)` plus the
  `m_assetFiles.size() > ASSET_FXSHEET` manifest guard and `LOGI/LOGW` —
  commented, with a `ponytail:`-style note that the maintainer enables it per
  environment. Runtime stays safe: absent sheet => `tileCount() == 0` =>
  draw passes no-op; `getTile()` and `drawTile()` are already null-safe.
- `drawFx(front, viewLeft, viewTop)`: bail if empty set; build `fxView_t`
  `{viewLeft, viewTop, viewLeft + getWidth(), viewTop + getHeight()}`
  (logical px — `drawTile` applies `SCALE 2`); `m_game->getFxToDraw()`;
  blit `drawTile(m_tileset_fx.getTile(frameID), x - rect.w/2 - viewLeft,
  y - rect.h/2 - viewTop)`.
- Insert in **both viewport passes** (back after tile layers / before the
  `gatherSprites` loop; front after the sprite loop, before fg `overlays`):
  - `EngineHW::drawViewPortStatic` (engine_hw.cpp L668): sprite loop
    L704-715; origin `mx/my * TILE_SIZE` (L678-681), no parity.
  - `EngineHW::drawViewPortDynamic` (engine_hw.cpp L924): sprite loop
    L965-980, front before `overlays` L982; parity origin
    `mx * TILE_SIZE - (ox ? halfOffset : 0)` from `m_state.m_cx` L931-935.
- Test: table-level only (asset-free, reuses Phase 1 `getFxToDraw` cases);
  visual check manually in both viewports incl. odd scroll parity.

## Phase 3 — software renderer (`CGameMixin`)

- Member `std::unique_ptr<CFrameSet> m_fxSheet` beside `m_sheet0/1`
  (gamemixin.h L260-261). **Fill it in `CRuntime::preloadAssets()`
  (runtime.cpp L1269) as commented-out boilerplate only** — same rule as the
  hardware side: preload is environment-dependent (in-tree, `m_sheet0/1` are
  never assigned at all; only the `t_gamemixin.cpp` seam populates them).
  Boilerplate: PNG + `g_fxSheet_data` `tileinfo_t` rects -> `CFrameSet`
  (boss/sheet loader pattern), skip-not-fail when the asset is absent.
  Null `m_fxSheet` => draw passes no-op.
- **Two viewport passes**, back/front blit loops around each existing sprite/
  sfx overlay loop:
  - `drawViewPortDynamic` (gamemixin.cpp L738): sprite loop L799-834, front
    before `drawBossses` L837; parity origin per spec R2 (`m_cx` locals).
  - `drawViewPortStatic` (gamemixin.cpp L840): sprite loop L909-920, front
    before fg `overlays` blit L922; origin clamped `mx/my` (L850-853)
    `* TILE_SIZE`, no parity.
- Resolve `(*m_fxSheet)[frameID]`, blit centered via existing `drawTile`.
- Test: subclass seam per coverage-round2 ground rules — synthetic
  `CFrameSet`, assert changed-pixel bounding box centered on spawn px and
  off-view fx paint zero pixels, in **both** viewport functions.

## Phase 4 — docs + compliance

- Flip spec Open Items (O1 resolved by FX_REPEATABLE/ttl ruling 2026-11-19;
  O2 stays open until art; hitbox/damage/splash provision stays "declared,
  no consumer").
- Update `docs/compliance/` (hash, tally, checkpoint rows) **after** ctest is
  green; never bump a hash before testing.
- Run `itools/gen_coverage.py` / `gen_compliance.py` per spec README.
- valgrind pass on `t_fx` (std::function erase order, CFrameSet/TileSet
  ownership, no leaks).
- Move this plan to `docs/plans/archive` when fully implemented.

## Deliberately skipped (add when the trigger fires)

- Hitbox/damage/splash **mechanics** — fields + `fx_hitbox_t` shape exist
  (spec 1.4); wire tick hit-test vs player/actors when the first damaging
  effect (lightning strike) gets art + design.
- Per-instance `onEnd` overrides — callback is per-effect in the table; add a
  per-spawn callback only when two spawns of the same effect must behave
  differently.
- Stored per-instance `frame` index (spec I5) — derived from `timer` today;
  add the field only for per-instance pause or mid-life speed changes.
- Per-instance layer state — only if an effect needs randomized or
  per-instance-desynced layers.
- Multiple fx sheets — `sheet` field exists; vector of sets when sheet 2 has
  art.
- Migrating classic sfx to fx (spec O3) — per-effect call, needs art.

## Suggested commits

1. `feat(fx): extended fx data model + CGame spawn/tick/draw-list + save (v0x02000012)`
2. `feat(fx): EngineHW fx sheet load and static/dynamic draw passes`
3. `feat(fx): CGameMixin software renderer fx draw passes`
4. `docs(fx): extended fx compliance + coverage refresh`
