# Implementation Plan: Flow Field path algorithm (Path::FLOWFIELD)

Status: implemented (2026-11-26)
Date: 2026-11-26
Brief: `docs/briefs/ipath.md` (procedure); model: `docs/plans/archive/dijkstra-plan.md`

## 1. Goal

Add a `FlowField` path algorithm as the next `IPath` implementation,
selectable by `path=FLOWFIELD` in bosses.ini and `algo=FLOWFIELD` in skill
specs. Instead of a forward search start→goal, it runs one **reverse BFS
from the goal** over the same 4-direction half-tile grid, storing per cell
the unit `JoyAim` that steps downhill toward the goal; the answer is the
gradient descent from the start. It is the sixth searcher and the first one
whose natural structure is *goal-centric*: the same field serves every cell
at once, which is what a future multi-minion swarm feature would reuse.

## 2. Constraints inherited from the existing model

- **4-direction only.** Expansion and descent use `PathData::g_steps`
  exclusively; output is `std::vector<JoyAim>` of unit steps.
- **Half-tile space.** Bounds `map.width() * granularFactor` ×
  `map.height() * granularFactor`; identical start/goal validation and
  `LOGE("Invalid start (%d,%d) or goal ...")`-and-`return {}` convention.
- **Per-edge walkability.** The reverse BFS admits the forward edge
  `prev -> cur` iff, with the sprite moved to `prev`,
  `sprite.canMove(dir)` succeeds for the `g_steps` entry whose delta takes
  `prev` to `cur` — byte-for-byte the predicate `AStar::findPath` applies
  when expanding `prev` (save/restore of `sprite.pos()`). Because it is the
  *same* predicate, reverse reachability from the goal is exactly forward
  reachability to the goal, and unit-cost BFS distances are optimal. This
  is why the length-parity fuzz test (Section 5) is a valid drift check
  even though the traversal direction is flipped.
- **No `Node` / `OpenList` / BUG-C9 / BUG-H17 machinery.** Like `BFS` this
  algo needs no heap and no heap-owned nodes; the `OpenEntry` snapshot and
  probe-map (`find()`, never `operator[]`) rules do not apply — state lives
  in one flat grid vector (Section 3.2), so there is no key-defaulting trap.
- Reuse `std::hash<Pos>`? **Not needed** — no unordered_map here. Do not
  add another hash either way.
- `CPath::followPath` caching needs no change.

## 3. Design

### 3.1 Class surface (`src/ai_path.h`)

```cpp
// Flow Field Pathfinding class: reverse BFS from the goal, then gradient descent
class FlowField : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};
```

### 3.2 Algorithm (`src/ai_path.cpp`)

`FlowField::findPath`, after the shared bounds check:

1. **Field.** `std::vector<int8_t> flow(static_cast<size_t>(mapLen) * mapHei, -1);`
   indexed `y * mapLen + x`. Value = `g_steps` index of the downhill step,
   `-1` = unreached, `TOTAL_AIMS` (or `-2`) sentinel at the goal cell.
2. **Build.** `std::queue<Pos>` seeded with `goalPos`. Pop `cur`; for each
   `g_steps[i]`, the predecessor is `prev = cur - g_steps[i].delta`.
   Bounds-check `prev`, skip cells already reached, then probe
   `sprite.move(prev); canStep = sprite.canMove(g_steps[i].dir);
   sprite.move(originalPos);`. On success set `flow[prev] = i` and push
   `prev`. First-touch wins — BFS order means `dist(prev) = dist(cur) + 1`
   is minimal, so no relaxation pass exists (the whole BUG-C9/H17 class of
   bug is structurally absent).
3. **Descend.** From `startPos`: while `cell != goalPos`, read
   `i = flow[cell]`; if `i < 0` the start cannot reach the goal —
   `LOGE("FlowField: no path from (%d,%d) to (%d,%d)")` and `return {}`.
   Otherwise `directions.push_back(g_steps[i].dir)` and
   `cell += g_steps[i].delta`. Termination is guaranteed: every stored
   step strictly decreases the BFS distance, so no step cap is needed
   (same no-cap posture as the existing searches). `start == goal`
   descends zero steps (upstream `followPath` already short-circuits it).

Field build is O(reachable cells), same asymptotics as Dijkstra, and scans
the full reachable region where BFS/A* stop at the goal — that is the known
cost of this model, accepted in Section 7. Field memory is 1 byte/cell,
cheaper per cell than either searcher's per-node `unordered_map` allocation.

`ponytail:` comment in code: field is rebuilt per call; `findPath` is
`const` on a shared `PathData` singleton so there is no legitimate place to
cache. If a swarm feature ever needs one field for N agents with the same
goal, give that feature an explicit `FlowFieldBuilder` keyed on
(goal, footprint, map epoch) — do not bolt `mutable` state onto this
instance.

## 4. Wiring (registry, per brief §2)

| File | Change |
|---|---|
| `src/bossdata.h` | append `FLOWFIELD` to `BossData::Path` **at the end** (after `DIJKSTRA` — or after `JPS` if `docs/plans/jps-plan.md` landed first; never renumber) |
| `src/names.cpp` | append `_N(FLOWFIELD)` to `kPaths[]` |
| `src/ai_path.cpp` | `const FlowField flowField;` in `namespace PathData` + `FlowField::findPath` impl + `getPathAlgo` branch `else if (algo == BossData::FLOWFIELD) return &PathData::flowField;` |
| `docs/spec/bosses/bosses.ini` | add `FLOWFIELD` to the documented `path=`/`algo=` value list |

Mirror fixtures, same change (AGENTS.md mirror rule):
`docs/spec/bosses/bossdata_runtime.h` enum, `docs/spec/bosses/bossdata_runtime.cpp`
`kPaths[]` name map, `docs/spec/bosses/parse.py` `HEADER_STRUCT` enum.

Enum-value-only append → wire-backward-compatible, **no `ENGINE_VERSION`
bump** (same ruling as Dijkstra/JPS). No new source files → `bin/refresh.py`
not required.

## 5. Tests

### `tests/unit/t_ai_path.cpp` (extend `test_pathfinding`)

Reuse `TestSprite` / `checkPath` / `fillMap` / `wallColumn`; add
`const FlowField flowField;` next to `dijkstra`:

1. Open grid `(1,1) → (4,1)`: `expectedLen == 3`.
2. Gap-wall detour `(0,0) → (4,0)`: `expectedLen == 10` (same optimum).
3. Fully sealed wall: empty result.
4. Out-of-bounds start/goal: empty, no crash (LOGE only).
5. **Length-parity fuzz:** same seeded `std::mt19937` random 12×12 map loop
   — add `flowField` to the per-map comparison against A*/BFS: equal
   lengths and both-empty-or-both-non-empty. This doubles as the check that
   the reversed traversal kept the same edge predicate.
6. **Gradient validity (flow-field-only):** on one detour map, replay the
   emitted `JoyAim` sequence cell-by-cell from the start and assert it
   lands exactly on the goal in `len` steps — the descent invariant, as a
   runnable check.
7. `CPath::getPathAlgo(BossData::FLOWFIELD)` returns non-null.

### `tests/unit/t_tables.cpp`

`resolvePath("FLOWFIELD") == BossData::FLOWFIELD` and
`resolvePath("Path::FLOWFIELD") == BossData::FLOWFIELD`.

### `tests/unit/t_actor.cpp` (`test_path_algos`)

Add `FLOWFIELD` to the `valid[]` table; invalid selectors still null.

### `tests/unit/t_bossdata.cpp`

Loaded-content `path <=` validity bound tracks the new highest enum value.

## 6. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw/zig/clang and emscripten (no new headers beyond
   `<queue>`/`<vector>`, already included; no new portability surface).
2. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`, `t_actor`,
   `t_bossdata`).
3. Only after tests pass: `python itools/gen_compliance.py` to regenerate
   `docs/compliance/src-checklist.md`. Never bump hashes before testing.
4. Move this plan to `docs/plans/archive/`.

## 7. Risks / non-goals

- **Cost:** the field build explores the entire goal-reachable region on
  every call, so single-agent cost is strictly above BFS/A* (constant
  factor, same asymptotics). The payoff only materialises for many-agents-
  one-goal, which does not exist yet; nothing ships using it by default.
- **Non-goals (brief §5):** no cross-call field caching (const singleton,
  no invalidation hook — see the `ponytail:` note), no weighted/terrain
  costs, no 8-direction output, no change to `CPath` cache or any existing
  algo, no extraction of the duplicated emission code (still deferred until
  a *fourth* copy of the node-loop forces it; this algo does not add one).

## 8. Suggested commit

`ai_path: add FlowField path algo (Path::FLOWFIELD), reverse-BFS gradient descent`
