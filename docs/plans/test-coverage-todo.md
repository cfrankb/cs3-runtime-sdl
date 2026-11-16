# TODO — unit test coverage (low-hanging fruit first)

Tracks adding unit tests to raise `cs3-tests` coverage of `src/`.
State of play lives in [`../compliance/src-checklist.md`](../compliance/src-checklist.md)
(49/121 verified). Ordered by effort ascending; work top-down.

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
- [ ] `boss.cpp` — `CBoss` serialize/deserialize round-trip (needs bossdata;
      `write()` derefs `m_bossData`).
- [ ] `level.cpp` — `fetchLevel(CMap&, fname)` against a real level file from
      `data/`; verify map title/dims/attrs.

## Tier 3 — need singleton or fixture scaffolding first

- [ ] `ai_path.cpp` — A\*/BFS/LOS/AStarSmooth `findPath` behavior: small
      walkable/blocked map via `CGame::getGame()` (helper in `thelper`);
      assert reachable path vs `NoValidPath` around a wall.
      (CPath serialization + `getPathAlgo` already covered by `t_actor`.)
- [ ] `assetman.cpp` — asset file loading against `tests/in/ima`, `mcx`,
      `obl*` fixtures.
- [ ] `layerdata.cpp` / `tilesdata.cpp` / `colormap.cpp` / `sheetdata.cpp` /
      `names.cpp` — table loaders (`loadTileLayer` etc.) against sample data
      files; verify loaded entries and bounds.
- [ ] `logger.cpp` — log line lands in output file; levels filter correctly.
- [ ] `game_ai.cpp` — AI tick logic against singleton + map fixture
      (982 lines, biggest untested block; do after ai_path helper exists).

## Tier 4 — not worth unit-testing (defer / integrate manually)

- `engine.cpp`, `engine_hw.cpp`, `tileset_tex.cpp` — SDL render context.
- `gameui.cpp`, `menu.cpp`, `menuitem.cpp`, `menumanager.cpp` — UI state
  machines bound to the engine loop.
- `main.cpp`, `steam_helper.cpp` — entry point / platform SDK.
- `mu_sdl.cpp`, `sn_sdl.cpp` — SDL audio backends.
- `tilesdebug.cpp` — debug dump helpers.
