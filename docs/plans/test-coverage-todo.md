# TODO — unit test coverage (low-hanging fruit first)

Tracks adding unit tests to raise `cs3-tests` coverage of `src/`.
State of play lives in [`../compliance/src-checklist.md`](../compliance/src-checklist.md)
(71/121 verified) and [`../compliance/src-coverage.md`](../compliance/src-coverage.md)
(42.2% total). Ordered by effort ascending; work top-down.

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
- [x] `map.cpp` / `maparch.cpp` negative paths — `test_map_corrupt()` and
      `test_maparch_corrupt()`: truncated/corrupt MAPZ and MAAZ byte streams
      rejected with an error (header, signature, version, layer/attribute,
      title-size cap, index table and index-entry offsets, missing files);
      legacy V0 MAPZ read + its truncated rejection; optional-XDR absence
      accepted; `indexFromFile`/`indexFromMemory` validator rejects. Fixed
      product bug found by test: `CMap::readCommon` legacy-V0 error branch
      called `getMainLayer()->lastError()` after `m_layers.clear()` and
      before pushing the layer — out-of-bounds `m_layers[0]` (aborted);
      now uses the local layer. `map.cpp` 70.4% → 79.5%,
      `maparch.cpp` 59.9% → 80.2%.

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
- [x] `logger.cpp` — `t_logger.cpp`: LOGI/LOGW/LOGE lines land in the
      `setOutputFile` target with `[file/LEVEL]` tags, output-file switch
      closes and reopens, `setLevel(L_WARN)` filters INFO but keeps
      WARN/ERROR, format-trailing-`\n` is not doubled; reset via
      `setOutputFile("")` (fopen failure → stdout routing). LOGF (exits)
      and the Android branches not exercised. `logger.cpp` 68.4% → 94.7%.
      Note: `src/shared/logger.h` duplicates `src/logger.h` (same include
      guard); left unverified.
- [x] `gamemixin.cpp` rules scraps — `t_gamemixin.cpp` via a headless
      `TestRuntime : CRuntime` subclass exposing the protected members
      (same pattern `t_runtime` uses): `rankUserScore` (no-placement
      below-worst, mid-table insert + shift + level/name stamp),
      `getEventText` (8 fixed event banners, SUGAR_RUSH/EXIT_OPENED tick
      flashing, YUMMY/SHIELD stat interpolation, MSG0 lines from map
      states, unhandled-event empty), `manageTimer` (decrement, expiry →
      `incTimeTaken` + TICK_RATE reload, TIMEOUT 100→99 / 15→14; the
      TIMEOUT==1 kill/level-intro path enters game flow + music and is
      not exercised), `moveCamera` (all four deadzone branches + stay).
      `gamemixin.cpp` 7.8% → 15.7%; remainder is SDL draw/input.
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

## Round 2 (2026-11-16) — re-tiering the leftovers

Detailed phase-by-phase plan: [coverage-round2-test-plan.md](coverage-round2-test-plan.md).

`grep -c SDL_` on the files Tier 4 wrote off as "not worth unit-testing":
`menu.cpp`, `menuitem.cpp`, `menumanager.cpp`, `gameui.cpp`, `engine.cpp`,
`gamemixin.cpp`, `animator.cpp`, `shared/helper.cpp`, `shared/Frame.cpp`,
`shared/FrameSet.cpp`, `shared/PngMagic.cpp` → **0 SDL references each**.
They are UI state machines and a software rasteriser, not GPU code. The old
"bound to the engine loop" label was an assumption, not a dependency. Ordered
by lines-of-coverage per unit of effort; `N` = gcov executable lines at `%`.

## Tier 4 — pure logic, no new scaffolding (~1320 lines, 42% → ~53%)

- [x] `menu.cpp` (105 @ 8.6%) + `menuitem.cpp` (93 @ 0%) — one `t_menu.cpp`.
      `addItem`/`at`/`size`, `up`/`down` ends return false (no wrap), `setCurrent`
      clamping, range items `left`/`right` clamps (raw value steps by 1;
      `m_factor`/`m_start` only scale `str()`), options items, `disable`
      (caret still visits disabled items), `role`/`userData`/`type`
      accessors, `str()` rendering, `containsRole`/`removeRole`/
      `getItemByRole`/`moveItem` (caret tracking). No fixture, no singleton.
      menu.cpp 98.1%, menuitem.cpp 97.8%.
- [x] `menumanager.cpp` (48 @ 16.7%) — extend `t_menu.cpp`: `get()` menus
      pre-created, `setActive`/`isMenuActive`/`toggleMenuActive`,
      `menuItemAt(x,y)` hit-test (y-band only, x ignored, disabled rows are
      hits), `followPointer`. menumanager.cpp 95.8%.
- [x] `gameui.cpp` (37 @ 37.8%) — `addButton`/`clear`/`at`/`buttons`,
      `show`/`hide`/`isVisible`, `width`/`height`/`margin` geometry (max
      button extents; margin is an independent knob). gameui.cpp 100%.
- [x] `shared/helper.cpp` (112 @ 33.9%) — `t_helper`: `copyFile`/`concat`
      round-trip + missing-source error strings, `compressData` (all three
      overloads) round-trip through zlib `uncompress` (`zlibstatic` is
      already linked), `readFile` hit/miss, `toUpper`/`upperClean` table,
      `getUUID` format + 100-sample uniqueness. 87.5%.
- [x] `shared/Frame.cpp` (514 @ 29.0%) — `tests/t_frame.cpp` (5 suites):
      `flipH`/`flipV`/`rotate`/`shrink`/`enlarge`,
      `shiftUP|DOWN|LEFT|RIGHT` with and without `wrap`, `floodFill`,
      `floodFillAlpha`, `clip` (incl. throw on over-wide extent),
      `fill`/`isEmpty`/`inverse`/`copy`, `shadow`/`fade`,
      `draw(vector<Dot>)`+`save` incl. colour-only/alpha-only modes,
      `drawAt` transparency, `toNet`, `getChunkType`,
      `setTransparency`/`hasTransparency`/`setTopPixelAsTranparency`,
      `split`/`explode` (both overloads), `write`/`read` round-trip via
      `CFileWrap`, `toBmp` header + `toPng` signature/IHDR/IEND.
      Frame.cpp 85.8%, DotArray.cpp 94.9%.
- [x] `shared/FrameSet.cpp` (683 @ 44.1%) — `tests/t_frameset.cpp`
      extension: `extract` over every `tests/in` family (ima/imc1/obl3/
      obl4/obl5/mcx) with error-free assertions, garbage/empty/foreign-
      signature rejection on both `extract` and `read`, container ops
      (add/`++`/`[]`/insertAt/move/removeAt/set/resize/clear), tags/UUID/
      name, `toSubset` and `operator=` deep copies, `setCurrFrame`
      no-clamp pinned. 70.3%. `operator--` non-empty path is UB — left
      unexercised, finding recorded.
- [x] `shared/PngMagic.cpp` (310 @ 39.7%) — DONE via `tests/t_pngmagic.cpp`
      extension: refs parse + `skipOblt` split-vs-whole, synthetic encode→
      decode identity incl. transparent frame, bad-signature/bad-CRC/
      truncated rejection (first `shared/CRC.h` exercise), `orgPos` offset,
      hand-built 4-bit indexed PNG with tRNS + obLT v2 metadata, full 8-bit
      row-filter matrix (NONE/SUB/UP/AVERAGE/PAETH), split-IDAT, bad filter
      byte and corrupt-zlib rejection. 89.0%. obLT header is native-endian;
      "unsupported oblt version" return is unreachable — findings recorded.
- [ ] `animator.cpp` (83 @ 24.1%) — `animate()` frame advance/loop,
      `getSpecialInfo`, `reloadTileData` with the synthetic `g_layerdata`
      entries `t_game.cpp` already builds.

## Tier 5 — needs a stub subclass, still no SDL (~960 lines, ~53% → ~61%)

- [ ] `engine.cpp` (344 @ 0%) — `Engine` is an abstract base with 0 SDL calls.
      A ~20-line `TestEngine : Engine` with empty pure-virtual bodies (same
      expose-protected trick `t_gamemixin.cpp` uses) unlocks the real logic:
      `gatherSprites` viewport culling (`cameraContext_t` zoom/scale math —
      the best bug surface in the file), `getEventText`, `initLevelSummary` +
      `drawLevelSummary`, `drawScores`, `resizeGameMenu`,
      `initVirtualKeyboard`/`drawVirtualKeyboard`, `getEventText` parity with
      `CGameMixin::getEventText` (two near-duplicate implementations — a test
      that asserts both agree is the cheap guard against one drifting).
- [ ] `gamemixin.cpp` (1454 @ 15.7%) — the single biggest block in `src/`
      (~1225 uncovered lines) and it calls no SDL: every `draw*` takes a
      `CFrame &` and rasterises in software. Drive it from the existing
      headless `TestRuntime : CRuntime` + map/bossdata fixtures and assert
      on invariants (changed-pixel counts, non-empty bounding boxes, layer
      order, `tile2Frame`/`drawTile` frame identity), not goldens:
      `drawTile`, `tile2Frame`, `drawViewPortStatic`, `drawViewPortDynamic`,
      `drawBossses`, `drawScores`, `drawLevelIntro`, `drawScreen`,
      `drawMenu`. Then the non-draw rules: `handleFunctionKeys_Game`,
      `handleInputString`, `changeZoom`, `setWidth`/`setHeight`,
      `enableHiScore`, `setSkill`. `mainLoop`/`manageGamePlay` last — they
      fan out into audio and menu state; assert state transitions over N
      ticks only.

## Tier 6 — SDL-bound: one dummy-driver test, no mocks (~570 lines, → ~66%)

- [ ] `runtime.cpp` (1570 @ 2.3%) + `engine_hw.cpp` (701 @ 0%) + `paint()` /
      `takeScreenshot()` — set `SDL_VIDEODRIVER=dummy` (and
      `SDL_AUDIODRIVER=dummy`) in the test, `SDL_Init(SDL_INIT_VIDEO)`, create
      the window through the existing `CRuntime` path, then call `paint`,
      `drawMenu`, `takeScreenshot` and the `onMouseEvent`/`onGamePadEvent`
      dispatchers with hand-built `SDL_Event`s. Skip the suite (return true,
      log a warning) when `SDL_Init` fails so headless CI cannot go red.
      Put it in a separate `cs3-tests-sdl` binary registered with its own
      `add_test` if it destabilises the main suite; do not mock SDL, and do
      not refactor for testability.
- [ ] Out of scope, keep `[ ]` in the checklist with a note:
      `main.cpp`, `steam_helper.cpp`, `mu_sdl.cpp`, `sn_sdl.cpp`,
      `tileset_tex.cpp`, `tilesdebug.cpp`, header-only tables
      (`animzdata.h`, `colormasks.h`, `font8x8.h`, `tilesdefs.h`, ...).

## Tier 7 — process gaps found while re-tiering

- [ ] No workflow runs `ctest` (`grep -l ctest .github/workflows/*` → empty).
      `docs/spec/README.md` requires tests wired to CI. Add one small
      `linux-tests.yml`: configure + build `src_lib` and `cs3-tests`, run
      `ctest`. Coverage generation stays local/manual.
- [ ] `itools/gen_coverage.py` reports per-file only, which is how the Tier 4
      mislabelling survived. Have it also list the worst uncovered functions
      per file (`gcov -f -n`, filter `_ZSt`/`std::`/`operator` noise) so the
      next round of targets comes from the report.
