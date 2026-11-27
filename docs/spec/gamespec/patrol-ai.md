# AI Patrol (`AI_PATROL`)

Opt-in idle-patrol supplement to the legacy wall-hugging wander tail of
`CGame::handleMonsterV3` (`src/game_ai.cpp`). Implementation plan of
record: `docs/plans/archive/ai-patrol-plan.md`.

## Flag

`AI_PATROL = 0x10` (`src/sprtypes.h`, enum `AI_TYPE`) is an ai-bitfield
bit consumed **only** in the MONSTERV3/GHOST wander tail. The legacy
`TYPE_MONSTER` dispatch and the boss states never read it. A monster is
patrol-enabled iff `(getTileDef(m_tile).ai & AI_PATROL) != 0`; the bit
comes from the static `tileDefs[]` table (`src/tilesdata.cpp`).

## Contract

- **Flag off ⇒ byte-for-byte today's behaviour.** The supplement block
  is skipped entirely: no path-mode, ttl, aim or position mutation.
- **Flag on ⇒ patrol supplements, never replaces.** Each wander-tick a
  leg toward one of four cardinal posts is attempted *before* the
  wall-hug tail. Any tick the leg cannot step (`startPath` returns
  false — blocked LOS, out-of-bounds goal, already standing on the
  post), a path-mode failure leaves no observable state and the
  **unchanged** wall-hug tail runs.
- **Posts** are anchored on `CActor::m_origin` (the serialized spawn
  cell): `N=(ox,oy-R) S=(ox,oy+R) E=(ox+R,oy) W=(ox-R,oy)` with
  `R = PATROL_RADIUS = 6`, clamped into the map by `CGame::translate`.
- **Cadence**: post = `posts[(tick / PATROL_LEG_TICKS) % 4]` with
  `PATROL_LEG_TICKS = 10`; each post owns a 10-tick window; one step
  per actor-accepted path tick, then the path is cleared and the leg is
  retried next tick.
- **No new state**: the leg rides the existing `BossData` path fields
  (`m_algo`, `m_path`, `m_ttl`), refreshed by `actor.clearPath()` each
  tick, invisible while the player is out of `def.range`. No `CActor`
  members, no wire fields, hence **no `ENGINE_VERSION` bump**.
- **No RNG**: movement stays a pure function of tick + origin + map.
  Monster–monster occupancy is not modelled (as today).
- **Bounds safety**: post coordinates are always clamped in-bounds;
  `LineOfSight::findPath` goal-validates via `canWalk`; patrol never
  widens the monster's reachable set beyond what wandering allows.

## Cost bound

Per patrol tick: one `LineOfSight::findPath` (≤ ~2·PATROL_RADIUS line
trace + ≤10 `canWalk` steps) + at most one `findNextDir` on leg failure
— bounded by today's already-documented wander cost, O(1) per step.

## Demo row

`TILES_ZOMBIE` (`TYPE_MONSTERV3`, speed 6, range 7, `Path::ASTAR`, no
bullet) carries `AI_PATROL` as the first patrol-enabled content row:
bullet-free so an out-of-range patrol never turns into an out-of-range
attacker, and its ASTAR chase branch is unchanged when the player comes
within range.

## Verification

`tests/unit/t_game_ai.cpp` (§ numbering of the plan):

| Check | Assertion |
|---|---|
| 5.1 flagless golden | unflagged walker (no `AI_PATROL` MONSTERV3 tile) on a boxed corridor fixture replays a hard-coded 16-step pre-change walk; fails if it enters chase range |
| 5.2 supplement-not-replace | `AI_PATROL` monster on a fixture where every leg fails at step 1 traces the identical boxed golden (wall-hug fallback, leg failure mutates nothing) |
| 5.3 patrol engages | open field: 40 active ticks produce >3 distinct positions inside radius `PATROL_RADIUS+2`, and the sequence differs from the pure wall-hug (`TILES_GHOST`, `ai=0`) |
| 5.4 edge origin | origin (0,0) corner, 300 active ticks: no crash, in-bounds, stamped player tile never entered |
| 5.5 determinism | two fresh identical runs produce identical sequences; no RNG seeding anywhere in the suite |
