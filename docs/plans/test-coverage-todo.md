# TODO — unit test coverage (low-hanging fruit first)

Tracks adding unit tests to raise `cs3-tests` coverage of `src/`.
State of play lives in [`../compliance/src-checklist.md`](../compliance/src-checklist.md)
(70/121 verified) and [`../compliance/src-coverage.md`](../compliance/src-coverage.md)
(39.8% total). Ordered by effort ascending; work top-down.

Acceptance for every item: new `tests/t_*.cpp` following existing harness
style, registered in `tmain.cpp` + `tests/CMakeLists.txt`,
`cmake --build build && ctest --test-dir build` green, checklist hashes
refreshed via `python3 itools/gen_compliance.py` and boxes flipped.

## Tier 1 — pure logic, zero setup ✅ done

- [x] `randomz.cpp` — `t_randomz.cpp`: determinism, `reset()` replay, seed
      divergence, zero-state guard, `setTick` consistency, `range()` bounds,
      `PrecomputedRandom` wrap.
- [x] `parseargs.cpp` — `t_parseargs.cpp`: skill flags, `-p/-m/-w` values,
      combined switches, `--window` halving + sub-240 rejection, `--short`,
      positional level, unknown-option/missing-value failures, `-h/--help` appExit.
- [x] `shared/DotArray.cpp` — `t_dotarray.cpp`: add/flush/`operator[]`,
      `setLimit` clipping, horizontal `lineTab` full coverage, `circle`
      bounds + degenerate r=0. Fixed inverted `isEmpty()` (no callers).
- [x] `statedata.cpp` — `t_statedata.cpp`: non-empty, unique values/names,
      display↔enum agreement spot checks.

## Tier 2 — file-driven, fixtures already in repo

- [x] `bossdata.cpp` — `t_bossdata.cpp`: production fixture `data/bosses/`
      + `data/bosses/data/` (skip-not-fail when absent); bad-dir failure with
      clean state, `g_bossCount == BOSS_COUNT`, all four known types resolve
      with name/hp/type/path-algo sanity, unknown type null, `getHitboxes`
      (sheet, frame) probe + miss, `freeBossData` resets state.
      **Unblocks boss.cpp below.**
- [x] `boss.cpp` — `t_boss.cpp`: `CBoss` round-trip (x/y/type/hp/speed/state/aim)
      using production bossdata; clone constructed with the wrong boss type to
      prove `read()` adopts the stream's type; bogus type byte rejected.
      Skips with the same fixture guard.
- [x] `level.cpp` — `t_level.cpp`: all three `fetchLevel` dispatches —
      synthesized cs3 binary (64x64+7, convTable tile+attr spot checks incl.
      out-of-table clamp), generated MAPZ round-trip, text `.map` via
      `getChTile`, missing-file error. Fixed product bug found by test:
      `CMap::resize` on a `clear()`ed map had no layers → assert; now
      recreates the main layer.

## Tier 3 — need singleton or fixture scaffolding first

- [x] `ai_path.cpp` — `t_ai_path.cpp`: A*/BFS/AStarSmooth/LOS `findPath`
      on a 10x10 map (open grid optimal length, gap-detour routing,
      sealed-wall no-path, out-of-bounds goals) driven by a minimal
      `ISprite` (tile==0xff blocks) mirroring `CGame::translate`. Fixed
      three product bugs found by test: `g_deltas` axes were rotated 90°
      vs the engine aim convention (sprite validity checked a perpendicular
      cell); A*/BFS/AStarSmooth probed `canMove` after moving to `newPos`
      (checked one cell past the step, never the entered cell) and LOS had
      the same off-by-one; `AStarSmooth::smoothPath` probed LOS from the
      sprite's static position and then rejected every smoothed jump >1
      cell, so it always returned an empty path.
      (CPath serialization + `getPathAlgo` already covered by `t_actor`.)
- [x] `assetman.cpp` — `t_assetman.cpp`: `defaultPrefix`/`addTrailSlash`/
      `setPrefix` slash handling, `read()` of `tests/in/test.rec` byte count
      + `terminator` NUL, missing-file empty result.
- [x] `layerdata.cpp` / `tilesdata.cpp` / `colormap.cpp` / `sheetdata.cpp` /
      `names.cpp` — `t_tables.cpp`: `resolvePath` (bare + `Path::` prefix),
      `resolveTile`/`resolveSound`/`resolveColor` hit+miss sentinels,
      `resolveFlags` OR/`0`/numeric-fallback; `parseColorMaps` sections,
      clear-on-reparse, invalid-section/kv-before-section/bad-hex rejection;
      `loadTileLayer` JSON defaults, `next:-1`→0, field parse, baseIdx
      offset, missing-file failure; sheet tables non-empty; `getChTile`
      128-entry wrap.
- [ ] `logger.cpp` — log line lands in output file; levels filter correctly.
- [x] `game_ai.cpp` — AI tick logic against singleton + map fixture
      (982 lines, biggest untested block; do after ai_path helper exists).
      Phases 1–4 done in `t_game_ai.cpp`: dispatcher/movers, MonsterV3/
      spawnBullet, vamplant, egg, barrel blast/fuse, bullets, spawns
      (queueRespawn + manageSpawns delay/block/active), and bosses
      (path/bullet/hitbox-contact/states). Seams: public `rebuildMonsterGrid()`
      (Phase 0), `getBosses()` + public `addHealth` (Phase 4).
      Plan archived: [game_ai-test-plan.md](archive/game_ai-test-plan.md).
- [x] `game.cpp` rules — filled the empty `t_game.cpp` stub: `validateSignature`
      (sig/version accept+reject), `addKey`/`hasKey` inventory (fill, dedupe,
      overflow), `purgeSfx` timeout drain, `scanPos` (swamp/deadly/solid/water
      + granular quadrant gates per aim) and `isGranularSolidFromPos` via a
      scratch overlay layer + saved/restored `g_layerdata` entries, and all
      `checkClosure` branches (death, level-complete, exit-key gate +
      EVENT_EXIT_OPENED/reveal, chute). Seams: public `stats()`. Fixed
      three product bugs found by test: `addKey` looped `sizeof(m_keys)`
      (12) instead of `MAX_KEYS`, so a 7th key aliased/wrote past the
      indicators array; `CAnimator` left `m_tileLayer`/`m_tileMainLayer`
      uninitialized until `reloadTileData()`, so `getLayerTile` on a fresh
      animator read garbage into `g_layerdata` out of bounds (now
      value-initialized); `validateSignature` logged `&signature` (pointer
      bytes) instead of the signature string. `game.cpp` 32.0% → 42.4%.

## Tier 4 — not worth unit-testing (defer / integrate manually)

- `engine.cpp`, `engine_hw.cpp`, `tileset_tex.cpp` — SDL render context.
- `gameui.cpp`, `menu.cpp`, `menuitem.cpp`, `menumanager.cpp` — UI state
  machines bound to the engine loop.
- `main.cpp`, `steam_helper.cpp` — entry point / platform SDK.
- `mu_sdl.cpp`, `sn_sdl.cpp` — SDL audio backends.
- `tilesdebug.cpp` — debug dump helpers.
