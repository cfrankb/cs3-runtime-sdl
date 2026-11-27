# Enemy Movement — engine-grounded implementation plan

Status: draft (2026-11-27). This file previously held a byte-identical
copy of the generic projectile-trajectories spec; that spec now lives
adapted in [ai-projectile-trajectories.md](ai-projectile-trajectories.md).
This file is (and stays) the enemy-movement plan.

## 0. Ground truth (what enemy movement actually is here)

All movement is 4-direction `JoyAim` steps of one tile cell per
action-tick, gated by per-edge `ISprite::canMove` (`CActor::canMove`,
`CBoss::canMove`). No floats, no velocity, no `dt`. Verified sources:

- **Monsters** (`CGame::handleMonsterV3`, `src/game_ai.cpp`): action
  ticks are gated by the `def.speed` class → `speeds[]` modulo table.
  Inside `def.range` of the player, `TYPE_MONSTERV3`/`TYPE_GHOST`
  actors run `startPath(m_player.pos(), def.path, -1)` +
  `followPath`; outside range they fall to the legacy wander tail
  (`findNextDir`, `AI_REVERSE`/`AI_ROUND`/`AI_FOCUS`/`AI_STICKY` flags,
  `src/sprtypes.h`). Ranged monsters shoot `def.bullet` along the path
  aim gated by `def.rate`. **Do not remove the type guard** at the
  pathfinding branch — it is load-bearing (see code comment).
- **Bosses** (`CGame::handleBossPath`): move in the half-tile space
  (`BOSS_GRANULAR_FACTOR = 2`), `followPath` with `boss.data()->path`,
  plus the maintainer-ruled fallback: exactly one greedy cardinal move
  per tick, and only when path AI did not move. Engagement radii are
  boss data (`distance_chase/pursuit/attack`).
- **Path machinery:** `CPath` caches per-actor step lists with a
  re-path timeout; `CPath::getPathAlgo` maps the append-only
  `BossData::Path` enum to the singletons in `PathData`. 14 selectors
  ship today: `NONE ASTAR BFS LOS ASTAR_SMOOTH DIJKSTRA FLOWFIELD
  THETASTAR RAYCAST RAYCAST_BULLET ASTAR_BUDGET GBFS FLEE ASTAR_SAFE`
  (ini: `path=` on tile defs, `algo=` on boss paths / skill lines;
  state of the world: [docs/spec/ipath-implementations.md](../spec/ipath-implementations.md)).

So the algorithm family is *done* for movement purposes. The remaining
enemy-movement work is (a) wiring behavior tiers onto knobs, (b) one
genuinely missing motion primitive, (c) a low-cost idle patrol that
supplements (never replaces) the wall-hugging legacy tail, (d) tuning
surface. No new search structures.

## 1. Task 1 — Behavior tiers as content (no new code)

Pick per-monster personality from existing selectors; encode the tier
table in `docs/spec/` alongside `ipath-implementations.md`, apply via
`TileDef.path` / bosses.ini, not C++:

| Tier | Motion | Selector | Why (per spec summary) |
|---|---|---|---|
| Dumb wanderer | wander tail only | `path=NONE` | legacy `findNextDir` + AI flags, zero search cost |
| Dumb chaser | beeline, dumb turns | `LOS` | greedy, no backtrack — breaks on walls like a classic Doom-2D mob |
| Smart chaser | optimal pursuit | `ASTAR_BUDGET` | bounded 256-node expansions + partial path; strictly safer than plain `ASTAR` for long corridors (use `ASTAR` only where optimality must be proven) |
| Straight-line shooter mob | ray + detonate at first wall | keep `algo=RAYCAST_BULLET` on the bullet, `path=LOS` on the body | matches the bullet plan's contract |
| Pack swarm | many bodies, one goal | `FLOWFIELD` | optimal, gradient descent; **caveat:** the field is computed per-actor through each actor's `CPath` cache, not shared. Fine at current monster counts; share one field per (level, goal) only if profiling shows the cost — that is the upgrade path, don't prebuild it. |
| Coward / retreater | maximize distance | `FLEE` | already implemented (`flees() == true`); needs content wiring + a trigger, Task 2 |
| Boss | half-tile pursuit | boss `path=` + greedy fallback | untouched by this plan |

## 2. Task 2 — Wounded retreat wiring (small hook, no serialization)

`Path::FLEE` exists but nothing selects it at runtime. Minimal hook in
`handleMonsterV3`, before the chase branch: a TileDef-driven retreat
condition. Monsters have **no per-instance HP** (`CActor` carries
health only via `TileDef` as player damage) — so the honest trigger is
not "HP < X". Cheapest real trigger: `AI_COWER`-style behavior gated
on `def.range` proximity *after* the monster has been touched, which
needs a touched-flag... which is new state. **Decision point for the
maintainer before implementation**:
- Option A (zero state): `path=FLEE` is a static content choice —
  cowards always flee until cornered; cornered `followPath` returns
  `Blocked` → they fall into the wander tail and bite. No code at all.
- Option B (per-instance flag): a `uint8_t` flag on `CActor` = actor
  wire-record change = `ENGINE_VERSION` bump + `tests/t_game.cpp`
  `EXPECTED_VERSION` mirror (AGENTS.md). Only pick B if Option A plays
  badly in a real level.
Lazy default: ship A, reopen only with playtest evidence.

## 3. Task 3 — Orbiter motion: reuse `Path::ORBIT`

Enemies that circle the player instead of hairpinning are the same
primitive as the projectile plan's turn-rate-limited homing orb:
`Path::ORBIT` (reversal-forbidden greedy router), specified in full in
[ai-projectile-trajectories.md §4](ai-projectile-trajectories.md). It
serves monsters via `path=ORBIT` with zero extra code. Do not
implement a second orbiting routine here.

## 4. Task 4 — Idle patrol supplementing the wall-hug — **DONE**

Implemented 2026-11-27 as specified in
[archive/ai-patrol-plan.md](archive/ai-patrol-plan.md): `AI_PATROL`
supplement in `CGame::handleMonsterV3` (`src/game_ai.cpp`), demo row
`TILES_ZOMBIE` (`src/tilesdata.cpp`), contract in
[`../spec/gamespec/patrol-ai.md`](../spec/gamespec/patrol-ai.md),
five checks in `tests/unit/t_game_ai.cpp` ("patrol ..." scenarios),
full `ctest` green. Details below are kept as the design record.

Out of `def.range`, `TYPE_MONSTERV3`/`TYPE_GHOST` fall to the legacy
wander tail (`game_ai.cpp:579-599`): `CActor::findNextDir`
(`actor.cpp:323`) walks the fixed `g_aims` preference table and takes
the first `canMove` step — deterministic, so it hugs one wall.
Maintainer rulings: **`AI_PATROL = 0x10`** (alias of the zero-consumer
`AI_SMART`, kept in place), and patrol **supplements** the wall-hug,
never replaces or mutates it: with the flag on, one leg of a 4-post
cardinal loop anchored on the already-serialized `CActor::m_origin`
is tried per tick via `startPath(post, Path::LOS, -1)`; any tick it
cannot step, the **unchanged** tail runs. Flag off (all shipped
content) = byte-for-byte today's behavior; legacy `TYPE_MONSTER`
dispatch untouched. Pure tick+origin function, no RNG, no new state,
no wire change, no `ENGINE_VERSION` bump.

Full design, code, cost bounds and test list:
**[ai-patrol-plan.md](ai-patrol-plan.md)** — the detailed
implementation plan for this task.

## 5. Non-goals (don't sneak in)

- **Half/quarter-tile monster movement** (adopting the boss
  `GRANULAR_FACTOR` for `TYPE_MONSTERV3`): touches `canMove`,
  monster-grid bookkeeping (`updateMonsterGrid`/`findMonsterAt` are
  full-tile), rendering (tile-stamped actors) and the wire record. A
  subsystem, not a tuning knob. Revisit only if sub-tile rendering
  lands for the bullet plan's deferred float pass.
- **Shared flow fields / group coordination / steering (separation,
  flocking):** O(BFS) per level is cheap but the cache is per-actor
  today; measure first, share later.
- **New pathfinding algorithms.** 14 shipped; the family is
  feature-complete for this plan (see ipath brief §0 scope check).
- **Speed variety beyond the `speeds[]` modulo table** (accel,
  dash-cycles): the table gives integer action-rate classes already;
  add a class row, not a new motion model.

## 6. Verification loop (AGENTS.md)

Tasks 2A/3 touch content + tables only; Task 3's algorithm lands with
the bullet plan's change. Either way:

1. `cmake -B build && cmake --build build` (pedantic-clean, all
   toolchains).
2. `ctest --test-dir build` (min: `t_ai_path`, `t_tables`, `t_actor`,
   `t_bossdata`).
3. After green: `python itools/gen_compliance.py`; update
   `docs/spec/ipath-implementations.md` "in-use content" column for
   any selector this plan wires into content.
4. Move this plan to `docs/plans/archive/`.
