# Projectile Trajectories — engine-grounded implementation plan

Status: draft (2026-11-27). Supersedes the generic floating-point spec
this file previously duplicated verbatim from the pre-adaptation draft;
the intent mapping below preserves every feature of that spec.
Sibling: [ai-enemy-movement.md](ai-enemy-movement.md) (shares Task 3's
`ORBIT` algorithm). Brief for new algorithms:
[docs/briefs/ipath.md](../briefs/ipath.md).

## 0. Ground truth (what a bullet actually is here)

The original spec assumed a float pixel world (`float x,y`, `vel`,
`isTileBlocked`, `hasLOS`, per-frame `dt`). None of that exists. Actual
model, verified against the sources:

- **Tile cells, not pixels.** `CActor` stores `uint8_t m_x, m_y`
  (`src/actor.h`); `Pos` is `int16_t` tile coords (`src/map.h`).
  `TILE_SIZE = 16` is a render-path constant only (`src/engine.h`).
- **Tick-driven, one step per tick.** Movement is one 4-direction
  `JoyAim` step through `ISprite::canMove/move` (per-edge walkability,
  `CActor::canMove`). Throttling is `def.speed` class gates over a
  `ticks % i` table (`game_ai.cpp` `speeds[]`), not `dt`.
- **Bullets are map-stamped `CActor`s.** `CGame::spawnBullet`
  (`src/game_ai.cpp`) stamps the bullet tile into `m_map` and restores
  `m_pu` on death; `CGame::handleBullet` advances it one cell: a
  homing bullet runs `followPath` (algo from `bulletSpec_t::algo` or
  `TileDef::path`), a straight bullet steps along its `aim`. Detonation
  covers player contact (`isPlayerThere` → `addHealth`), icecube,
  barrel, vamplant. There is no velocity vector and no player velocity
  tracking anywhere in `src/`.
- **Rendering is the tile-draw pass** (`drawTile(x * TILE_SIZE, ...)`).
  Nothing renders between tile centers.
- **Straight-shot rays already exist as IPath algos**: `Path::LOS`,
  `Path::RAYCAST` (Bresenham), `Path::RAYCAST_BULLET` (DDA, exact cell
  sequence, detonates at first wall). Homing bullets already exist:
  `skill BULLET ... algo=<selector>` + `ttl=` (`bulletSpec_t`,
  `handleBossBullet`, `startPath`).

## 1. Mapping: generic spec → engine

| Generic feature | Disposition here |
|---|---|
| Task 1 float coords / velocity / 360° flight | **Deferred, non-goal** (§5). Needs a non-CActor bullet entity, a sub-tile render pass, and its own serialization — a new subsystem, not a bullet change. |
| Direct aim (Tier 1) | **Exists** — straight bullet along `aim`; `handleBossBullet` picks the cardinal toward the player. No work. |
| Predictive lead-targeting | **Task 1**: spawn-time aim at a *predicted player cell* (integer lead in ticks). |
| Spread / fan volleys | **Task 2**: N staggered spawns on flanking cells with diverging cardinals. |
| Soft homing (turn-rate-limited orbs) | Homing **exists** via `algo=`. The *turn-rate limit / no hairpin* part becomes **Task 3**: new IPath algo `ORBIT`. |
| Sine-wave oscillation | **Non-goal** — a sub-tile phenomenon; a tile can hold one bullet. Tile-space square wave only if content asks (§5). |
| Tile ricochet | **Task 4**: 90° wall-bank turns in `handleBullet`, bounded bounce counter. |

## 2. Task 1 — Predictive lead at spawn (no new serialization)

Replace the dumb cardinal pick with an aim at the predicted cell.

- **Player velocity, derived not stored:** add `Pos m_prevPlayerPos`
  to `CGame` (`src/game.h`), refreshed once per tick in the monster
  update entry (`CGame::handleMonsters`). Runtime-only game state, not
  serialized, **no `ENGINE_VERSION` bump**. Velocity =
  `player.pos() - m_prevPlayerPos` (each component −1/0/+1 cell).
- **Predicted cell:** `lead = clamp(dist / max(bulletSpeedCells, 1), 0,
  LEAD_MAX)` ticks (integer, `LEAD_MAX` = 3); `target = player.pos() +
  vel * lead`, clamped to map bounds. No wall-clip guard is needed:
  the aim steps are resolved by the existing `canMove` probes at
  runtime, and rays detonate at the first wall — same contract as
  `Path::RAYCAST`.
- **Call sites:** the cardinal ladder in `handleBossBullet`
  (`src/game_ai.cpp:183-198`) and the mob-shot spawn in
  `handleMonsterV3` (`game_ai.cpp:560-567`) pick the best single
  `JoyAim` from `translate(actor.pos(), aim)` minimizing Manhattan
  distance to `target`, preferring the current aim on ties.
- Lead applies to the *aim choice only* — bullet motion stays
  single-axis one cell/tick.

## 3. Task 2 — Fan volleys (no new serialization)

In `handleBossBullet` only: when the skill declares a fan (reuse the
existing `aims` boss knob semantics, or add ini key `count=N` to the
SKILL_BULLET line parsed in `src/bossdata.cpp`), spawn N bullets:
center along the Task-1 aim; flankers from `translate(origin, aim, ±k)`
cells (clamped, skipping occupied/invalid cells — `spawnBullet`
already returns `nullptr` there) with the two perpendicular cardinals
fanned outward. Cap N at 5 (2 flanking offsets per side; 4-dir means
there are no diagonal fan angles — say so in the ini docs). Each
spawned bullet keeps its own `algo`/`ttl`. Watch the documented
reallocation hazard: `spawnBullet` may `emplace_back` and invalidate
`actor` refs (`game_ai.cpp:563` comment) — re-look-up between spawns.

## 4. Task 3 — `Path::ORBIT`: turn-rate-limited homing (new IPath algo)

The tile-space equivalent of a max-turn-rate steering orb: greedy steps
toward the goal that **never emit a step directly reversing the
previous step** — a hairpin is impossible, so a sidestepping player
forces the bullet to loop wide around them before re-approaching,
exactly the acceptance criterion "overshoot in a wide arc".

Follow [docs/briefs/ipath.md](../briefs/ipath.md) end to end:

- `src/ai_path.h` / `.cpp`: `class Orbit : public IPath` in
  `namespace PathData` + `getPathAlgo` branch. Algorithm: from the
  sprite cell, greedily step toward `playerPos` (larger remaining
  axis, fall back to the other — `LineOfSight` shape); a candidate
  step equal to the reverse of the last emitted step is skipped; if
  both remaining-axis steps are illegal or reversed, take either
  perpendicular step; if all four fail → `return {}`. Bound the node
  walk (reuse `FLEE_RADIUS`-style constant, e.g. `ORBIT_RADIUS = 32`)
  so a boxed-in orb terminates instead of looping — it is a greedy
  router, not a searcher: no `Node`/`OpenList`, so BUG-C9/BUG-H17
  hygiene is structurally absent (as in `Raycast`/`FlowField`).
- `src/bossdata.h`: append `ORBIT` to `BossData::Path` **at the end**
  (append-only enum, values serialize in `CActor::m_algo`);
  `src/names.cpp`: `_N(ORBIT)` in `kPaths[]`; mirror fixtures
  `docs/spec/bosses/bossdata_runtime.{h,cpp}`, `parse.py`; update
  `docs/spec/ipath-implementations.md` inventory in the same change.
  Appending at the end is wire-backward-compatible: **no
  `ENGINE_VERSION` bump**.
- Content: `skill BULLET tile=... algo=ORBIT ttl=...`; monster
  `path=ORBIT` gets the same motion for free (this is the enemy-movement
  plan's orb requirement — one algo, both consumers).
- Tests per brief §3: `t_ai_path` (open grid, gap detour, sealed wall
  → empty, OOB → empty, reversal-forbidden property check on every
  returned step pair, 12×12 fuzz vs BFS for reachability parity),
  `t_tables` `resolvePath("ORBIT")`, `t_actor` `valid[]`, `t_bossdata`
  upper bound.

## 5. Task 4 — Tile ricochet (bank-shot bounce)

`handleBullet` (`src/game_ai.cpp:889`) currently detonates on
`!isMoving`. Add, in the non-path branch only:

- `uint8_t m_bounces` on `CActor` (+ `getBounces/setBounces/decBounces`),
  set at spawn from a `bounces=N` SKILL_BULLET ini key / TileDef
  column (whichever loader already owns bullet tuning; pick one, not
  both).
- On blocked-ahead with `m_bounces > 0`: turn to the perpendicular
  cardinal — deterministic left-turn, right-turn on tie — if
  `canMove(turned)`; else fall through to detonation. Decrement,
  `setAim(turned)`, continue this tick without damage.
  (One-axis velocity + 4-dir grid means a physical mirror-reflection is
  a hairpin; the 90° bank is the tile-space billiard — note this in
  the ini docs.)
- **Serialized-state change:** a new `CActor` member alters the actor
  wire record. Per AGENTS.md: mirror `CActor::read/write`
  (`readCommon/writeCommon`), bump `ENGINE_VERSION` (`src/game.cpp`)
  **and** the mirrored `EXPECTED_VERSION` in `tests/t_game.cpp` in the
  same commit. If the recorder replays actor records, re-check that
  path too.
- Tests: `t_actor` wire round-trip with bounces set; a `game_ai`
  corridor case (bullet banks off a wall corner instead of dying) if
  the existing `game_ai` test harness can drive `handleBullet`.

## 6. Non-goals (stated so they don't sneak in)

- **Float/pixel projectiles, 360° flight, sub-tile rendering.** Needs a
  bullet entity outside the `CActor`/tile-stamp model, an entity render
  pass between tiles, AABB-vs-tile hit tests, and its own save format.
  Revisit only when content demands arcs/bezier paths visibly.
  Stepping stone if ever: the boss half-tile space
  (`BOSS_GRANULAR_FACTOR = 2`) — quarter-tile at best, still not 360°.
- **Sine wave.** Sub-tile phenomenon; a tile holds one bullet. If ever
  wanted, the tile-space shape is a zigzag step router — a post-`ORBIT`
  greedy variant, not steering math.
- Per-frame `dt` steering math, `player.vel` as stored state,
  `isTileBlocked`/`hasLOS` predicates: deliberately not added; use
  `canMove` probes and the ray algos.

## 7. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw.
2. `ctest --test-dir build` (min: `t_ai_path`, `t_tables`, `t_actor`,
   `t_bossdata`, `t_game`).
3. After green: `python itools/gen_compliance.py`.
4. Move this plan to `docs/plans/archive/`.

Order of attack: Task 1 → 2 → 3 → 4 (independent; each shippable
alone). Task 3 is the largest and is fully proceduralized by the
ipath brief; Task 4 is the only one touching the wire format.
