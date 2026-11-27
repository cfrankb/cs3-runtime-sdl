# Implementation Plan: RaycastBullet path algorithm (Path::RAYCAST_BULLET)

Status: implemented (2026-11-27)
Date: 2026-11-27
Brief: [docs/briefs/ipath.md](../briefs/ipath.md)
Model: sibling of `Raycast` in `src/ai_path.cpp` (geometric ray family, no node loop)

## 1. Goal

Add `RaycastBullet`: an Amanatides & Woo fast voxel-traversal (DDA) ray over
the half-tile grid, selectable via `path=RAYCAST_BULLET` / `algo=RAYCAST_BULLET`,
for bullet attacks that must follow the *exact cell sequence the straight
line passes through*.

## 2. Scope check (brief §0) — why this is genuinely new

- Bresenham-for-bullets already exists: `Raycast` (`Path::RAYCAST`), a
  generalized-Bresenham staircase. Do **not** re-add it.
- Bresenham and DDA are *not* the same traversal on this contract. Bresenham
  balances axis steps evenly; DDA emits axis crossings in line-parameter
  order, i.e. the cells the segment from center to center actually enters.
  Discriminator, start `(0,0)` → goal `(1,3)` (dx=1, dy=3):
  - `Raycast`: `DOWN RIGHT DOWN DOWN` — visits `(1,1)`, a cell the line
    never passes through (line is still in column 0 at row 1).
  - DDA: `DOWN DOWN RIGHT DOWN` — visits only touched cells.
  Consequence for bullets: `CPath::followPath` probes `canMove` per step, so
  the two orders *detonate the bullet in different cells* when walls sit in
  the disagreement cells. DDA is the physically-correct sight ray; Bresenham
  hugs the dominant axis early on steep/shallow slopes.
- Not a post-processing pass, no `Node`/`OpenList` (the BUG-C9/BUG-H17
  hygiene is structurally absent, as in `Raycast`/`FlowField`).

## 3. Design

### 3.1 Class surface (`src/ai_path.h`)

```cpp
// Grid traversal (Amanatides & Woo DDA) "pathfinding" class: the exact
// supercover cell sequence of the segment sprite->goal, no walkability
// probe and no axis fallback (same runtime-resolved-ray contract as
// Raycast, see cpp comment).
class RaycastBullet : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};
```

### 3.2 Algorithm (`src/ai_path.cpp`, next to `Raycast::findPath`)

Copy `Raycast::findPath`'s bounds validation byte-for-byte (same `LOGE`
text convention, `return {}` on out-of-bounds; no per-step bounds check —
the traversal stays inside the start/goal bounding box) and the "no probe
on purpose" comment rationale (probe-trimmed ray parks a `NoTTL` bullet
forever; `followPath`'s `Blocked` detonates it). Replace only the emission
loop with an integer DDA (no floats — pedantic-clean and bit-identical on
g++/clang/msvc/mingw/zig):

```cpp
// Amanatides & Woo DDA in integer rationals: next vertical grid crossing
// is at line-parameter (i+1)/dx, next horizontal at (j+1)/dy; compare by
// cross-multiplication. Exact-corner ties emit the x step first (the
// canonical x-first convention shared with AStarSmooth's staircase
// oracle), so both corner-adjacent cells are visited (supercover: a
// bullet never leaks through a diagonal wall seam).
std::vector<JoyAim> directions;
const int dx = std::abs(playerPos.x - startPos.x);
const int dy = std::abs(playerPos.y - startPos.y);
const int stepX = playerPos.x >= startPos.x ? 1 : -1;
const int stepY = playerPos.y >= startPos.y ? 1 : -1;
int i = 0, j = 0;
while (i < dx || j < dy)
{
    if (j >= dy || (i < dx && static_cast<int64_t>(i + 1) * dy <= static_cast<int64_t>(j + 1) * dx))
    {
        ++i;
        directions.push_back(stepX > 0 ? AIM_RIGHT : AIM_LEFT);
    }
    else
    {
        ++j;
        directions.push_back(stepY > 0 ? AIM_DOWN : AIM_UP);
    }
}
return directions;
```

Properties (asserted by tests, §5):
- Exactly `dx + dy` steps; `g_steps`-free (aims derived from sign deltas
  like `Raycast`; the two geometric rays are the only algos that skip
  `g_steps` — accepted for this family).
- Axis-aligned rays and exact 45° rays produce output identical to
  `Raycast` (the algorithms coincide there); they differ only on
  non-corner slopes of ratio ≠ 1:1.
- Products bounded by (map half-tile extent)² — `int64_t` cross-products
  make overflow impossible; no float, no UB.
- No new members, no `Node`, no hashing. `CPath::followPath` caching: no
  change.

`ponytail:` comment in code: DDA ≡ `Raycast` for 1:1 and axis rays; if the
family ever grows a third geometric ray, share one loop via a tie-order
lambda instead of a third copy.

### 3.3 Semantics doc decision

`RaycastBullet` inherits `Raycast`'s §2-contract deviation (deliberately
**not** probe-checked; runtime resolves the ray). Document as a second row
in `docs/spec/ipath-implementations.md` §1 + a §3 subsection stating the
Bresenham-vs-DDA discriminator and when to pick which:
- `RAYCAST`: even staircase interleaving (legacy look/feel, already in
  content);
- `RAYCAST_BULLET`: exact sight-line collision cell for bullet attacks.

## 4. Wiring (brief §2 touch points)

| File | Change |
|---|---|
| `src/bossdata.h` | append `RAYCAST_BULLET` after `RAYCAST` in `BossData::Path` (append-only enum, wire-backward-compatible) |
| `src/names.cpp` | append `_N(RAYCAST_BULLET)` to `kPaths[]` |
| `src/ai_path.h` | `RaycastBullet` class decl |
| `src/ai_path.cpp` | `const RaycastBullet raycastBullet;` in `namespace PathData` + impl + `getPathAlgo` branch `else if (algo == BossData::RAYCAST_BULLET) return &PathData::raycastBullet;` |

Mirror fixtures (mirror rule, same change):
- `docs/spec/bosses/bossdata_runtime.h` — `BossData::Path` enum
- `docs/spec/bosses/bossdata_runtime.cpp` — `kPaths[]` name map
- `docs/spec/bosses/parse.py` — `HEADER_STRUCT` enum
- `docs/spec/bosses/bosses.ini` — mention in the documented `algo=` values if listed

No new source files → `bin/refresh.py` not required. No `CActor`/`CMap`/
`CLayer` data-structure or `CPath::read/write` change → **no
`ENGINE_VERSION` bump** (enum-value-only append, same ruling as Dijkstra).

## 5. Tests

### `tests/unit/t_ai_path.cpp` (`test_pathfinding`)

Add `const RaycastBullet raycastBullet;` beside `raycast`:

1. **Discriminator (shape, not just length):** open grid
   `(0,0) → (1,3)` yields exactly `AIM_DOWN AIM_DOWN AIM_RIGHT AIM_DOWN`;
   and `raycast` yields `DOWN RIGHT DOWN DOWN` — pins that the two algos
   are *not* accidentally the same loop.
2. Exact-corner diagonal `(0,0) → (3,3)`: supercover `RIGHT UP RIGHT UP
   RIGHT UP` (x-first tie), identical to `raycast`.
3. Axis row `(2,2) → (6,2)` (4×AIM_RIGHT) and mirror-slope check
   `(3,0) → (0,2)` — all four quadrant sign combinations emit the correct
   aims.
4. Un-probed wall ray: sealed wall column between start and goal —
   `raycastBullet` must stay geometric (same expected step sequence as the
   open grid), mirroring the existing raycast §424 assertion.
5. Out-of-bounds start/goal → empty, no crash; start == goal → empty.
6. **Fuzz parity (length + emptiness):** in the seeded 12×12 loop, assert
   `raycastBullet.size() == raycast.size()` (both always `dx + dy` when
   non-empty) — the copy-drift check. *Not* cell-sequence parity with
   A*/BFS: this algo never routes, it has no wall semantics to compare.
7. DDA-vs-brute-force oracle: for the fuzz maps' start/goal pairs, replay
   the emitted aim sequence and assert every visited cell `(x,y)`
   satisfies the supercover predicate `|dy*(x+0.5-cx) - dx*...|` — skip;
   the exact-sequence cases 1–3 plus length fuzz are sufficient (ponytail:
   a full supercover oracle is a second implementation to test).

### `tests/unit/t_tables.cpp`

`resolvePath("RAYCAST_BULLET")` and `resolvePath("Path::RAYCAST_BULLET")`
resolve; name-count comment/expectations for "all nine names".

### `tests/unit/t_actor.cpp` (`test_path_algos`)

Add `BossData::RAYCAST_BULLET` to `valid[]`; invalid selectors still null.

### `tests/unit/t_bossdata.cpp`

Loaded-content `path <=` validity bound tracks the new highest enum value
(`<= RAYCAST_BULLET`).

## 6. Verification loop (AGENTS.md / brief §4)

1. `cmake -B build && cmake --build build` — pedantic-clean on
   g++/clang/msvc/mingw (integer-only math; Qt/wasm surfaces untouched).
2. `ctest --test-dir build` (at minimum `t_ai_path`, `t_tables`,
   `t_actor`, `t_bossdata`).
3. Only after tests pass: `python itools/gen_compliance.py`; update
   `docs/spec/ipath-implementations.md` inventory + §3 in the same commit
   (its maintenance rule), never bump hashes before testing.
4. Move this plan to `docs/plans/archive/`.

## 7. Risks / non-goals

- **Risk — content mismatch:** existing `algo=RAYCAST` bullets keep the
  Bresenham staircase; nothing is re-pointed. Switching a boss to
  `RAYCAST_BULLET` changes which wall cell the bullet dies on (that is the
  feature — flag it in the ini comment when content adopts it).
- Non-goals (brief §5): weighted costs, 8-direction output (the wire format
  can't express the DDA diagonal corner shortcut even though voxel DDA
  could), continuous/pixel-space ray start with distance cutoff and
  "first-hit cell + distance" return (the `IPath` interface is
  integer start→goal, unit-step list; a bullet *query* API would be a new
  subsystem, not an `IPath`), replacing `Raycast` or touching the `CPath`
  cache.

## 8. Suggested commit

`ai_path: add RaycastBullet path algo (Path::RAYCAST_BULLET), Amanatides & Woo DDA sight ray`
