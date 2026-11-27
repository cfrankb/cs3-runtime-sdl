# Implementation Plan: Dijkstra path algorithm (Path::DIJKSTRA)

Status: implemented (2026-11-26)
Date: 2026-11-26
Model: existing `AStar` in `src/ai_path.cpp` (lines 98–216)

## 1. Goal

Add a `Dijkstra` path algorithm as a fifth `IPath` implementation, selectable
by `path=DIJKSTRA` in bosses.ini and `algo=DIJKSTRA` in skill specs. It is
`AStar` with the heuristic dropped (`h = 0`), i.e. uniform-cost search over
the same 4-direction half-tile grid. Value: a reference/ground-truth searcher
for tests and future heuristics (e.g. the JPS plan), and correct optimal
search if terrain costs are ever introduced.

## 2. Constraints inherited from the existing model

- **4-direction only.** Expansion uses `PathData::g_steps` (AIM_UP/DOWN/
  LEFT/RIGHT only); output is `std::vector<JoyAim>` of unit steps; the
  path-to-direction converter keeps its `LOGE`-and-bail on bad transitions.
- **Per-edge walkability.** A step into a neighbor is legal iff
  `sprite.move(current); sprite.canMove(dir)` with the same save/restore of
  `sprite.pos()` as `AStar::findPath` (lines 185–191).
- **Half-tile space.** Bounds `map.width() * granularFactor` ×
  `map.height() * granularFactor`; identical start/goal validation and
  `LOGE` convention as AStar.
- Reuse `Node`, `CompareNode`, `std::hash<Pos>`, and the BUG-C9 pattern:
  relax `gCost`/`parent` **in place**, re-push the same pointer, skip stale
  copies on pop via `closedList` membership (`find()`, never `operator[]`
  in probe contexts). `CPath::followPath` caching needs no change.
- All edge costs are 1 today, so Dijkstra explores strictly more nodes than
  A* for the same optimal length. That is accepted — this algo is the
  reference, not the performance path.

## 3. Design

### 3.1 Class surface (`src/ai_path.h`)

Derived from the same base as `AStar`, same API:

```cpp
// Dijkstra (uniform-cost) Pathfinding class: A* with h = 0
class Dijkstra : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};
```

No `manhattanDistance` member — that is the entire delta from `AStar`.

### 3.2 Algorithm (`src/ai_path.cpp`)

`Dijkstra::findPath` is a copy of `AStar::findPath` with three edits:

1. Start node: `Node(startPos, 0, 0, nullptr)` (hCost = 0).
2. Successor creation: `Node(newPos, newGCost, 0, current)` — drop the
   `manhattanDistance` call.
3. `CompareNode` on `fCost() = gCost + hCost` is untouched; with `h ≡ 0`
   the existing min-heap ordering *is* g-cost ordering. No new comparator.

Everything else — bounds checks, `closedList` pop-skip, in-place
relaxation, goal-test-on-pop, path→`JoyAim` conversion — stays byte-identical
to AStar (including BUG-C9 comments, re-tagged `// BUG-C9:` applies).

`ponytail:` comment in code: identical to `AStar` with `h = 0`; if a second
heuristic-free searcher is ever needed, share one impl via an `hCost` lambda
rather than a third copy.

### 3.3 Optional later step (not in this change)

The AStar/AStarSmooth/Dijkstra triple duplicates the node loop and the
pos→JoyAim emission. The audit (`docs/audits/finalized/ai_path.cpp.audit.md`,
L51 note) already deferred extraction; keep deferring until a fourth copy
forces the issue.

## 4. Wiring (registry, 4 touch points)

| File | Change |
|---|---|
| `src/bossdata.h` | append `DIJKSTRA` to `BossData::Path` enum **after `ASTAR_SMOOTH`** (values serialize in `CActor::m_algo`'s uint8; appending is wire-backward-compatible, old saves never carry it) |
| `src/names.cpp` | append `_N(DIJKSTRA)` to `kPaths[]` |
| `src/ai_path.cpp` | `PathData::dijkstra` const instance + `Dijkstra::findPath` impl + `CPath::getPathAlgo` branch (`else if (algo == BossData::DIJKSTRA) return &PathData::dijkstra;`) |
| `docs/spec/bosses/bosses.ini` | add `DIJKSTRA` to the documented `path=`/`algo=` value list |

Mirrored copies to check on the same bump (per AGENTS.md mirror rule):
`docs/spec/bosses/bossdata_runtime.{h,cpp}` enum + name map and
`docs/spec/bosses/parse.py` enum — treat as spec fixtures, update to match.

No new source files → `bin/refresh.py` not required.
No `CActor`/`CMap`/`CLayer` data-structure change, no serialized field change
(`CPath::read/write` untouched) → **no `ENGINE_VERSION` bump** (enum-value-only
append, same ruling as the JPS plan).

## 5. Tests

### `tests/unit/t_ai_path.cpp` (extend `test_pathfinding`)

Reuse `TestSprite` / `checkPath` / `fillMap` / `wallColumn`; add
`const Dijkstra dijkstra;` next to `aStar`:

1. Open grid `(1,1) → (4,1)`: `expectedLen == 3`.
2. Gap-wall detour `(0,0) → (4,0)`: `expectedLen == 10` (same optimum as
   AStar/BFS).
3. Fully sealed wall: empty result.
4. Out-of-bounds start/goal: empty, no crash (LOGE only).
5. **Equivalence fuzz:** same seeded `std::mt19937` random 12×12 maps as the
   JPS plan proposes (or a smaller ~15-map set if that lands first):
   `dijkstra.findPath(...).size() == aStar.findPath(...).size()` and both
   empty-or-both non-empty, per map. This is the runnable check that the
   h=0 copy didn't drift.
6. Start == goal: handled upstream by `CPath::followPath`; no special case.
7. `CPath::getPathAlgo(BossData::DIJKSTRA)` returns non-null (mirror of the
   existing `getPathAlgo` check).

### `tests/unit/t_tables.cpp`

Extend the `resolvePath` block: `resolvePath("DIJKSTRA") == BossData::DIJKSTRA`
and `resolvePath("Path::DIJKSTRA") == BossData::DIJKSTRA`.

## 6. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build`; pedantic-clean on
   g++/clang/msvc/mingw (same headers only, no new portability surface).
2. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`, `t_actor`
   algo-name round-trip).
3. Regenerate `docs/compliance/src-checklist.md`
   (`python itools/gen_compliance.py`) only after tests pass — never bump
   hashes first.
4. Move this plan to `docs/plans/archive/` once implemented.

## 7. Risks / non-goals

- **Cost:** h=0 expands the full reachable ellipse-ish frontier instead of
  the A* corridor; on `MAX_PATH_SIZE = 4096`-scale searches it is a constant
  factor slower than A*, same asymptotics. Acceptable for a reference algo;
  nothing ships using it by default.
- Non-goals: weighted/terrain costs (no cost model exists), 8-direction
  movement (wire format can't express it), replacing AStar, CPath cache
  changes.

## 8. Suggested commit

`ai_path: add Dijkstra path algo (Path::DIJKSTRA), A* with h=0`
