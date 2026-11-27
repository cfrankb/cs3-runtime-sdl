# Implementation Plan: IPath candidate adoption (ASTAR_BUDGET, GBFS, FLEE, ASTAR_SAFE)

Status: phases 1-3 (ASTAR_BUDGET, GBFS, FLEE) implemented 2026-11-27; phase 4 proposed
Date: 2026-11-27
Source: [docs/spec/ipath-algorithm-candidates.md](../spec/ipath-algorithm-candidates.md)
(verdicts and oracle rationale live there; this plan is the executable form)
Procedure: [docs/briefs/ipath.md](../briefs/ipath.md) applies in full to each
phase. Model for phases 1/2/4: existing `AStar::findPath` in
`src/ai_path.cpp`; phase 3 additionally touches `IPath` / `CPath::followPath`.

## 0. Scope and sequencing

Four independent changes, in the candidate doc's adoption order. Each is a
separate commit with its own enum append, `kPaths[]` entry, 3 mirror
fixtures, `getPathAlgo` branch, tests, and
`docs/spec/ipath-implementations.md` inventory + §3 update in the same
commit (maintenance rule). No `ENGINE_VERSION` bumps: every change is an
append-only `BossData::Path` tail value (wire-backward-compatible, see
brief §2). Candidate #4 (`RAYCAST_BULLET`) is already shipped — no phase.

Current enum tail is `RAYCAST_BULLET` (`src/bossdata.h`). Tail appends:

| Phase | New enum tail value | Class | Loop base |
|---|---|---|---|
| 1 | `ASTAR_BUDGET` | `AStarBudget` | `AStar` + counter |
| 2 | `GBFS` | `Greedy` | phase 1 loop, greedy key |
| 3 | `FLEE` | `Flee` | bounded BFS (see §3.2) + `IPath::flees()` |
| 4 | `ASTAR_SAFE` | `AStarSafe` | phase 1 loop, `stepCost` |

Scope check (brief §0): phases 1/2/4 are constant/key tweaks of the closest
existing algo — copy, per the brief's deliberate extraction deferral (the
node loop copy count grows; the `ai_path.cpp` audit defers extraction until
the maintainer rules on it — do not extract unprompted). Phase 3's search
is new-but-bounded and needs no heap at all.

Common touch points per phase (brief §2, identical shape each time):

- `src/ai_path.h` — class declaration
- `src/ai_path.cpp` — `const <Class> <instance>;` in `namespace PathData`,
  `findPath` impl, `CPath::getPathAlgo` branch
- `src/bossdata.h` — append enum value **after `RAYCAST_BULLET`** (phase-ordered)
- `src/names.cpp` — append `_N(<NAME>)` to `kPaths[]`
- Mirrors in the same commit: `docs/spec/bosses/bossdata_runtime.h` (enum),
  `docs/spec/bosses/bossdata_runtime.cpp` (`kPaths[]`),
  `docs/spec/bosses/parse.py` (`HEADER_STRUCT` enum)
- `docs/spec/bosses/bosses.ini` documented `path=`/`algo=` value list
- Tests: `t_ai_path` (cases below), `t_tables` (`resolvePath` both forms),
  `t_actor::test_path_algos` (`valid[]` row), `t_bossdata` (`path <=` bound
  tracks the new highest enum value each phase)
- `docs/spec/ipath-implementations.md`: inventory row + §3 section + §5
  test-listing name count, same commit

No new source files in any phase → `bin/refresh.py` not required.

## 1. Phase 1 — Capped A* (`ASTAR_BUDGET`, class `AStarBudget`)

### 1.1 Design

Copy `AStar::findPath` verbatim (bounds check, snapshot/stale pop-skip,
goal-test-on-pop, BUG-C9/H17 comments re-tagged) plus two variables:

- `int expansions = 0;` incremented **only on non-stale pops** (after the
  stale/closed `continue`s), compared against `int m_budget;`
- `const Node *best = nullptr;` updated on every non-stale pop: smaller
  `hCost` wins; tie → lower `gCost`; tie → lexicographically smaller `pos`
  (declared tie-break, deterministic across stdlib heap flavors).

On budget exhaustion (before the failing pop would continue the loop): do
**not** return empty. If `best == &start node` → `return {}` (boxed in).
Otherwise reconstruct through the parent chain exactly as the goal case
does, ending at `best`. Budget-exhaustion partial success is *not* a
contract failure: the result is still a legal `std::vector<JoyAim>` of
probed unit steps to an in-bounds cell; `CPath` re-plans on exhaustion and
at every `PATH_TIMEOUT_MAX` step and converges at the wall nearest the goal
(candidate doc §2).

Budget as ctor parameter: `explicit AStarBudget(int budget = 256);` — the
shared `PathData` instance uses the default; tests inject small budgets.
`kPaths[]`/`getPathAlgo` wiring unchanged by the ctor (`IPath::findPath`
stays `const`).

### 1.2 Tests (`t_ai_path.cpp`)

1. Length parity + endpoint fuzz: join the seeded 12×12 loop asserting
   `aStarBudget` matches `aStar` length and replays to the goal whenever
   A* found a path — 12×12 ≤ 144 cells < 256 budget, so exhaustion never
   triggers. (Aim-list identity is not assertable: A* heap ties break on
   `Node*` pointer, so even two A* runs differ on tie-heavy maps.)
2. Corridor fallback (aim-pinned; corridor has no pop-order ties so pins
   are deterministic — note A* itself breaks fCost heap ties on `Node*`
   pointer, so aim pins are only sound on tie-free maps): 10×10, row y=1
   fully walled, goal (9,1) sealed in the walled row. Default budget: A*
   empty, capped non-empty, exactly 9 RIGHT ending at (9,0), the
   reachable cell minimizing Manhattan-to-goal. Injected budget 3:
   exactly 3 RIGHT (budget-exhaustion cut, path length ≤ budget).
3. Start walled in on all reachable sides: empty (best == start
   reconstructs to the empty list).
4. Standard: open-grid optimal length, gap-wall detour, out-of-bounds
   start/goal → empty + `LOGE`, no crash.

## 2. Phase 2 — Greedy best-first (`GBFS`, class `Greedy`)

### 2.1 Design

Phase-1 loop with the open-list key replaced (candidate doc §1):

- Entry key is a declared `std::pair<int, int>(hCost, gCost)` on a local
  min-heap (mirror `OpenList`'s declaration with the pair key + matching
  comparator) — `hCost` alone ties and lets heap insertion order leak
  compiler flavor into behavior. Do **not** reuse `Node::fCost()` (`g + h`,
  shared with the other searchers); compute `h` at push time.
- Stale-skip on pop compares the snapshot's `first` against
  `current->hCost` (position-fixed, so it never goes stale; duplicate
  pushes from `gCost` relaxations carry identical keys and die at the
  existing `closedList` pop-skip).
- Budget: same counter, same default 256, same ctor-parameter shape; on
  exhaustion, the same best-node partial reconstruction as phase 1 (the
  budget, not the greedy key, is what fails fast — behavior is one rule
  across phases 1/2).

### 2.2 Tests

Non-optimal by construction → **exempt from the A*/BFS length-parity
fuzz**; do not join that loop with a length assertion. Instead:

1. Per-map checks in the seeded 12×12 loop (reachable-goal maps where BFS
   is non-empty): GBFS non-empty, length ≤ budget, every emitted step
   `canMove`-legal via `checkPath`.
2. Open grid: length ≤ A* length + slack is *not* pinned; assert only
   legality + both-non-empty vs BFS.
3. Sealed-goal case (injected small budget): returns non-empty within
   budget where a plain `AStar` run on the same map would exceed it —
   pin the budget bound by asserting expansions-bounded length, i.e.
   `path.size() <= budget`.
4. Standard: sealed start → empty, out-of-bounds → empty.

## 3. Phase 3 — Flee (`FLEE`, class `Flee`) + driver concession

### 3.1 `IPath::flees()` (`src/ai_path.h`)

```cpp
// True for searchers whose goal is "farthest from playerPos", not
// "standing on it". Only Flee overrides; all shipped algos inherit
// false and are bit-identical.
virtual bool flees() const { return false; }
```

In `CPath::followPath` (`src/ai_path.cpp`), guard the on-goal
short-circuit:

```cpp
if (!astar.flees() && sprite.pos() == playerPos)
    return Result::NoValidPath;
```

That is the entire driver change. `isPlayerThere` final-step refusal and
the cached-aims machinery are untouched: a flee path moves away and can
never step into the threat's cell.

### 3.2 Search

`findPath` ignores the approach framing entirely: bounded uniform-cost
expansion of the threat cell `playerPos`'s surroundings, answer = farthest
reachable cell. **BFS shape, no `Node`/`OpenList`** (FIFO first-touch on
unit edges is optimal; BUG-C9/H17 hygiene does not apply — same ruling as
`FlowField`). Candidate doc §3's max-`hCost` open-list key is deliberately
dropped: with no goal-test early exit the visited set is the full
`g ≤ RADIUS` reachable set regardless of pop order, so the answer is
identical with less machinery.

- `constexpr int FLEE_RADIUS = 10;` (declared; ≤ `PATH_TIMEOUT_MAX` so one
  flee plan consumes like any other).
- BFS from `sprite.pos()`; expand a cell only while `g < FLEE_RADIUS`;
  per-edge `sprite.move(cur); sprite.canMove(dir)` + pos restore (§2
  contract, copy from `BFS::findPath`); visited-on-push via `std::hash<Pos>`.
- Track the answer live: max `manhattanDistance(pos, playerPos)`; tie →
  lower `g`; tie → lexicographic `pos`.
- Reconstruct via the parent map (copy `BFS::findPath` reconstruction),
  reversed to start→answer order. Start itself is a candidate; a fully
  boxed-in sprite returns `{}` (contract: failure = empty — the
  `!flees()` guard makes "adjacent to threat" *not* a failure case: the
  search runs and finds the best escape cell).
- Bounds validation identical (§2 contract): invalid start/goal → `LOGE` +
  `{}`. Note the "goal" here is the threat position — validate it the same
  way even though the search moves away from it (keeps garbage inputs from
  poisoning the Manhattan score).

### 3.3 Tests

1. **Adjacency pin (locks the concession):** sprite on the threat cell in
   an open map, `CPath::followPath(sprite, threatPos, fleeAlgo)` returns a
   non-empty `MoveSuccesful` (not `NoValidPath`) moving away.
2. Endpoint oracle: on gap-wall and open maps, endpoint = the
   BFS-reachable max-Manhattan cell within `FLEE_RADIUS` (compute via
   `bfs` in the test); every step legal.
3. Dead-end case: corridor pocket — endpoint is genuinely the farthest
   reachable cell (no oscillation; step count ≤ radius).
4. Regression: the nine shipped algos still short-circuit on-goal to
   `NoValidPath` (existing case) — `flees() == false` inherited.
5. Wiring: `getPathAlgo(BossData::FLEE)` non-null, `resolvePath` both
   forms, `t_bossdata` bound.

### 3.4 Group-engage policy knobs — deferred to first selecting content

The caller-side policy (ally-count scan over `CGame::m_monsters`,
per-tick selector flip in the mob movement driver, leash/alone-too-long
return-to-attack rule; candidate doc §3) needs new `bosses.ini` fields and
a content decision on *which* monsters engage-in-groups. Nothing selects
`FLEE` until then. Deferred, named, not speculative scaffolding: when the
first monster content asks for it, land the knobs + `m_algo` runtime-flip
test (`CActor::followPath` re-reads `m_algo` every tick; a flipped-to
appended enum value is load-safe under the append-only rule) in that
change.

## 4. Phase 4 — Probe-derived open-space A* (`ASTAR_SAFE`, class `AStarSafe`)

### 4.1 Design

Phase-1 loop (keeps the 256 budget knob and best-node fallback — worst
case stays bounded) plus a cell entry cost:

- File-local helper (anonymous namespace, next to the impl):
  `int openPenalty(ISprite &sprite, const Pos &p, std::unordered_map<Pos, int, std::hash<Pos>> &cache)`
  = number of the 4 `g_steps` edges out of `p` whose `canMove` probe is
  refused (0..3), computed once per cell per call (cache; teleport +
  restore like every other probe site).
- Edge relaxation: `newGCost = current->gCost + 1 + k * OPEN_SPACE_WEIGHT`
  where the successor is the cell whose penalty was just scored, and
  `constexpr int OPEN_SPACE_WEIGHT = 1;` (declared constant; `k` scaled by
  it).
- Admissibility: min edge cost is still 1 → `Manhattan × 1` heuristic
  stays admissible + consistent; BUG-H17 snapshot machinery unchanged
  (keys remain `fCost() = g + h` snapshots). Goal-test-on-pop stays
  correct under consistent costs.
- Probe budget is structural: ≤ 4 probes per *distinct* expanded cell
  (cache), ≤ `budget` distinct cells.

Deferred, per candidate doc §5: true terrain hazards (lava/fire/trap soft
costs) stay vetoed on the missing hazard-tile data source; do not add a
`TileDef` annotation or a cost-query probe contract here. Re-evaluate when
that data lands — the search change is then one `+ cost(...)` term.

### 4.2 Tests

1. **Free oracle:** on the wall-free seeded fuzz maps every penalty is 0
   → join the exact-parity loop against `AStar` (element-for-element),
   same map sizes as phase 1 (budget never hits).
2. Wall-hug choice: two equal-step routes from S to G, one adjacent to a
   wall column, one clear → the clear route is chosen (pin exact aim
   sequence; route costs differ by the penalty term only).
3. Corridor forced: single corridor where every route pays → path still
   found with optimal-under-penalty length vs the pinned expected aim list
   (penalty biases, never refuses).
4. Budget interaction: injected small budget + open space → best-node
   partial path as phase 1 (fallback still keyed on `hCost`, not `g`).
5. Wiring rows as every phase.

## 5. Per-phase commit shape and verification loop (AGENTS.md)

Each phase independently:

1. Code + mirrors + `bosses.ini` doc list + tests + `ipath-implementations.md`
   update in one commit.
2. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw (same headers only; phase 3's virtual is
   default-inline, no portability surface).
3. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`,
   `t_actor`, `t_bossdata`); phase 3 must additionally show the shipped
   algos' existing cases green (they inherit `flees() == false`).
4. Only after tests pass: `python itools/gen_compliance.py` to regenerate
   `docs/compliance/src-checklist.md`. Never bump hashes before testing.
5. Flip the corresponding candidate-doc verdict row status only via the
   `ipath-implementations.md` update (candidate doc stays the frozen
   evaluation record; move this plan to `docs/plans/archive/` when all
   four phases land).

Suggested commit names (one per phase):

1. `ai_path: add AStarBudget capped A* (Path::ASTAR_BUDGET), bounded partial paths`
2. `ai_path: add Greedy best-first (Path::GBFS), hCost-keyed capped search`
3. `ai_path: add Flee (Path::FLEE) + IPath::flees() driver concession`
4. `ai_path: add AStarSafe (Path::ASTAR_SAFE), probe-derived open-space penalty`

## 6. Risks / non-goals

- Node-loop copy count reaches eight; extraction stays deferred per
  `docs/audits/finalized/ai_path.cpp.audit.md` — do not extract in these
  commits.
- Phase 1 changes boxed-in mob *cost*, not today's idle behavior (re-plan
  prefix convergence, candidate doc §2); content keeps selecting `ASTAR`
  — per-monster selector switches are a content pass, not here.
- Non-goals (brief §5 + candidate doc): 8-direction output, Euclidean/
  any-angle costs, terrain-hazard costs and the `TileDef`/probe-contract
  widening they need, `CPath` cache changes, replacing any shipped algo,
  group-engage content fields (phase 3.4 deferral).
