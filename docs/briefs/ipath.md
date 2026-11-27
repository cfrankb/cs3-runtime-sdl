# Brief: adding a new IPath pathfinding algorithm

Procedure for adding a path algorithm to the `IPath` system
(`src/ai_path.h` / `src/ai_path.cpp`), distilled from the Dijkstra
implementation (`docs/plans/archive/dijkstra-plan.md`, 2026-11-26).

## 0. Scope check first

The existing algorithms are `AStar`, `AStarSmooth`, `BFS`,
`LineOfSight`, `Dijkstra`. Most "new algo" requests are one of:

- an existing algo with a constant tweak (Dijkstra = A* with `h = 0`) —
  copy the closest one and change the minimum delta;
- a post-processing pass (`AStarSmooth` wraps A*'s node list);
- genuinely new search structure (JPS plan, `docs/plans/jps-plan.md`).

The node loop and the pos→`JoyAim` emission are duplicated per algo; the
extraction is deliberately deferred (`docs/audits/finalized/ai_path.cpp.audit.md`).
Keep deferring until a fourth copy forces the issue — until then, copy.

## 1. Model constraints (all algos share these)

- **4-direction only.** Expand via `PathData::g_steps`
  (AIM_UP/DOWN/LEFT/RIGHT with their `Pos` deltas); never hardcode a
  second delta table. The 8-direction wire format cannot express more.
- **Half-tile space.** Bounds are `map.width() * sprite.getGranularFactor()`
  × `map.height() * granularFactor`. Validate start and goal against
  these bounds; on failure `LOGE("Invalid start (%d,%d) or goal ...")`
  and `return {}`. Never crash on out-of-bounds.
- **Per-edge walkability.** A step into a neighbor is legal iff
  `sprite.move(current); sprite.canMove(dir)` with save/restore of
  `sprite.pos()` (see `AStar::findPath`). Do not assume node walkability.
- **Output.** `std::vector<JoyAim>` of unit steps; on an impossible
  path transition, `LOGE` and `return {}` (keep the converter contract).
- **Node loop hygiene (BUG-C9 / BUG-H17).** If you use the `Node` /
  `OpenList` machinery: open-list entries are `fCost` snapshots
  (`OpenEntry = std::pair<int, Node *>`); relax `gCost`/`parent` **in
  place** on the heap-owned `Node` (never reassign the `unique_ptr` — the
  open list holds raw pointers); skip stale entries on pop by comparing
  the snapshot and by `closedList` membership. Probe maps with `find()`,
  never `operator[]` (it default-inserts keys that corrupt the pop-skip
  logic).
- Reuse `std::hash<Pos>` (defined in `ai_path.cpp`); do not add another.
- `CPath::followPath` caching needs no change for a new algo.

## 2. Touch points

| File | Change |
|---|---|
| `src/ai_path.h` | `class NewAlgo : public IPath { std::vector<JoyAim> findPath(ISprite &, const Pos &) const override; };` |
| `src/ai_path.cpp` | `const NewAlgo newAlgo;` in `namespace PathData` + `findPath` impl + `getPathAlgo` branch `else if (algo == BossData::NEWALGO) return &PathData::newAlgo;` |
| `src/bossdata.h` | append the name to `BossData::Path` enum **at the end** — the enum value serializes in `CActor::m_algo`'s uint8, so never renumber or insert mid-enum |
| `src/names.cpp` | append `_N(NEWALGO)` to `kPaths[]` (enables `path=NEWALGO` and `algo=Path::NEWALGO` in bosses.ini) |

Mirror fixtures to update in the same change (AGENTS.md mirror rule):

- `docs/spec/bosses/bossdata_runtime.h` — `BossData::Path` enum
- `docs/spec/bosses/bossdata_runtime.cpp` — `kPaths[]` name map
- `docs/spec/bosses/parse.py` — `HEADER_STRUCT` enum

Wiring an enum value at the end is wire-backward-compatible (old saves
never carry it) and needs **no `ENGINE_VERSION` bump**. A data-structure
change to `CActor`/`CMap`/`CLayer` or any `CPath::read/write` field would
(see AGENTS.md) — adding an algo never should.

No new source files → `bin/refresh.py` not required.

## 3. Tests

- `tests/unit/t_ai_path.cpp` (`test_pathfinding`): reuse `TestSprite`,
  `checkPath`, `fillMap`, `wallColumn`. Minimum cases: open-grid optimal
  length, gap-wall detour, fully sealed wall → empty, out-of-bounds
  start/goal → empty. Add the algo to the seeded 12×12 fuzz loop and
  assert path length parity (and both-empty-or-both-non-empty) against
  A*/BFS — that is the check that a copy did not drift.
- `tests/unit/t_tables.cpp`: `resolvePath("NEWALGO")` and
  `resolvePath("Path::NEWALGO")` resolve to the enum value.
- `tests/unit/t_actor.cpp` (`test_path_algos`): add to the `valid[]`
  table so `CPath::getPathAlgo` non-null is covered; invalid selectors
  must still return null.
- `tests/unit/t_bossdata.cpp`: the loaded-content `path <=` validity
  bound tracks the new highest enum value.

## 4. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw (header-only surface adds no new portability
   risk).
2. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`,
   `t_actor`).
3. Only after tests pass: `python itools/gen_compliance.py` to
   regenerate `docs/compliance/src-checklist.md`. Never bump hashes
   before testing.
4. Move the driving plan to `docs/plans/archive/`.

## 5. Non-goals seen so far (don't sneak them in)

- Weighted/terrain edge costs — no cost model exists.
- 8-direction movement — the `JoyAim` wire format can't express it.
- Replacing an existing algo or touching the `CPath` cache.
