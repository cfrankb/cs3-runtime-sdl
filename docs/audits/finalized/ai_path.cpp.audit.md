# Audit: src/ai_path.cpp
Date: 2026-11-17

## Summary
Four path searchers (AStar, BFS, LineOfSight, AStarSmooth) plus a caching driver (CPath). The core algorithms are structurally sound (Manhattan heuristic is admissible/consistent, goal check on pop is correct, BFS gives shortest paths, all searches terminate), but both A* variants contain a **use-after-free**: improving a node's g-cost destroys the old `Node` via `unique_ptr` reassignment while its raw pointer is still sitting in the priority queue. One unchecked downcast, ignored validation results in `read()`, and several perf/style issues round out the list. Counts: 1 Critical, 1 High, 5 Medium, 9 Low.

## Findings

### Critical
- L187 (and L479 in `AStarSmooth::findPath`): [ub] **FIXED 2026-11-17** (docs/bugs/resolved/critical-triage-2026-11-17.md BUG-C9; `test_pathfinding` relaxation map + ASan). Original finding: On g-cost improvement, `nodes[newPos] = std::make_unique<Node>(...)` destroys the previous `Node` while the old raw pointer is still in `openList` — every subsequent heap sift (`push`/`pop`, comparator `CompareNode` dereferences `a->fCost()`), and any eventual pop of that entry, touches freed memory. Any g-cost relaxation (common in A*) is a use-after-free. Fixed: g-cost relaxations update the live `Node` in place and re-push its pointer; pop skips stale entries via `closedList` membership.

### High
**FIXED 2026-11-17 (BUG-H13):** `dynamic_cast` + null-check + LOGE. **L516: [ub] `*static_cast<CActor *>(&sprite)` — unchecked downcast of `ISprite&` for every non-boss. Currently safe only by caller convention (`CActor::followPath` at actor.cpp:580/603 and `CBoss::followPath` at boss.cpp:372 are the only callers; non-CActor callers would be UB). Fix: hoist `shadowActorMove`-equivalent into `ISprite`, or `dynamic_cast<CActor *>` + null-check + `LOGE`. (needs verification that no future/other ISprite reaches this)

### Medium
- L117–121 / L442–445: [algo] **FIXED 2026-11-17** (BUG-C9 relaxation redesign; stale-pop skip via `closedList` verified at ai_path.cpp:122-123 and :464-465). Original finding: No staleness check on pop: a node already in `closedList` can be popped again (duplicates are pushed on every improvement) and re-expanded, wasting work; this is also the delivery mechanism for the Critical UAF. Same one-line guard as above fixes both.
**FIXED 2026-11-18: all three gates now `return false` (BUG-H6a policy);
forged-record cases pinned by `t_ai_path::test_path_load_validation`.**
- L581, L590, L593: [error-handling] `checkBound(...)` return values are ignored — a corrupt save with `pathSize`/`m_pathIndex`/`m_pathTimeout` over `MAX_PATH_SIZE` logs but is then loaded anyway. Fix: `if (!checkBound(...)) return false;`.
- L117 / L218 / L442: [perf] No node-expansion cap on any search. An unreachable goal forces a full-grid exploration, and each expansion does 4 × (move + canMove + move-back) sprite mutations (L174–177, L265–268, L469–472) — the most expensive inner loop possible. `MAX_PATH_SIZE` (L40) exists but only gates save loading. Fix: cap expansions (e.g. L40 * factor) and return `{}` with `LOGW`.
**FIXED 2026-11-18: clamps negatives to 0.**
- L649–651: [bug] `setTimeout(int)` assigns straight to `size_t m_pathTimeout`; a negative argument wraps to a huge value, disabling path expiry. Guard with `if (timeout >= 0)` / clamp.
**RESOLVED 2026-11-18 (maintainer ruling: "if start==goal then don't do a path
search"): `CPath::followPath` early-returns `NoValidPath` when
`sprite.pos() == playerPos`, skipping the empty re-search. Searcher
empty-vector semantics unchanged. Pinned in `test_pathfinding`.**
- L123–155 / L223–255 / L309: [algo] "start == goal" returns an empty vector, indistinguishable from "no path"; `CPath::followPath` (L497) maps empty → `NoValidPath`, so an actor standing on its goal is reported as unpathable. Empty-vector-as-failure overloads `IPath::findPath`; if this matters, give `IPath` a `Result` out-param. (needs verification whether the gameplay symptom matters)

### Low
**PARTIAL FIX 2026-11-18: comparison is now `== 0`. The empty-path
re-search-every-turn behavior left as-is (plausible intentional retry).**
- L491: [bug] `m_pathTimeout <= 0` on unsigned `size_t` is a tautological comparison; write `== 0`. Also when the recompute at L493 yields an empty path (L497–502), the timeout isn't consumed, so blocked actors re-run the full search **every** turn despite `PATH_TIMEOUT_MAX` — may be intentional retry, but it defeats the timeout's cost-limiting purpose.
- L580, L589, L592: [bug] **FIXED 2026-11-18 (0x0011 cluster):** `read()`/`write()` now use `uint16_t` locals for `pathSize`/`m_pathIndex`/`m_pathTimeout` (wire layout unchanged; LE dependency gone, in-memory types no longer alias the 2-byte fields). Values stay bounded by `MAX_PATH_SIZE`/`PATH_TIMEOUT_MAX`. Original finding: `_R(&pathSize, 2)` reads 2 bytes into a zero-initialized `size_t` (and same for `m_pathIndex`/`m_pathTimeout`): little-endian-dependent, and the matching `_W` silently truncates any value > 65535 on write (the 4096 cap is never enforced there).
**FIXED 2026-11-18: `read` rejects `dir >= TOTAL_AIMS` (searches only ever
emit `g_dirs` values; downstream `CGame::translate` switch made garbage aims
inert but they polluted `setAim`). Pinned in `test_path_load_validation`.**
- L584–585: [error-handling] `JoyAim dir` is read from the save with no range check against `TOTAL_AIMS`; a garbage value flows into `sprite.setAim()`/`canMove()`. (needs verification of downstream range handling)
- L261, L465: [perf] **FIXED 2026-11-18** — BFS lookup is now `visited.find(newPos) != visited.end()` (only `true` is ever inserted, so membership is equivalent); the AStarSmooth `closedList` probe was already converted to `find()` in the BUG-C9 round. Original finding: `visited[newPos]` / `closedList[newPos]` in lookup contexts default-inserted keys, bloating the map on every rejected neighbor.
- L477: [perf] **FIXED 2026-11-18** — relaxation block now opens with one `nodes.find(newPos)`; the null/default insert is gone and the `else`-branch reads go through the iterator. Original finding: `nodes[newPos]` hashed up to three times per neighbor — and `if (!nodes[newPos])` *inserted a null entry* for every probed cell.
- L381–401: [perf] **NOTED 2026-11-18** — accepted as-is per audit ("acceptable for short paths"): `smoothPath` remains O(n²) LOS probes, each an O(path) walk with sprite moves. No code change.
- L41–49: [shrink] **FIXED 2026-11-18** — merged into `PathData::Step { Pos delta; JoyAim dir; } g_steps[]`; all three search loops index `std::size(g_steps)` so the pairing is structural. Original: `g_dirs[]` and `g_deltas` parallel arrays kept in sync only by a comment.
- L385–391: [dead] **STALE 2026-11-18** — `smoothPath` was rewritten in the BUG-C9 round; the unreachable `start`/`end` bounds check no longer exists.
- L499–500, L524–525: [dead] **FIXED 2026-11-18** — commented-out `LOGI` blocks deleted.
- L539–573: [shrink] **FIXED 2026-11-18** — replaced with a 3-arg `checkBound(name, value, max)` lambda (`LOGE` names the field, gate returns false); all three gated fields are `size_t` after the 0x0011 `uint16_t`-local reads. Original: 35-line `if constexpr` machinery whose result was ignored.
- L57–64: [shrink] **FIXED 2026-11-18** — `std::hash<Pos>` body now packs the int16 pair injectively into 32 bits (`((uint32_t)(uint16_t)x << 16) | (uint16_t)y`) instead of relying on sign-extended shifts.

## Notes
- All searches probe walkability by physically calling `sprite.move(pos)` / `sprite.canMove(aim)` / `sprite.move(originalPos)` mid-search. If `ISprite::move` ever grows side effects beyond position (animation state, collision bookkeeping, aim changes), all four algorithms break silently. Worth an explicit `ISprite::canMove(from, dir)` predicate to kill the mutation dance.
- The Critical UAF is data-dependent (needs one g-cost relaxation, i.e. two routes to the same cell) and may be masked by allocators reusing the freed block with identical contents. Verify under ASan with any A*/AStarSmooth-using boss/actor on a map with obstacles. **DONE 2026-11-18** — verified under ASan with the `test_pathfinding` relaxation map (BUG-C9 round).
- Direction convention (L44–49 comment) pairs `g_dirs[i]` with `g_deltas[i]`; the pairs as written (Up/Down/Left/Right) are internally consistent — runtime check against `CGame::translate()` behavior still stands. **RESOLVED 2026-11-18** — pairing is now structural in `g_steps`; the convention comment moved with it.
- `AStar`, `BFS`, `AStarSmooth` duplicate the identical pos→JoyAim emission loop (L136–154, L236–254); one shared helper would remove ~40 lines. **DEFERRED 2026-11-18** — same ruling as the `handleMonster`/`handleMonsterV3` duplication in game_ai.cpp: extraction deferred. (Superseded 2026-11-26: the `handleMonster`/`handleMonsterV3` duplication was resolved by merging into one handler, commit `977e2a1`; this path-algo duplication remains deferred.)
