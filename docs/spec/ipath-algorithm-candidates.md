# IPath candidate algorithms — evaluation (2026-11-27)

Status: **evaluation / spec extension**, not state-of-the-world. The live
inventory remains
[ipath-implementations.md](ipath-implementations.md); anything adopted here
lands there (procedure: [../briefs/ipath.md](../briefs/ipath.md)) in the
same change as its implementation.

Evaluated against four gates, in order:

1. **Contract fit:** does it fit the §2 shared contract of
   `ipath-implementations.md` (4-dir unit steps, half-tile bounds,
   per-edge `canMove` probe, empty = no-path, `Node`/`OpenList`
   BUG-C9/BUG-H17 hygiene)? A *needed* contract or driver extension is
   not a rejection — it is scoped work to name and price.
2. **Prerequisites:** does the algorithm need an input it cannot compute
   itself (a data source that doesn't exist)? That is the **only** kind
   of veto, and it must come with the minimal step that unblocks it.
   Where the algorithm can derive its own costs from `canMove` probes,
   there is no prerequisite.
3. **Diff size** on the brief's touch points (class + `PathData` instance
   + `getPathAlgo` + append-only enum + `kPaths[]` + 3 mirror fixtures +
   tests).
4. **Test oracle:** parity fuzz where the algo is provably
   length-equivalent; a declared new oracle otherwise. A new oracle is a
   cost, not a rejection.

**Missing current content is never a negative.** Content cannot select an
algorithm that does not exist; ranking below orders *sequencing and
effort*, not permission.

## Verdict summary

| # | Candidate | Selector | Verdict | Priority |
|---|---|---|---|---|
| 4 | DDA true-ray | `RAYCAST_BULLET` | **Already implemented** — `RaycastBullet` shipped with the exact DDA described; nothing to do | done |
| 2 | Capped A* | `ASTAR_BUDGET` | **Adopt.** Fits contract as-is; de-risks every existing mob/boss at ~1 copy of the node loop | 1st |
| 1 | Greedy best-first | `GBFS` | **Adopt.** Near-free sibling of #2 (same loop, different key); swarm-heavy maps may later prefer a shared FlowField build — independent decision | 2nd |
| 3 | Flee / evade | `FLEE` | **Adopt**, bundled with a minimal named `CPath`/`IPath` driver concession (§3); completes the patrol/attack/flee movement vocabulary and unlocks the engage-in-groups behavior pattern | 3rd |
| 5 | Hazard-aware A* | `ASTAR_SAFE` | **Adopt the probe-derived variant now** (open-space cost from `canMove` probes — no new data). Full terrain hazards keep a real prerequisite: hazard tile data (§5) | 4th |

## 1. Greedy Best-First — `GBFS`

**What:** A*'s node loop with the open-list key reduced to `hCost`
(`f = Manhattan(n, goal)`), plus a hard expansion budget (counted on
non-stale pops).

**Contract fit:** copies `AStar::findPath` almost verbatim. Two loop
details to pin, not obstacles:

- `OpenEntry` snapshots must key on `hCost`, and the BUG-H17
  stale-by-snapshot skip must compare against `current->hCost`. `hCost`
  is position-fixed per node, so it never goes stale: duplicate pushes
  from `gCost` relaxations carry identical keys and are handled by the
  existing `closedList` pop-skip. Do **not** reuse `Node::fCost()` (it is
  `g + h` and shared with the other searchers).
- Equal-`h` ties are decided by heap insertion order → make the key a
  declared `std::pair<hCost, gCost>` (or equivalent) so behavior is
  deterministic across stdlib heap implementations; otherwise fuzz maps
  become compiler-flavoured.

**Oracle:** non-optimal by construction — the seeded A*/BFS
length-parity fuzz does not apply. Pin instead: both-empty-or-both-
non-empty vs BFS on open maps, every emitted step `canMove`-legal,
path length ≤ budget, and a sealed-goal case that *returns within budget*
(a plain A* run on the same map exceeds it).

**Note on swarms:** for many-agents-one-goal, `FLOWFIELD` is the
structural fit (one reverse-BFS field beats N greedy searches per player
step) and a shared-field builder is the known upgrade path. But field
edges are not every mob's edges — ghosts phase solids and occupancy
refusals are sprite-dependent — so per-agent `GBFS` remains the correct
selector for mixed mob types. Ship `GBFS`; the builder decision stays
independent.

## 2. Capped A* — `ASTAR_BUDGET`

**What:** the A* node loop with a constant expansion budget (suggest 256;
a 256×256-tile map is 65 536 cells at gf=1, 262 144 at gf=2 — full A* to
a sealed goal is the current unbounded cost). On budget exhaustion, do not
return empty: track the best-open/closed node by `hCost` (tie-break:
lower `gCost`, then lexicographic pos — declared so behavior is
deterministic and testable) and reconstruct the path to it. If that node
is the start, return `{}` (genuinely boxed in).

**Contract fit:** best. The result is still a legal `std::vector<JoyAim>`
of probed unit steps ending at an in-bounds cell; "failure = empty" is
untouched (budget exhaustion is *not* failure — it is a partial success
the caller cannot and need not distinguish). `CPath` already re-plans on
exhaustion and every `PATH_TIMEOUT_MAX` (10) steps, so a mob 150 tiles
out consumes the prefix, re-plans from a better cell, and re-plans
converge at the wall nearest the goal before returning empty — boxed-in
mobs idle exactly as they do today, with a bounded ~budget-node cost per
re-plan instead of an unbounded one. No oscillation beyond full A*: the
goal cell is fixed and the tie-break is deterministic.

Why 1st is *sequencing*, not *permission*: it is the only candidate that
improves already-shipped content (`zombie`, `ghost`, `bbdragondn`, the
demon boss on gf=2) with zero contract questions. It also supplies the
fail-fast the GBFS pitch attributed to greedy search — the budget, not
the greedy key, is what fails fast. Existing content may keep selecting
`ASTAR`; switching selectors per monster is a content pass, not part of
the algo change.

**Fringe A\* rejected as the vehicle:** fringe search is a different
search *structure* (no heap, repeated threshold scans) trading CPU for
memory the arena already bounds. Same goal, bigger diff — capped A* is
the same loop plus a counter and a best-node variable.

**Oracle:** when the budget is not hit, output must be **exactly**
`AStar`'s (test on maps small enough that 256 expansions always suffice,
or expose the budget as a ctor parameter with the shared instance using
the default). Sealed-goal case: non-empty, endpoint is the BFS-reachable
cell minimizing Manhattan-to-goal. Bounded work is structural (one `int`
counter) — no instrumentation test needed.

## 3. Flee / evade — `FLEE`

**What:** same node loop; prune `gCost > RADIUS` (e.g. 10); no goal test;
open-list key maximizes `hCost` from the threat (the `playerPos`
argument). Answer = max-`hCost` reached cell (tie-break: lower `gCost`),
reconstructed. The `gCost ≤ RADIUS` bound + per-edge `canMove` pruning
genuinely fixes the dead-end failure of naive opposite-direction steps:
a dead end inside the radius is only chosen if it is genuinely the
farthest reachable cell.

**Motivating content — the group-engage pattern.** `FLEE` is the third
leg of the mob movement vocabulary (patrol / attack / flee) and the only
missing one. The pattern it unlocks: a monster engages the player only
when enough of its kind are near (`ASTAR`-class selector when allies-in-
radius ≥ N), and otherwise flees to avoid a lone confrontation. No new
machinery is required for the switch itself:

- Ally counting is an O(live-monsters) Manhattan scan over
  `CGame::m_monsters` (`spawnMonsters` keeps it current) filtered by
  type — no spatial index needed at these counts.
- `CActor::followPath` re-reads `m_algo` every tick and
  `CActor::startPath` already takes the selector per call, so the
  policy is a per-tick selector flip before movement. `IPath` is
  untouched by the policy.
- Keep `Flee` a pure single-threat solver: `findPath` carries exactly
  one goal `Pos`, so group cohesion (a fleer must not sprint away from
  its own pack and never re-engage) lives in the caller policy — flip
  back to the attack selector when the mob is leashed too far from its
  spawn or has been alone too long. Do not widen `findPath` with rally
  points.
- `m_algo` serializes in `CActor`; a runtime flip to the new appended
  enum value is load-safe under the append-only rule (old saves never
  carry it).

**Driver concession (the named work, part of this adoption):**
`CPath::followPath` is approach-shaped — standing on the goal
short-circuits to `NoValidPath` *without searching*, so a fleeing mob
adjacent to the threat gets no move precisely when it needs one.
Minimal fix, contract-widening by one default:

- `IPath` gains `virtual bool flees() const { return false; }`; only
  `Flee` overrides it. All nine shipped algos inherit `false`, their
  behavior is bit-identical.
- In `followPath`, the on-goal short-circuit applies only when
  `!algo.flees()`. (The `isPlayerThere` final-step refusal and the
  cached-aims/recompute machinery need no change: a flee path moves
  away, so it can never step into the threat's cell, and
  `RADIUS ≤ PATH_TIMEOUT_MAX` means one flee plan is consumed exactly
  like any other.)

**Oracle (new, declared):** endpoint is the BFS-reachable max-Manhattan
cell within RADIUS; every step legal; and a pinned adjacency case —
sprite at/beside the threat returns a **non-empty** move away (this
locks the driver concession). Standard wiring tests as per brief §3
(`getPathAlgo` non-null, `resolvePath`, `t_bossdata` bound bump).

## 4. DDA true-ray — `RAYCAST_BULLET`

**Already implemented.** `RaycastBullet` (`src/ai_path.cpp`, shipped
2026-11-27) is the Amanatides & Woo integer-rational DDA exactly as
proposed: line-parameter step order, x-first corner ties, supercover
cells, no probe (runtime `followPath` detonation contract),
`(0,0)→(1,3)` Bresenham discriminator pinned in `t_ai_path`, plan
archived at `../plans/archive/raycast-bullet-plan.md`. `RAYCAST` is
retained unmodified for legacy hit cells. Remaining work is content
migration only, and `ipath-implementations.md` §1 already says it: new
straight-shot content selects `RAYCAST_BULLET`.

## 5. Hazard-aware / tactical A* — `ASTAR_SAFE`

Split into what is buildable now and what has a true gate-2
prerequisite.

**Buildable now — probe-derived open-space cost.** `stepCost(n) = 1 + k`
where `k` = number of the 4 neighbor edges out of `n` that the existing
`canMove` probe refuses (0..3), `k` scaled by a declared constant. No
new data source: every input is a probe the search already knows how to
make. Consequences to spec:

- Admissibility holds: min edge cost is still 1, so `Manhattan × 1`
  remains admissible and consistent; the BUG-H17 snapshot machinery is
  unchanged (keys are still `fCost() = g + h` snapshots).
- Cost is a property of the *cell* under the *same* sprite model, so it
  is cacheable per expansion; budget 3 extra probes per expanded node —
  compose with `ASTAR_BUDGET` (take its budget knob, keep worst case
  bounded).
- Oracle is partly free: **on a wall-free map every `k = 0`, costs are
  unit, and output must be exactly `AStar`'s** — that folds into the
  existing parity fuzz. New pinned cases: two equal-step routes, one
  hugging a wall → wall-free route chosen; single corridor where every
  route pays → route still found (penalty never refuses a step).
- This is `TACTICAL_ASTAR`'s "tiles adjacent to walls/other enemies"
  half; occupancy refusal is already in `canMove`, so crowded tiles cost
  more for free.

**True terrain hazards (lava/fire/trap soft costs) — vetoed on gate 2,
with the unblocking step named.** Verified against `CActor::canMove`:
the engine's only hazard concepts (`scanPos().isDeadly`, water,
`attr`-stamped cells) are **hard refusals** for mobs — there is no tile
datum a monster pays a penalty to enter. Minimal unblock: a hazard
annotation on `TileDef`/map data (or a cost hook on the sprite model),
plus widening the §2 probe contract from binary to a cost query. That
is a map-data design decision, priced separately; the search change
afterwards is one `+ cost(...)` in the relaxation. Re-evaluate when the
data source lands — the veto is on the missing input, not on demand.

## Adoption order and wiring notes

All selectors below are independent changes following
`../briefs/ipath.md` in full (append-only enum tail value, `kPaths[]`,
3 mirror fixtures, `getPathAlgo` branch, brief §3 tests, inventory +
§3 update of `ipath-implementations.md` in the same commit; no
`ENGINE_VERSION` bump for selector additions).

1. `ASTAR_BUDGET` — new tail value `ASTAR_BUDGET`.
2. `GBFS` — new tail value `GBFS`; declared non-optimal (parity fuzz
   exempt, both-empty-or-both-non-empty retained).
3. `FLEE` — new tail value `FLEE`; includes the `IPath::flees()` +
   `followPath` short-circuit change (regression-check the nine shipped
   algos' behavior is unchanged — they all inherit `flees() == false`)
   and the caller-side group-engage policy knobs (ally radius/count,
   leash); the `IPath` surface gains only `flees()`.
4. `ASTAR_SAFE` — new tail value `ASTAR_SAFE` (probe-derived open-space
   cost only); terrain-hazard extension deferred on its data prerequisite.
