# BUG-H17 (High) A*/AStarSmooth relax-in-place corrupts the open-list heap order

Status: **Fixed** (2026-11-26)
Date: 2026-11-26
Found by: hidden-bug review of `AStar` / `BFS` / `LineOfSight` requested
2026-11-26; regression latent in the BUG-C9 fix
(`docs/bugs/resolved/critical-triage-2026-11-17.md`).
Severity: Medium–High — wrong (suboptimal) path returned; no UB, no crash.

### BUG-H17 (High) `gCost` relaxation mutates keys of entries already inside `std::priority_queue`

- **Where:** `src/ai_path.cpp` `AStar::findPath` relaxation branch
  (`it->second->gCost = newGCost; openList.push(it->second.get());`,
  L202–211) and the identical `AStarSmooth::findPath` branch (L517–524).
  Shared enablers: `CompareNode` (L85–91) dereferences `Node::fCost()` at
  comparison time; `openList` is a `priority_queue<Node *>` of **live
  pointers**.

- **Symptom:** none visible today — paths merely *can* be longer than
  optimal. Mechanism: on a g-cost improvement the fix relaxes the `Node`
  **in place**, but the *stale raw pointer copy* of that same `Node` is
  already parked at a fixed slot mid-heap. Its effective key just dropped,
  typically below its parent's key → min-heap invariant breached
  (`parent.f <= child.f` no longer holds). `std::priority_queue` has no
  decrease-key/reheapify, so every later `push`/`pop` (`push_heap`/
  `pop_heap` assume a valid heap) operates on a corrupted structure and
  `top()` is **not** guaranteed to be the global minimum fCost.
  Consequences:
  1. A node can pop (and get closed) **out of f-order**, before a lower-f
     predecessor has expanded; the later better g-route through that
     predecessor is then rejected by the `closedList.find(newPos)` guard
     (L182 / L496) and lost → the goal, if popped early, reconstructs at a
     non-minimal g; the returned `vector<JoyAim>` is longer than the true
     optimum (still a legal walkable path — it is never *invalid*).
  2. The BUG-C9 pop-skip reason comment ("the heap sees the improved
     fCost") is only half-true: the *re-pushed* copy percolates with the
     new key, the stale copy's slot was fixed long ago.
  With uniform edge cost + Manhattan h, fCost ties are massive, so greedy
  pop-order deviations are rarely observable — which is why the fixed
  length assertions in `test_pathfinding` (3, 10, 10 steps) pass both pre-
  and post-corruption. This is a *latent* wrongness, not a red test.

  Minimal shape of the violation (keys = fCost): heap array
  `[5, 7, 6, 9, 10]`, relax the node at index 1 from 7 → 3 and re-push.
  Array `[5, 3, 6, 9, 10, 3]` violates 5 ≤ 3; `pop()` still returns the
  5-entry even though 3 exists at a non-root slot.

- **Fix:** make queue entries **immutable key snapshots**, the standard
  lazy-deletion A* pattern, so nothing in the heap is ever mutated:

  ```cpp
  std::priority_queue<std::pair<int, Node *>,
                      std::vector<std::pair<int, Node *>>,
                      std::greater<>> openList;
  // every push:
  openList.emplace(node->fCost(), node);
  // on pop, before the goal test:
  auto [f, current] = openList.top();
  openList.pop();
  if (f != current->fCost()) continue; // stale snapshot
  if (closedList.find(current->pos) != closedList.end()) continue; // BUG-C9 skip stays
  ```

  In-place relaxation of the `Node` (owned by `nodes`) remains correct —
  the heap no longer holds pointers-qua-keys. Apply identically in
  `AStarSmooth`. BFS (`std::queue`) and `LineOfSight` (no search) are
  unaffected. `Node`, `CompareNode` (delete it), and the `nodes` arena are
  otherwise untouched. No serialized state touched → **no
  `ENGINE_VERSION` bump**.

- **Verify:**
  1. `cmake -B build && cmake --build build && ctest --test-dir build`
     (`t_ai_path`, `t_tables`) — existing fixed-length assertions must
     still pass.
  2. Ground-truth check: in `t_ai_path.cpp`, for the seeded
     `std::mt19937` random 12×12 maps proposed in
     `docs/plans/dijkstra-plan.md` §5, assert `AStar.findPath(...).size()
     == BFS.findPath(...).size()` per map (both-empty = pass). BFS uses a
     plain FIFO and is order-corruption-immune, so any disagreement is
     this bug firing. Optionally assert the same for the BUG-C9 relaxation
     map (8×6, start (2,5), goal (7,0)).
  3. ASan build re-run (`build-asan`) to confirm the BUG-C9 memory-safety
     property is preserved (entry lifetime unchanged; pairs hold raw
     pointers into the still-live `nodes` arena).
  4. On fix: move this file to `docs/bugs/resolved/`, append the BUG-H17
     row alongside the BUG-C9 entries in the resolved triage docs, and
     refresh the `ai_path.cpp` SHA256 row in
     `docs/compliance/src-checklist.md` via `python itools/gen_compliance.py`
     **after** tests pass (never bump the hash first).

- **Status:** **Fixed.** Both `AStar` and `AStarSmooth` open lists now hold
  immutable `OpenEntry = std::pair<int /*fCost snapshot*/, Node *>` under a
  `std::greater` min-heap; pops skip stale snapshots
  (`entry.first != current->fCost()`) before the BUG-C9 `closedList` skip.
  `CompareNode` removed (its live-key read was the corruption enabler).
  In-place relaxation of the arena-owned `Node` unchanged, so the BUG-C9
  memory fix stands. Ground truth pinned in `test_pathfinding`: 20 seeded
  12×12 random maps assert `AStar.size() == BFS.size()` per map. Plain
  `ctest` 112/112 and the ASan `t_ai_path` run are clean.
