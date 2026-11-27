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
[FlowField plan](../plans/archive/flow-fields-plan.md),
[RaycastBullet plan](../plans/archive/raycast-bullet-plan.md).

## 1. Inventory (2026-11-27)

| Class | Enum (`BossData::Path`) | ini selector | Search strategy | Path quality | In-use content |
|---|---|---|---|---|---|
| `AStar` | `ASTAR` | `Path::ASTAR` / `ASTAR` | best-first, `f = g + Manhattan` | optimal (consistent heuristic) | `bbdragondn`, `ghost`, `zombie` (`src/tilesdata.cpp`); `bosses.ini` demon |
| `AStarSmooth` | `ASTAR_SMOOTH` | `Path::ASTAR_SMOOTH` | A* then anchor-greedy smoothing with a staircase-clear oracle | optimal length, straighter shape | none yet (available) |
| `Dijkstra` | `DIJKSTRA` | `Path::DIJKSTRA` | uniform-cost (`h = 0`), same node loop as `AStar` | optimal | none (reference algo) |
| `BFS` | `BFS` | `Path::BFS` | FIFO breadth-first | optimal (fewest steps) | available for content |
| `LineOfSight` | `LOS` | `Path::LOS` | greedy: step toward goal, prefer larger remaining axis, fall back to the other axis | **not optimal, often no path**; never backtracks | tracking bullets (LOS bolt pattern) |
| `FlowField` | `FLOWFIELD` | `Path::FLOWFIELD` | reverse BFS from the goal over the whole reachable region, then gradient descent | optimal (fewest steps) | none (available; payoff is many-agents-one-goal) |
| `ThetaStar` | `THETASTAR` | `Path::THETASTAR` | A* with line-of-sight-grandparent relaxation, Manhattan costs | optimal length, straighter shape | none yet |
| `Raycast` | `RAYCAST` | `Path::RAYCAST` | Bresenham ray toward the goal; no walkability probe, no fallback | straight sight ray; ends at the first wall at runtime | bullet attacks (`skill BULLET ... algo=RAYCAST`) |
| `RaycastBullet` | `RAYCAST_BULLET` | `Path::RAYCAST_BULLET` | Amanatides & Woo DDA traversal of the segment; no walkability probe, no fallback | exact supercover sight ray (cells the line actually enters); ends at the first wall at runtime | none yet (available for bullets) |
| `AStarBudget` | `ASTAR_BUDGET` | `Path::ASTAR_BUDGET` | A* node loop with a capped expansion budget (256); on exhaustion returns the path to the popped node closest to the goal | optimal when the budget suffices; bounded-cost partial path otherwise | none yet (de-risk option for long-range chasers) |
| `Greedy` | `GBFS` | `Path::GBFS` | AStarBudget's loop keyed on (hCost, gCost) only — dives at the goal, no distance cost paid | NON-optimal (can exceed A*'s length); bounded per call; partial path on exhaustion | none yet (crowd/cheap-chaser slots) |
| `Flee` | `FLEE` | `Path::FLEE` | bounded BFS (radius 10) around the start; answer = reachable cell maximizing Manhattan distance from the "goal" (treated as a threat) | NON-approach by design; farthest-reachable cell within radius, deterministic tie-break | none yet (retreat/reposition behavior) |

All selectors also parse bare (`ASTAR`) or prefixed (`Path::ASTAR`) via
`kPaths[]` / `resolvePath()`.

### Recommended per use case

| Use case | Pick | Also consider | Avoid | Why |
|---|---|---|---|---|
| Mob monster (independent chaser, `CActor`, gf=1) | `ASTAR` | `LOS` (open-arena chasers that may give up at walls), `ASTAR_BUDGET` (bounded per-recompute cost on large/gf=2 maps), `GBFS` (crowds where a slightly longer walk is invisible), `FLEE` (retreat/reposition; group-flee policy knobs deferred until content uses it) | `BFS`, `DIJKSTRA` | Shipped default (`zombie`, `ghost`, `bbdragondn` in `tilesdata.cpp`): optimal unit steps, stops at the goal, cheapest per-recompute on the small gf=1 grid. `BFS`/`DIJKSTRA` expand strictly more for the same length — they are test references, not content choices. `FLOWFIELD` is the structural fit for many-mobs-one-player but currently rebuilds the whole field per call (see §3), so it wins nothing until a shared-field builder lands. |
| Boss (`CBoss`, gf=2, one on screen, movement is the spectacle) | `ASTAR` | `AStarSmooth` or `THETASTAR` for straighter walks, `ASTAR_BUDGET` (the gf=2 grid is where the unbounded sealed-goal cost hurts) | `BFS`, `DIJKSTRA` | Same optimal length everywhere; bosses move on the 4x larger gf=2 half-tile grid, so minimize expansions but maximize perceived quality: the two smoothers merge collinear runs (fewer turns) at zero length cost — nicer to watch for a big walking sprite. In use: demon (`bosses.ini`). |
| Bullet (`skill BULLET algo=`, flies until `Blocked`/TTL/goal) | straight shot: `RAYCAST_BULLET` (new content) / `RAYCAST` (existing content) | routing bolt: `LOS` | search algos (`ASTAR`…) | Bullets are fire-and-forget rays resolved by `followPath`'s per-step probe, not searchers: no routing, no re-plans. New straight-shot content should use `RAYCAST_BULLET` (true sight-line collision cells, §3); keep `RAYCAST` in existing content so its hit cells don't shift. `LOS` stays for bolts that step around cover (greedy, never backtracks). |

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

### ThetaStar — `ThetaStar::findPath`
The same A* node loop, but successor relaxation also considers the node's
**grandparent** (anchor): when expanding `s` into successor `s'`, if the
shared x-first staircase oracle (lifted `segmentSteps` body) is clear from
`parent(s)` to `s'`, then `parent(s') = parent(s)` and `g(s') =
g(parent(s')) + Manhattan`. Collinear runs merge *during* the search
instead of in AStarSmooth's after-pass; the oracle is gated by a
`viaAnchor <= g(s) + 1` bound. Costs are Manhattan, so `gCost` stays equal
to the emitted step count and length is exactly A*-optimal (verified by
the exact-parity fuzz); the gain is turn reduction. Reconstruction
re-runs the oracle per parent-chain segment (deterministic, map-static —
must replay what the relaxation probed); a refused segment is a
sprite-model inconsistency → `LOGE` + empty.

### Greedy — `Greedy::findPath`
The `AStarBudget` loop with the open-list key reduced to `(hCost,
gCost)` snapshots (pair key so equal-h ties fall to gCost, not heap
`Node*` order). Sorting on hCost alone dives straight at the goal and
routes wall detours by feel, so paths may exceed A*'s optimal length —
**declared non-optimal**; the seeded fuzz therefore asserts legality +
endpoint (never length parity), plus a budget-3 clone never exceeds its
cap. Same expansion budget (ctor parameter, default 256), same
best-node partial path and boxed-in-start rule as `AStarBudget` (the
`isCloser` / `chainToDirections` helpers are shared file-locals). gCost
is not finalized at pop under a greedy key; the parent chain stays
geometrically valid, which is all the reconstruction reads. Cheapest
searcher per call; candidate for crowd mobs where A* is overkill. No
shipped content selects it.

### AStarBudget — `AStarBudget::findPath`
The `AStar` node loop with a hard expansion budget (ctor parameter,
default 256, counted on non-stale pops). Budget exhaustion is *not*
contract failure: the path to the popped node closest to the goal (min
`hCost`, tie lower `gCost`, tie lexicographic pos) is returned, so a mob
150 tiles out consumes the prefix, `CPath` re-plans from a better cell,
and re-plans converge at the wall nearest the goal — bounded ~budget-node
cost per re-plan instead of the unbounded sealed-goal cost of full A*.
When the budget suffices the output is optimal, length-identical to
`AStar` (aim sequences need not match: `AStar` breaks fCost heap ties on
`Node*` pointer, so equal-length variants are all it may emit; exact aims
are pinned by the deterministic corridor test). A boxed-in start reconstructs to an
empty list, so "failure = empty" is untouched. No shipped content selects
it yet; switching long-range chasers from `ASTAR` is a content pass.

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

### Raycast — `Raycast::findPath`
Not a search and deliberately **not** probe-checked (the one deviation
from the §2 walkability contract, by design): a generalized-Bresenham ray
from the sprite to the goal, emitted as the exact staircase of 4-dir unit
steps, full geometric length even through walls. The bullet's runtime
driving is what resolves the ray: `CPath::followPath` probes `canMove()`
per step, and its `Blocked` at the first wall cell is what detonates the
bullet there (`handleBullet` expiry branch). A probe-trimmed partial ray
would instead exhaust into `NoValidPath` and park a `NoTTL` bullet at the
wall forever. Use for fire-and-forget straight-shot bullets; `LOS` stays
the choice for bolts that route around cover.

**Staircase order = collision order (Bresenham vs. true-ray traversal).**
The Bresenham error term spreads the `dx` horizontal and `dy` vertical
steps *evenly*, which is line-drawing geometry, not the sequence of cells
the segment actually passes through: on slopes whose ratio is neither 1:1
nor axis-aligned, the staircase enters the minor axis early and visits
cells the true line never touches. Example, start `(0,0)` goal `(1,3)`:
`DOWN RIGHT DOWN DOWN` visits `(1,1)`, but the center-to-center segment is
still in column 0 at row 1. Since `followPath` probes per step, the aim
order *is* the bullet's collision sequence — a wall in a disagreement cell
detonates the bullet on a wall its sight line never crossed. `RaycastBullet`
(§3) implements the Amanatides & Woo DDA ray, which emits grid crossings in
line-parameter order and visits only cells the line enters; axis and exact
45° rays are identical between the two algorithms. `RAYCAST` keeps the
even-staircase behavior so existing content's hit cells do not shift.

### RaycastBullet — `RaycastBullet::findPath`
The bullet ray with true sight-line geometry: an Amanatides & Woo fast grid
traversal (integer-rational DDA — no floats) of the segment from sprite to
goal. Steps are emitted in line-parameter order, so the visited cell
sequence is exactly the segment's supercover; exact-corner ties take the x
step first (x-first convention shared with AStarSmooth's staircase oracle),
visiting both corner-adjacent cells so a bullet never leaks through a
diagonal wall seam. Same deliberate §2 deviation as `Raycast`: not
probe-checked, full geometric length through walls, runtime `followPath`
detonates at the first blocked cell. Exactly `dx + dy` steps, identical to
`Raycast` on axis-aligned and exact 45° rays; differs only in step order
(the collision-cell sequence) on other slopes — see the `(0,0)→(1,3)`
discriminator in `t_ai_path`. No `Node`/`OpenList`.

### Flee — `Flee::findPath`
The inverted goal: `playerPos` is a **threat**, and the answer is the
reachable cell *maximizing* Manhattan distance from it, searched by
bounded BFS (radius 10, `FLEE_RADIUS`) around the start. BFS shape, no
`Node`/`OpenList` (FIFO first-touch on unit edges is optimal; same ruling
as `FlowField`); the candidate doc's max-hCost open-list key was dropped
— without a goal-test early exit the visited set is the full reachable
set within the radius regardless of pop order. Tie-break declared: max
distance, then min g, then lexicographic pos. Start is a candidate: a
boxed-in (or already-farthest) sprite reconstructs to the empty list, so
"failure = empty" is untouched. `IPath::flees()` returns true — the one
override of that hook — which makes `CPath::followPath` skip its on-goal
short-circuit: standing ON the threat is exactly when a flee must
search (all approach algos inherit `false` and are bit-identical). The
radius cap keeps one flee plan consumable within `PATH_TIMEOUT_MAX`. No
shipped content selects it; the candidate doc's group-engage policy
(knockback-adjacent ini knobs) is deferred until first content uses it.

## 4. How a choice reaches the algo

`bosses.ini path=` / skill `algo=` → `resolvePath()` (`kPaths[]`) →
`BossData::Path` → `CActor::setAlgo` / `startPath` →
`CPath::getPathAlgo(algo)` returns the shared `const` instance from
`namespace PathData` (null + `LOGE` on unknown) →
`CPath::followPath(sprite, playerPos, algo)`.

`CPath` driver (algo-agnostic): caches the aim list, recomputes when
exhausted or every `PATH_TIMEOUT_MAX` (10) successful steps; a failed
step invalidates the cache (`Blocked`); standing on the goal short-circuits
to `NoValidPath` without searching (skipped for `flees()` searchers — for
them the "goal" is distance, not the player's cell); the actual final step
into the player's cell is always refused (`isPlayerThere` → `NoValidPath`)
even though searchers may path into it.

## 5. Test map (`tests/unit/t_ai_path.cpp` unless noted)

- `test_pathfinding`: open grid, LOS row, Raycast row/diagonal exact-staircase
  + un-probed wall ray + start==goal, RaycastBullet DDA-vs-Bresenham
  discriminator `(0,0)→(1,3)` + corner-diagonal + slope sign pins + Raycast
  length-parity fuzz + sealed-wall geometric ray + start==goal, gap-wall
  detour (AStar, BFS, AStarSmooth + turn-count parity), S-bend (21-step
  optimum), exact
  staircase-shape oracle case, BUG-C9 relaxation map, BUG-H17 A*/BFS
  length-parity fuzz (20 seeded maps; AStarBudget length parity + endpoint
  vs A* where a path exists), AStarBudget corridor fallback (sealed-row
  goal converges at closest reachable cell with exact aim pin; budget-3
  cut; boxed-in start → empty), Greedy sealed-wall empty + corridor pins
  (9 RIGHT fallback, budget-3 cut) + fuzz endpoint/budget-3-cap checks
  (no length parity — declared non-optimal), AStarSmooth fuzz (50 seeded maps,
  length parity + no blocked cell entered), ThetaStar exact length parity
   in the seeded fuzz + LOS-row turn check, sealed wall, out-of-bounds,
   start==goal ruling, Flee open-map farthest-corner endpoint pin +
   wall-sealed-pocket corridor + boxed-in empty + flees() flag + driver
   adjacency concession (sprite on threat → MoveSuccesful via a boss
   sprite).
- `t_tables.cpp`: selector name resolution incl. all twelve algo names.
- `t_actor.cpp` `test_path_algos`: `getPathAlgo` non-null per enum value.
- `t_bossdata.cpp`: loaded `path <= FLEE` validity bound.

## 6. Adding or changing an algorithm

Follow [docs/briefs/ipath.md](../briefs/ipath.md) (scope check, touch
points, mirror fixtures, tests, verification loop), then **update this
file's inventory table and sections in the same commit**.
