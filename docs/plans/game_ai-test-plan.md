# Action plan — test `game_ai.cpp` (982 lines)

Tracker: Tier 3 item in [test-coverage-todo.md](test-coverage-todo.md).
Current compliance: 69/121 verified. `src/game_ai.cpp`/`.h` are the last
major untested block.

## What the file is

15 `CGame` methods: per-tick AI for spawns, bosses, monsters, bullets.
Entry points (public): `manageMonsters(ticks)`, `manageBosses(ticks)`,
`manageSpawns(ticks)`. Everything else (`handleMonster`, `handleMonsterV3`,
`handleDrone`, `handleVamPlant`, `handleCrusher`, `handleIceCube`,
`handleEgg`, `handleBarrel`, `handleBullet`, `blastRadius`, `spawnBullet`,
`handleBossPath`, `handleBossBullet`, `handleBossHitboxContact`,
`pushChain`, `queueRespawn`) is private but mostly reachable through the
three public `manage*` drivers. `isPushable` is public static.

## Existing hooks (no new seams needed for Phases 1–2)

- `CGame::getGame()` singleton + `CGame::getMap().resize(10, 10, 0, true)`
  (pattern proven in `t_ai_path.cpp`).
- `getMonsters()` returns `std::vector<CActor> &` — inject actors directly
  (`CActor(Pos, type, aim)` ctor exists).
- `player()` returns `CActor &` — position the chase target via
  `move(x, y)`.
- `manageMonsters` dispatches on `actor.type()` and the TileDef under the
  actor (`getTileDef(m_map.at(pos))`), so planting the right tile + actor
  type routes into each `handle*` branch.
- Determinism: reset the PRNG (`randomz`/`g_random` — see `t_randomz.cpp`)
  at each scenario start; pick `ticks` values that satisfy the
  `speeds[i] = ticks % i` slicing and `RESPAWN_SLICE` masks.

## Test seams to add first (Phase 0, product-side, 2 lines)

**Done 2026-11-16 (adjusted):** the seam Phases 1–2 actually needed was
`rebuildMonsterGrid()` made public in `game.h` — `getMonsters()` injection
alone leaves `m_monsterGrid` stale, and `shadowActorMove`/`findMonsterAt`
are grid-driven, so injected actors could not move. Tests call it after
injecting/clearing. `std::vector<CBoss> &getBosses()` deferred: no caller
until Phase 4 (`const bosses()` already exists for read-only checks).
`m_spawns` seam still deliberately absent (see below).

- `std::vector<CBoss> &getBosses();` mirroring `getMonsters()` — without it
  `m_bosses` is unreachable and ~250 boss lines stay dark. `CBoss` ctor
  requires valid bossdata: load `data/bosses` with the skip-guard pattern
  from `t_boss.cpp`.
- `m_spawns` is only ever populated by private `queueRespawn`, itself only
  called on monster-death paths — test it via those paths (Phase 3), do NOT
  add an accessor unless the death path turns out unconstructable.

## Phases (each ends green + checklist refresh)

### Phase 1 — dispatcher + simple movers (largest, cheapest wins) ✅ DONE 2026-11-16
Implemented in `tests/t_game_ai.{h,cpp}` (`test_game_ai`, runs last in
`tmain.cpp`). Notes from implementation:
- Tile choice via a table scan (`tileOf`) asserting `getTileDef(id).type`,
  per the rule below — no hardcoded tile ids.
- Ticks 840/841 (lcm(1..8) and 29²) select all-pass / all-fail `speeds[]`.
- Player damage asserts skipped: `m_health` starts at 0 so negative
  `addHealth` is unobservable through the public API; phases that need
  damage will add a health seam then.
- `isPushable` + `pushChain` (last Phase-2 bullet) covered transitively by
  the icecube push scenarios.
Via `manageMonsters`:
- idle-attr short-circuit (`ATTR_IDLE_MIN..MAX`, wakes when player near).
- `speeds[def.speed]` gating (tick values where a monster must NOT move).
- `handleMonster`: moves toward player when in aggro range, wanders when far
  (seed PRNG, assert movement class, not exact tile).
- `handleDrone`, `handleCrusher` (uses `speeds[]`), `handleIceCube` (fall).
- Tile choice: do not hardcode "tile X is a monster" — assert
  `getTileDef(id).type == expected` inside the test so a table edit fails
  the test loudly instead of silently skipping the branch.

### Phase 2 — interactions ✅ DONE 2026-11-16
Implemented in `tests/t_game_ai.cpp` (`testMonsterV3`, `testVamPlant`,
`testEggHatch`, `testBullet`, `testBarrelBlast`, `testBarrelFuse`).
Notes from implementation:
- Plan expected "bullet hits monster → monster deleted"; the implementation
  does not do that: a bullet cannot enter a monster's tile, so it dies and
  the monster is untouched. The tests assert that reality; `blastRadius`
  (barrel explosions) is what deletes mob monsters, verified there.
- `spawnBullet` (private) covered transitively via `handleMonsterV3`: on a
  tick passing both `speeds[def.speed]` and `ticks % def.rate` the shooter
  spawns a `TYPE_FIREBALL` (count grows); a non-rate active tick fires
  nothing.
- Barrel fuse timing: the fuse is lit after the barrel's own index was
  already processed that tick, so the barrel arms at full `BARREL_TTL`
  (20, mirrored as a test constant — the scenario fails if it drifts).
- Blast victims placed at vector indices after the barrel are deleted
  before their own `handle*` runs, keeping the survivor set deterministic.
- `handleBullet` death paths covered: advance, blocked by monster tile,
  blocked by boulder tile (tile restored from PU), TTL+algo expiry.

### Phase 3 — spawns
- Death of a respawn-flagged monster (from Phase 2 barrel scenario) queues
  a spawn → `manageSpawns(ticks)` respects `delay` (no spawn too early),
  blocked cell (existing monster at spawn point) defers, delay expiry
  re-adds the actor at the spawn point, decrements `active`.

### Phase 4 — bosses (needs Phase 0 accessor)
- `handleBossPath`: with `data/bosses` loaded, place boss, set path algo
  (`BossData::ASTAR` vs `NONE`); assert cached path updates and boss moves
  (`t_ai_path.cpp` guarantees the pathfinder itself is sane now).
- `handleBossBullet`: boss fire spawns bullets at cadence.
- `handleBossHitboxContact`: player overlapping boss hitbox → damage/kill
  path (`isPlayerDead()`/`killPlayer()` observable via public API).
- `manageBosses` TTL handling (`BossData::NoTTL`, expiry → boss removed).

## Risks / notes

- Shared singleton: this test mutates monsters/bosses/player/map; clear all
  at exit (`getMonsters().clear()`, `getBosses().clear()`, `fillMap` reset)
  so `t_game_ai` runs LAST in `tmain.cpp`.
- Negative/edge paths will print expected LOGE noise (like existing suites).
- `handleBossHitboxContact` touches `m_gameStats`/gameover flow — if a
  default-constructed CGame crashes there, prefer fixing the product at
  root cause (precedent: `resize`, `g_deltas`) over skipping the case.
- PRNG: `g_random` advances with game ticks in some branches; re-seed per
  scenario.

## Acceptance (same recipe as all items)

1. `tests/t_game_ai.{h,cpp}` registered in `tmain.cpp` (append last) +
   `tests/CMakeLists.txt`.
2. `cmake -B build && cmake --build build` && `ctest --test-dir build` green.
3. COVERED entries for `src/game_ai` (and `src/actor`-adjacent stems if the
   phases finally justify them) in `itools/gen_compliance.py`; regen, flip
   boxes only after green.
4. Product bugs found → fixed at root cause, noted here and in
   test-coverage-todo.md; fixed bugs moved per `docs/bugs/resolved` rule.
5. Expect 69 → ~72/121 (game_ai.cpp/.h, plus any newly fully-covered files).
