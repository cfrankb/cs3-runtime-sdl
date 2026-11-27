# Implementation Plan: Theta* path algorithm (Path::THETASTAR)

Status: proposed
Date: 2026-11-26
Model: existing `AStar` in `src/ai_path.cpp` (`AStar::findPath`) plus the
staircase-clear oracle from `AStarSmooth::segmentSteps`; procedure per
[docs/briefs/ipath.md](../briefs/ipath.md).

## 1. Goal

Add a `ThetaStar` path algorithm as the next `IPath` implementation,
selectable by `path=THETASTAR` in bosses.ini and `algo=THETASTAR` in skill
specs. Theta* is A* whose relaxation also considers a node's **grandparent**:
when expanding `s` into successor `s'`, if a clear segment exists from
`parent(s)` to `s'`, then `parent(s') = parent(s)` and
`g(s') = g(parent(s)) + dist(parent(s), s')`. This merges collinear runs
**during** the search instead of in an after-pass.

Scope check (brief §0): a genuinely new in-loop relaxation, not a
constant tweak (Dijkstra) nor a post-processing pass (AStarSmooth).

## 2. Cost metric decision (read this first)

Textbook Theta* uses Euclidean g-costs on 8-grid for any-angle paths. That
does not apply here:

- Output is `std::vector<JoyAim>` of **unit cardinal steps** (4-dir wire
  format, brief §1). A visibility segment always expands to exactly
  `manhattanDistance` unit steps.
- Therefore `dist()` below is **Manhattan**, keeping `gCost` numerically
  equal to the number of emitted steps. The heuristic stays
  `manhattanDistance(pos, goal)` — admissible and consistent for these
  weights.
- Consequence (worth stating in the class comment): the step count of a
  ThetaStar path **equals A\*'s optimal** — never fewer (the emitted walk
  is traversable, and A* is optimal), never more (plain unit-step edges are
  always relaxed too, so `g(goal) ≤ g*(goal)` along the optimal chain).
  The gain over A* is straighter paths / fewer turns **chosen by the
  search itself**, replacing AStarSmooth's greedy anchor-greedy after-pass
  (which is O(n²) staircase probes over A*'s point list) with one merged
  pass. Euclidean any-angle costs are a non-goal: `JoyAim` cannot express
  them and a Euclidean `gCost` would decouple cost from emitted length and
  break the length-parity fuzz oracle.

## 3. Constraints inherited from the existing model (brief §1)

- **4-direction expansion** via `PathData::g_steps` only.
- **Half-tile space**; identical bounds validation and
  `LOGE("Invalid start (%d,%d) or goal ...")` + `return {}`.
- **Per-edge walkability** via `sprite.move(current); sprite.canMove(dir)`
  with the same save/restore of `sprite.pos()`.
- **Node-loop hygiene (BUG-C9 / BUG-H17):** `OpenEntry = std::pair<int,
  Node *>` fCost snapshots; relax `gCost`/`parent` in place on the
  heap-owned `Node`; skip stale snapshots and `closedList` membership on
  pop; probe maps with `find()`, never `operator[]`. Reuse `std::hash<Pos>`.
- **Output:** unit-step `JoyAim` list; impossible transition → `LOGE` +
  `return {}`. `CPath::followPath` caching needs no change.

## 4. Design

### 4.1 Shared oracle: lift `segmentSteps` (the only extraction)

`AStarSmooth::segmentSteps` is the codebase's blessed visibility predicate:
deterministic x-first staircase (full horizontal leg, then vertical),
every step `canMove`-probed, no axis fallback, sprite pos restored.
Theta*'s "line-of-sight" means exactly this oracle.

Lift the body into a file-scope anonymous-namespace helper

```cpp
// deterministic x-first staircase probe; fills `steps` and returns true
// only if every step's canMove probe succeeds (shared AStarSmooth/ThetaStar)
bool staircaseSteps(ISprite &sprite, const Pos &from, const Pos &to,
                    std::vector<JoyAim> &steps);
```

placed above both users (next to the existing `unitAim` block, which moves
with it). `AStarSmooth::segmentSteps` keeps its declared surface in
`src/ai_path.h` and becomes a one-line forwarder — zero header churn, no
behavior change, and Theta* gets rung 2 of the ladder (reuse) instead of a
second copy of a subtle probe. This is the *only* shared code extracted;
the node loop itself stays copied (see §4.4).

### 4.2 Class surface (`src/ai_path.h`)

```cpp
// Theta* Pathfinding class: A* whose relaxation prefers line-of-sight
// grandparents (Manhattan costs; unit-step 4-dir output, see cpp comment)
class ThetaStar : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
};
```

### 4.3 Algorithm (`src/ai_path.cpp`)

`ThetaStar::findPath` is a copy of `AStar::findPath` (bounds check,
snapshot/stale-skip pop, goal-test-on-pop, BUG-C9/H17 comments re-tagged)
with **two** edits:

1. **Successor relaxation** — after the unit-edge `canMove` probe passes:

   ```cpp
   int newGCost = current->gCost + 1;
   Node *newParent = current;
   Node *anchor = current->parent; // may be null (start expansion)
   if (anchor != nullptr)
   {
       const int viaAnchor = anchor->gCost + manhattanDistance(anchor->pos, newPos);
       std::vector<JoyAim> seg;
       // cheap bound first: only probe the oracle when the grandparent could
       // tie or beat the unit edge (tie goes to the straighter parent)
       if (viaAnchor <= newGCost && staircaseSteps(sprite, anchor->pos, newPos, seg))
       {
           newGCost = viaAnchor;
           newParent = anchor;
       }
   }
   ```

   Insert / relax-in-place logic is otherwise byte-identical to A*
   (`newGCost < it->second->gCost` sets `gCost` **and** `parent =
   newParent`, re-pushes the snapshot). Closed nodes are skipped exactly
   as A* does — no reopening; §2's unit-edge argument is why that stays
   optimal, and the fuzz in §6.1 is the empirical net.

2. **Reconstruction** — parent-chain points may be several cells apart on
   both axes. Walk the chain as in A*, then for each consecutive pair
   re-run `staircaseSteps` and append its steps (deterministic and
   map-static, so it must replay exactly what the relaxation validated).
   Adjacent pairs may shortcut straight through `unitAim`. A pair the
   oracle refuses after the search validated it is a sprite-model
   inconsistency: `LOGE` ("Invalid path transition ...", same convention)
   and `return {}` — no fail-soft single step here, since unlike
   AStarSmooth the points are not adjacent.

`ponytail:` comments in code:
- Manhattan-metric Theta*; upgrade path is Euclidean any-angle Theta*,
  blocked until `JoyAim` can express diagonals.
- Node loop is the fourth copy (AStar, AStarSmooth, Dijkstra, here); the
  audit-deferred extraction (brief §0) is now due — but Theta*'s successor
  block differs by design, so a shared loop would need a relaxation
  callback. Not worth it in this change; do it when a fifth searcher or a
  common change forces it.

### 4.4 Wiring (registry, brief §2)

| File | Change |
|---|---|
| `src/ai_path.h` | `ThetaStar` class (above) |
| `src/ai_path.cpp` | `const ThetaStar thetaStar;` in `namespace PathData` + `findPath` impl + `staircaseSteps` lift + `CPath::getPathAlgo` branch `else if (algo == BossData::THETASTAR) return &PathData::thetaStar;` |
| `src/bossdata.h` | append `THETASTAR` to `BossData::Path` **after `FLOWFIELD`** (enum value serializes in `CActor::m_algo`'s uint8 — append only, never renumber) |
| `src/names.cpp` | append `_N(THETASTAR)` to `kPaths[]` |

Mirror fixtures in the same change (AGENTS.md mirror rule):
`docs/spec/bosses/bossdata_runtime.h` (enum), `docs/spec/bosses/bossdata_runtime.cpp`
(`kPaths[]`), `docs/spec/bosses/parse.py` (`HEADER_STRUCT` enum).
Also add `THETASTAR` to the documented `path=`/`algo=` value list in
`docs/spec/bosses/bosses.ini`.

Enum-value-only append → wire-backward-compatible, **no `ENGINE_VERSION`
bump** (same ruling as Dijkstra/JPS plans). No new source files →
`bin/refresh.py` not required. `AStarSmooth::segmentSteps` is a forwarder
only, so its behavior (and the `docs/spec/ipath-implementations.md`
AStarSmooth semantics row) is unchanged.

### 4.5 Documentation (same commit, per the maintenance rule)

`docs/spec/ipath-implementations.md`: inventory row (`THETASTAR` /
`Path::THETASTAR` — "A* with line-of-sight-grandparent relaxation,
Manhattan costs" / "optimal length, straighter shape" / none yet), a
§3 section for it, and refresh the test-listing line (§5 of that file,
"six names" → seven).

## 5. Tests

### 5.1 `tests/unit/t_ai_path.cpp` (extend `test_pathfinding`)

Reuse `TestSprite` / `checkPath` / `fillMap` / `wallColumn`; add
`const ThetaStar thetaStar;`:

1. Open grid `(1,1) → (4,1)`: `expectedLen == 3` (unit output preserved).
2. Gap-wall detour `(0,0) → (4,0)`: `expectedLen == 10` (same optimum as
   AStar/BFS).
3. Fully sealed wall: empty result.
4. Out-of-bounds start/goal: empty, no crash (LOGE only).
5. **Fuzz parity:** join the existing seeded 12×12 loop —
   `thetaStar.findPath(...).size() == aStar.findPath(...).size()` and
   both-empty-or-both-non-empty per map. Per §2 this is an **exact**
   parity check, not a tolerance; it is the check that the in-loop
   relaxation did not drift.
6. **Straightness spot check (the new value):** on a clear LOS row/map
   (reuse the existing LOS-row fixture shape), reconstruct the parent
   chain implicitly via direction runs and assert ThetaStar's turn count
   ≤ AStarSmooth's on the same map — mirrors the existing AStarSmooth
   turn-parity case without pinning an exact aim sequence.
7. Start == goal: handled upstream by `CPath::followPath`; no special
   case.

### 5.2 `tests/unit/t_tables.cpp`

`resolvePath("THETASTAR") == BossData::THETASTAR` and
`resolvePath("Path::THETASTAR") == BossData::THETASTAR`, mirroring the
existing block.

### 5.3 `tests/unit/t_actor.cpp` (`test_path_algos`)

Append to the `valid[]` table so `CPath::getPathAlgo(BossData::THETASTAR)`
non-null is covered; invalid selectors still return null.

### 5.4 `tests/unit/t_bossdata.cpp`

The loaded-content `path <=` validity bound tracks the new highest enum
value (`FLOWFIELD` → `THETASTAR`).

## 6. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw (same headers only, no new portability surface).
2. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`,
   `t_actor`, `t_bossdata`).
3. Only after tests pass: `python itools/gen_compliance.py` to regenerate
   `docs/compliance/src-checklist.md` (never bump hashes first).
4. Move this plan to `docs/plans/archive/` once implemented.

## 7. Risks / non-goals

- **Oracle cost:** one staircase probe (O(segment length) canMove calls)
  per successor, gated by the `viaAnchor <= newGCost` bound so most
  dense-maze successors skip it. Worst case approaches AStarSmooth's
  probing per call, best case far less (no O(n²) anchor scan, search
  stops at the goal). Acceptable; same asymptotics as the family.
- **Optimality claim (§2):** the exact-parity fuzz is the empirical net
  for the shipped maps. If it ever disagrees, suspect the tie-breaking
  parent choice before the metric; the closed-list no-reopen ruling in
  §4.3 relies on the unit-edge argument, keep that invariant if touching
  the successor block.
- Non-goals (brief §5): Euclidean/any-angle costs, 8-direction output,
  weighted/terrain costs, replacing `AStar` or `AStarSmooth` (both stay
  as test references), `CPath` cache changes.

## 8. Suggested commit

`ai_path: add ThetaStar path algo (Path::THETASTAR), LOS-grandparent A* relaxation`
