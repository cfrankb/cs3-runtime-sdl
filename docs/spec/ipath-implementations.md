# IPath pathfinding — implementation summary

Audience: users choosing a `path=`/`algo=` selector for content, and AI
agents modifying `src/ai_path.{h,cpp}`.

> **Maintenance rule — keep this file current.** Whenever an algorithm is
> added to, removed from, or materially changed in the `IPath` family
> (class in `src/ai_path.h`, `BossData::Path` enum, `PathData` instances,
> `CPath::getPathAlgo`, or an algorithm's semantics/cost guarantees),
> update this summary in the same change: the inventory table, the
> per-algorithm section, and the content-users row. The step-by-step
> procedure for adding an algorithm is
> [docs/briefs/ipath.md](../briefs/ipath.md); this file is the *state of
> the world*, that one is the *how-to*.

Sources of truth: `src/ai_path.h`, `src/ai_path.cpp`, `src/bossdata.h`
(`BossData::Path`), `src/names.cpp` (`kPaths[]`), `src/actor.cpp`
(`CActor::canMove`), `src/boss.cpp` (`CBoss::canMove`).
History: [BUG-C9](../bugs/resolved/critical-triage-2026-11-17.md) (UAF),
[BUG-H17](../bugs/resolved/ai_path-heap-invariant.md) (heap keys),
[ai_path.cpp audit](../audits/finalized/ai_path.cpp.audit.md),
[AStarSmooth fix plan](../plans/archive/fix-astar-smooth-plan.md),
[Dijkstra plan](../plans/archive/dijkstra-plan.md),
[FlowField plan](../plans/archive/flow-fields-plan.md).

## 1. Inventory (2026-11-26)

| Class | Enum (`BossData::Path`) | ini selector | Search strategy | Path quality | In-use content |
|---|---|---|---|---|---|
| `AStar` | `ASTAR` | `Path::ASTAR` / `ASTAR` | best-first, `f = g + Manhattan` | optimal (consistent heuristic) | `bbdragondn`, `ghost`, `zombie` (`src/tilesdata.cpp`); `bosses.ini` demon |
| `AStarSmooth` | `ASTAR_SMOOTH` | `Path::ASTAR_SMOOTH` | A* then anchor-greedy smoothing with a staircase-clear oracle | optimal length, straighter shape | none yet (available) |
| `Dijkstra` | `DIJKSTRA` | `Path::DIJKSTRA` | uniform-cost (`h = 0`), same node loop as `AStar` | optimal | none (reference algo) |
| `BFS` | `BFS` | `Path::BFS` | FIFO breadth-first | optimal (fewest steps) | available for content |
| `LineOfSight` | `LOS` | `Path::LOS` | greedy: step toward goal, prefer larger remaining axis, fall back to the other axis | **not optimal, often no path**; never backtracks | tracking bullets (LOS bolt pattern) |
| `FlowField` | `FLOWFIELD` | `Path::FLOWFIELD` | reverse BFS from the goal over the whole reachable region, then gradient descent | optimal (fewest steps) | none (available; payoff is many-agents-one-goal) |

All selectors also parse bare (`ASTAR`) or prefixed (`Path::ASTAR`) via
`kPaths[]` / `resolvePath()`.

## 2. Shared contract (every implementation)

- **4 directions only.** Expansion is `PathData::g_steps`
  (AIM_UP/DOWN/LEFT/RIGHT); `JoyAim` output is one aim per unit step,
  labelled by the actual cell displacement.
- **Half-tile space.** Bounds are `map.width() *
  sprite.getGranularFactor()` × `map.height() * granularFactor`
  (`CActor` gf=1, `CBoss` gf=2). Start/goal validated against these
  bounds: invalid → `LOGE` + empty return, never a crash.
- **Per-edge walkability.** A step into a neighbor is legal iff
  `sprite.move(current); sprite.canMove(dir)` succeeds (sprite teleported
  to the expanded cell, position restored after each probe;
  `move(Pos)` never touches the map). Walkability is sprite-dependent and
  possibly directed — `CActor::canMove` gates solid/water/attr/occupancy
  per type (ghosts phase solids; MONSTERV3/GHOST/bullets admit the
  player's stamped cell so the goal is reachable).
- **Failure = empty `std::vector<JoyAim>`.** No path, sealed goal,
  invalid transition: empty. Callers must treat empty as "no path".
- **Node-loop hygiene (`Node` + `OpenEntry = std::pair<int, Node *>` +
  `OpenList` min-heap on snapshot `fCost`):** relax `gCost`/`parent` in
  place on the arena-owned `Node` (BUG-C9), never mutate a key of an
  entry inside the heap (BUG-H17); stale entries are skipped on pop by
  snapshot mismatch and by `closedList` membership; probe maps with
  `find()`, never `operator[]`.

## 3. Per-algorithm notes

### AStar — `src/ai_path.cpp` `AStar::findPath`
The reference searcher and test ground truth. Manhattan heuristic
(admissible + consistent for unit 4-dir steps), goal test on pop,
lazy-deletion open list (BUG-C9 + BUG-H17 machinery described above).

### Dijkstra — `Dijkstra::findPath`
`AStar` with `hCost ≡ 0`. Same optimality, strictly more expansions;
ships as the heuristic-free reference for tests and future heuristics.
Nothing in content selects it.

### BFS — `BFS::findPath`
Plain FIFO frontier, visited-on-push, parent-map reconstruction.
Order-corruption-immune, so `t_ai_path` uses it as the ground-truth
path-length oracle for the fuzz checks. Expands the most nodes of the
three optimal searchers on open maps.

### AStarSmooth — `AStarSmooth::findPath` + `smoothPath` + `segmentSteps`
Runs the same A* node loop, then smooths: repeatedly extends the current
anchor as far as possible along the A* point list where a **deterministic
x-first staircase** (full horizontal leg, then vertical leg, every step
`canMove`-probed, no axis fallback) is clear — the old implementation's
greedy `LineOfSight` oracle "saw" through walls by detouring and is gone
(fix plan archived). Segments come straight out of the oracle walk
(single pass); a segment the search itself validated but the oracle
refuses is walked as a single unit step with a `LOGW` (fail-soft) instead
of bailing. Length always equals A* (4-dir unit output cannot go below
optimal); the gain is turn reduction.

### FlowField — `FlowField::findPath`
One reverse BFS seeded at the goal fills a flat per-cell field (1 byte:
downhill `g_steps` index, `-1` unreached); the answer is the descent from
the start. Uses the same forward `canMove` edge predicate as the other
searchers, so lengths match A*/BFS exactly. No `Node`/`OpenList` (BFS
first-touch is already optimal, so the BUG-C9/BUG-H17 hygiene does not
apply). Pays a full-region scan per call where BFS/A* stop at the goal;
the field is rebuilt each call (const shared instance — see the
`ponytail:` note in code for the future `FlowFieldBuilder` shape).

### LineOfSight — `LineOfSight::findPath`
Not a search: greedy walk toward the goal (prefers the axis with more
remaining distance, falls back to the other axis when blocked), returns
the aims or empty if boxed in. Cheap and straight; use for bullets or
open-arena chasers only — it gives up at walls instead of routing.

## 4. How a choice reaches the algo

`bosses.ini path=` / skill `algo=` → `resolvePath()` (`kPaths[]`) →
`BossData::Path` → `CActor::setAlgo` / `startPath` →
`CPath::getPathAlgo(algo)` returns the shared `const` instance from
`namespace PathData` (null + `LOGE` on unknown) →
`CPath::followPath(sprite, playerPos, algo)`.

`CPath` driver (algo-agnostic): caches the aim list, recomputes when
exhausted or every `PATH_TIMEOUT_MAX` (10) successful steps; a failed
step invalidates the cache (`Blocked`); standing on the goal short-circuits
to `NoValidPath` without searching; the actual final step into the
player's cell is always refused (`isPlayerThere` → `NoValidPath`) even
though searchers may path into it.

## 5. Test map (`tests/unit/t_ai_path.cpp` unless noted)

- `test_pathfinding`: open grid, LOS row, gap-wall detour (AStar, BFS,
  AStarSmooth + turn-count parity), S-bend (21-step optimum), exact
  staircase-shape oracle case, BUG-C9 relaxation map, BUG-H17 A*/BFS
  length-parity fuzz (20 seeded maps), AStarSmooth fuzz (50 seeded maps,
  length parity + no blocked cell entered), sealed wall, out-of-bounds,
  start==goal ruling.
- `t_tables.cpp`: selector name resolution incl. all six names.
- `t_actor.cpp` `test_path_algos`: `getPathAlgo` non-null per enum value.
- `t_bossdata.cpp`: loaded `path <= FLOWFIELD` validity bound.

## 6. Adding or changing an algorithm

Follow [docs/briefs/ipath.md](../briefs/ipath.md) (scope check, touch
points, mirror fixtures, tests, verification loop), then **update this
file's inventory table and sections in the same commit**.
