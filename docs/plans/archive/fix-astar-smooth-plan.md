# Implementation Plan: Fix broken `ASTAR_SMOOTH`

Status: implemented 2026-11-26
Date: 2026-11-26
Target: `AStarSmooth::smoothPath` / `AStarSmooth::findPath` in `src/ai_path.cpp`
(L384–528)

## 1. Background and reproduction status

`ASTAR_SMOOTH` was historically "broken" and consequently no content in
`bosses.ini` uses it (only `ASTAR`, `BFS`, `LOS` appear; the maintainer does
not recall the original symptom).

Current empirical state (harnesses linking `libsrc_lib.a`, deterministic
layouts + ~400 seeded random maps, single-cell sprite at gf=1 **and** a
2×2 half-tile hitbox sprite at gf=2 emulating `CBoss::canMove`):

- `AStarSmooth::findPath` never enters a solid cell, never returns empty
  where `AStar` succeeds, and always returns length equal to `AStar`.
- Because the greedy `LineOfSight` router it uses only ever steps toward
  the goal (each step reduces Manhattan distance), any successful probe
  segment is automatically optimal-length — so a length regression is
  structurally impossible with symmetric `canMove`.

Conclusion: the surviving breakage is in the *smoothing semantics*, cost,
robustness and test coverage (§2), not in interface-level correctness.
This plan fixes those; if the original symptom was in-game (boss stall /
clip with the real `CBoss::canMove` asymmetries), the new regression tests
in §4 are the net that catches it when the entity is identified.

## 2. Defects to fix (all in `src/ai_path.cpp`)

### D1 — The "line-of-sight" oracle is not line-of-sight (real bug)

`smoothPath` decides "path[i-1] sees path[j]" by testing
`!los.findPath(sprite, end).empty()` (L418). `LineOfSight::findPath` is a
greedy *router*: when its preferred axis is blocked it falls back to the
other axis and happily walks around walls. Consequences:

- It "sees" through walls of any thickness whenever a gap exists → a
  "smoothed segment" can be a routed detour through cells the A* search
  never preferred (arbitrary, wall-hugging path shapes; unpredictable
  shape selection is exactly the "looks broken" failure mode).
- The emitted segment (`los.findPath(sprite, path[j - 1])`, L428) is a
  second, independent greedy walk — not verified to be a prefix of the
  tested one.

**Fix:** replace both uses with a real staircase-clear oracle, a private
helper on `AStarSmooth`:

```cpp
// probes the deterministic x-first staircase between two in-bounds
// cells; fills `steps` and returns true only if every step's canMove
// probe succeeds (no axis fallback, no routing around anything)
bool segmentSteps(ISprite &sprite, const Pos &from, const Pos &to,
                  std::vector<JoyAim> &steps) const;
```

Walk: `dx` steps of the horizontal aim, `dy` steps of the vertical aim
(same aim mapping as `LineOfSight`/`CGame::translate`), each probed with
the existing `sprite.move(cell); sprite.canMove(aim); ...` save/restore
pattern (L343–346). Bounds are guaranteed: the staircase stays inside
the bounding box of two in-bounds path points.

### D2 — Whole-path bail on a theoretically-unreachable failure

L428–435: if the re-emitted segment comes back empty, `smoothPath`
`LOGE`s and returns `{}`, killing the *entire* valid path. With D1's
oracle the unit segment `path[i-1] → path[i]` is the very edge A* already
validated, so a total failure is unreachable — but make it fail-soft
anyway: on segment failure, emit the single unit step toward `path[i]`
(map its delta through the existing `PathData::g_steps` table) and
continue, `LOGW` not `LOGE`.

### D3 — O(n²) duplicate probing (flagged NOTED in
`docs/audits/finalized/ai_path.cpp.audit.md` L40 — this plan supersedes
that acceptance)

Every accepted `j` runs the oracle, then the accepted run is re-executed
to emit the segment. Fix as a by-product of D1: `segmentSteps` fills
`steps`; keep the last successful `steps` and emit it directly — one
probe walk per oracle call, no second walk.

### Non-defects (do not touch)

- BUG-C9 relax-in-place / stale-copy-skip machinery in
  `AStarSmooth::findPath` — correct since 2026-11-17.
- `closedList` `find()` probes (2026-11-18 ruling).
- The A*/AStarSmooth duplicated emission loops — extraction stays
  DEFERRED per the 2026-11-18 audit ruling.
- `BossData::Path` enum values, `CPath::read/write` wire layout — no
  serialized-state change, **no `ENGINE_VERSION` bump**.

## 3. Expected behavior change

Path *lengths stay identical to `AStar`* — 4-connected unit-step output
means no smoother can go below A*'s optimal length (each `JoyAim` is one
cell). The visible change is *shape*: smoothed paths now take the
deterministic straight-first staircase between anchors instead of
wall-detouring greedy routes, and turns cluster where the corridor
forces them. State this in the test names so nobody "fixes" length later.

## 4. Tests (`tests/unit/t_ai_path.cpp`)

`test_pathfinding` currently covers `AStarSmooth` only on a straight
3-step path (L157) — the actual `smoothPath` loop never runs a turn.
Add, reusing `TestSprite`/`checkPath`/`fillMap`/`wallColumn`:

1. **Detour shape:** existing gap-wall map (`wallColumn(1, 0, 2)`),
   `(0,0)→(4,0)`: `checkPath("AStarSmooth", ..., 10)` (length parity
   with A*), plus a turn-count assertion: replay the aims, smoothed
   turns ≤ A* turns.
2. **S-bend:** two opposing wall columns (layout from the repro
   harness: columns x=3 y0..6 and x=8 y5..11 on a 12-grid,
   `(0,2)→(11,2)`): `checkPath` for length 21 on both algos.
3. **No-detour oracle:** a map where the old greedy oracle accepted a
   segment routed through a far gap (single wall column x=3, rows 0..6,
   segment anchors on opposite sides, row 6 gap): assert the smoothed
   path never enters the corridor cells between the old detour and the
   straight staircase — concretely: assert the smoothed aim sequence is
   the x-first-then-y staircase (exact sequence equality on this
   deterministic layout).
4. **Seeded fuzz:** port the repro loop — ~50 random 12×12 maps,
   `mt19937` fixed seed: `AStarSmooth` length == `AStar` length (both
   empty is a pass) and replay enters no `BLOCKED` cell. This is the
   regression net for unknown historical symptoms.
5. Keep the existing straight-line and sealed-wall/out-of-bounds cases.

`tests/unit/t_tables.cpp` and ini content: unchanged (no new enum, no
content flip — whether any boss *adopts* `ASTAR_SMOOTH` afterwards is a
content decision, out of scope).

## 5. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` (pedantic-clean; g++/clang/
   msvc/mingw — no new portability surface, same headers minus none).
2. `ctest --test-dir build` (at minimum `t_ai_path`).
3. Regenerate `docs/compliance/src-checklist.md` via
   `python itools/gen_compliance.py` after tests pass (hashes follow
   tested code, never before).
4. Move this plan to `docs/plans/archive/` when implemented; if the
   historical symptom is later pinned down, log it under
   `docs/bugs/resolved/` referencing this plan instead of `docs/bugs/`.

## 6. Suggested commit

`ai_path: fix AStarSmooth — real staircase-clear LOS oracle, fail-soft segments, single-pass smoothing, turn/fuzz coverage`
