# Plan: wire TILES_GHOST — AI_GHOST wall-phasing TYPE_MONSTERV3

Date: 2026-11-26
Status: **Implemented** 2026-11-26 — ghost gate in `CActor::canMove`
(`src/actor.cpp`), `testGhost` pins (`tests/unit/t_game_ai.cpp`), spec
`docs/spec/gamespec/ghost-ai.md`, compliance checkpoint 2026-11-26
(`src-checklist.md`, 112/112 ctest).
Goal: a mob placed as `TILES_GHOST` path-finds to the player with
`AI_GHOST` and, when that flag is set, `TYPE_MONSTERV3` movement ignores
solid walls.

Revised 2026-11-26 after maintainer ruling: `CActor` knows its own tile —
the tile-swap movement model (`CActor::move`, `actor.cpp:242-255`) keeps
the actor's tile at `map.at(m_x, m_y)` while it stands. `canMove()`
derives its own TileDef from there; **no `CActor` member, no wire-record
change, no `ENGINE_VERSION` bump** (an earlier draft's `m_ghost` field is
withdrawn). This is a behavior-only change to a member function.

## Current state (already wired — do not re-do)

- `TILES_GHOST = 0x70` — `src/tilesdata.h:138`; TileDef row at
  `src/tilesdata.cpp:251`: `{type=TYPE_MONSTERV3, health=-9, speed=6,
  ai=64 /* AI_GHOST */, path=PATH_ASTAR, range=7, basename="ghost.png"}`.
  **0x70 is repurposed from `TILES_YELKILLER`** (staged maintainer rename
  2026-11-26): existing maps that stamped a yellow-killer now spawn a
  ghost. The old slot's `ANIMZ_YELKILLER` enums stay in `animzdata.h`
  (legacy anim defs, do not delete). `names.cpp` missed the rename and
  was fixed (`_N(TILES_GHOST)`, 2026-11-26) — build + 112 tests green.
- `AI_GHOST = 0x40` — `src/sprtypes.h:35`. **Consumed nowhere** (only the
  `tilesdebug.cpp` name table). Today the ghost is behaviorally a zombie.
- Anim: `TILES_GHOST → ANIMZ_GHOST` already registered
  (`src/animator.cpp:49`); debug strings exist (`tilesdebug.cpp:142,206`).
- Dispatch: `CGame::manageMonsters` → `handleMonsterV3`
  (`src/game_ai.cpp:577`) already drives any `TYPE_MONSTERV3` via
  `startPath`/`followPath` within `def.range`.
- Asset check (runtime test gate): `ghost.png` is not in this repo (tile
  art lives in the external data tree, like `zombie.png`). Confirm it
  resolves in the dev asset tree before runtime verification.

## The choke point

Every movement decision — actual steps (`CPath::followPath`,
`ai_path.cpp:552`), path node probing in A*/A*Smooth/BFS/LOS
(`ai_path.cpp:185-189, 289-292, 343-365, 500-503`), and wander
(`findNextDir`, `actor.cpp:303`) — funnels through
**`CActor::canMove()` (`src/actor.cpp:164`)**. One guard there covers all
callers. No changes needed in `handleMonsterV3`, `CPath`, or any path
algorithm, and no new spawn wiring: `spawnMonsters` (`game.cpp:749-757`)
already stamps the tile into the map, which is where the flag lives.

## Step 1 — the bypass in `CActor::canMove` (`src/actor.cpp:164`)

After the translate/bounds check, derive ghost identity from the tile
**at the actor's position** — that is the actor's own tile under the
swap model. `m_pu` is NOT the actor's tile: it is the tile *underneath*
what the actor is standing on (restored to the old cell by `move()`);
never derive identity from it.

```cpp
const bool ghost = (getTileDef(map.at(m_x, m_y)).ai & AI_GHOST) != 0;
```

Gate exactly four checks (the ghost bypasses **solid walls only**):

- `if (result.isSolid && !ghost) return false;` (~L176)
- `if (!ghost && game->isGranularSolidFromPos(pos, aim)) return false;`
  (~L180)
- `if (ghost && map.getAttr(newPos.x, newPos.y) != 0) return false;` —
  see attr rule below.
- In the `m_type == TYPE_MONSTERV3` branch (~L211): keep
  `if (def.type == TYPE_PLAYER) return true;` (path-goal reachability,
  final-step refusal unchanged — see occupancy rules below) and add
  `if (ghost && !game->isMonsterType(def.type)) return true;` so a ghost
  may enter any cell the solidity gates no longer refuse **except one
  occupied by another monster**.

Explicitly NOT bypassed (spec rules, not oversights):

- **Attr cells:** the ghost never enters a cell with a map attribute set
  (`ATTR_IDLE_*`, `ATTR_CRUSHER_*`, `ATTR_BOSS_*`, `ATTR_AUTO_*`,
  spawn/automator markers — anything `getAttr` returns non-zero). Those
  cells are game-logic-owned and get (re-)stamped by idles, crushers and
  spawners; under the swap model a ghost parked on one risks its own
  tile being overwritten — which would silently kill the tile-derived
  ghost identity above and leave a stray `TILES_GHOST` stamp in an
  attr-keyed cell. Avoidance is one line; reconciling stamping order is
  not. Non-ghost actors keep their current behavior (gate is
  ghost-only).

- **Water:** `(result.isWater || result.isDeadly) && m_type !=
  TYPE_PLAYER` (actor.cpp:178) stays ungated — ghosts cannot cross
  swamp/water (base `TYPE_SWAMP` or Water-type layer tiles per
  `scanPos`) or deadly tiles. The ghost's type is not `TYPE_PLAYER`, so
  the existing check blocks it for free.
- **Monster occupancy:** the swap model stamps every `m_monsters`
  occupant's tile into its base-map cell, so `isMonsterType(def.type)`
  (`game.cpp:712` — MONSTER, VAMPLANT, DRONE, ICECUBE, BOULDER,
  FIREBALL, LIGHTNING_BOLT, BARREL, EGG, MONSTERV3) is the occupancy
  test; a ghost never walks over another monster (or a bullet/barrel/
  egg in flight).
- **Player tile:** a ghost may *path-target* the player's stamped cell
  (existing V3 goal-reachability rule) but never *step* on it:
  `CPath::followPath` refuses the final step and `shadowActorMove`
  refuses any MONSTERV3 move onto a `TYPE_PLAYER` cell
  (`game.cpp:2329-2335`). Contact damage comes from `handleMonsterV3`'s
  `isPlayerThere`.

Identity comes from the tile def itself, so no `m_type` belt is needed:
`handleVamPlant` swaps the map tile to `TILES_VAMPLANT` alongside
`setType` (`game_ai.cpp:706-707`), so a converted ghost derives the
vAMPLANT def and loses phasing automatically. Do not widen the
water/deadly bypasses without a spec change.

## Step 2 — derivation correctness (maintainer-accepted scope)

- **Real steps** (`followPath` L552 step check, `findNextDir` wander,
  `shadowActorMove` callers): always evaluated while the actor stands on
  its own swapped tile → derivation is exact, including every step taken
  from inside a wall (the tile travels with the ghost).
- **Path-node probes** (`ai_path.cpp:185-189` etc.) teleport the sprite
  via `move(Pos)` *without* a map swap, so a probe node's derived def is
  the raw cell content there, not the ghost's. Consequence: A* path
  generation crosses a **1-tile-thick** wall exactly (step into wall
  probes from the ghost's real position → derives ghost def; step out of
  the wall onto background is solid-refusal-free), but cannot continue
  through a **thicker** wall. Inside/past the first wall layer the ghost
  advances via wander steps, each of which derives correctly from its
  real position — thick walls degrade to cell-by-cell phasing, not a
  stuck monster. Accept this; if thick-wall A* ever matters, the fix is
  a `ghost` argument threaded into `ISprite::canMove` for the four probe
  sites — do not pre-build it.
- Bounds checking (`map.isValid`, pathfinder bound tests) is untouched —
  ghosts still cannot leave the map.

## Step 3 — accepted side effects (documented, not fixed)

- **Tile swap:** while a ghost stands inside a wall cell, the wall tile
  lives in `m_pu` and the cell's base tile is the ghost tile — that cell
  is briefly non-solid to the *player* (`scanPos` reads the base layer as
  the ghost def). Inherent to the swap-movement design every actor uses;
  on leaving, the wall is restored from `m_pu`. Kill-mid-wall works the
  same as any actor dying on a non-background tile.
- **Wander phasing:** outside `def.range` the ghost wanders via
  `findNextDir`/`shadowActorMove` and will also drift through walls.
  The flag is always-on; "phasing only while pathing to player" would
  need per-call context in `canMove` — not worth it unless the design
  demands it.

## Step 4 — test pins (one suite, `tests/unit/t_game_ai.cpp`)

Extend the existing MONSTERV3 scenario helpers (`spawn(tile,
TYPE_MONSTERV3, ...)`, `tileOf`) with the smallest failing-without-the-fix
checks. Ghost identity now flows entirely from the map tile, so the
fixture must stamp `TILES_GHOST` under the actor (as `spawnMonsters`
does):

1. Map with a solid wall column; ghost at (x,y), player 2 tiles beyond
   the wall (within `range=7`).
2. Pin: ghost `canMove(aim_into_wall) == true`; a zombie-style V3
   (`ai=0`, e.g. `TILES_ZOMBIE`) at the same spot `== false`.
3. Pin: across a 1-tile-thick wall, `followPath` returns
   `MoveSuccesful` and the ghost's position advances into/past the wall
   (a zombie V3 stays `Blocked`/no-progress).
4. Pin: ghost `canMove(aim_into_water) == false` (stamp a `TYPE_SWAMP`
   tile in the aim direction).
5. Pin: ghost `canMove` toward a cell stamped with another monster's
   tile (e.g. `TILES_ZOMBIE`) `== false`; `shadowActorMove` toward the
   player's stamped cell returns false and leaves the ghost in place.
6. Pin: ghost `canMove` toward a cell with `setAttr(...)` non-zero
   `== false` (the gate checks the *destination* cell, so a ghost
   already standing on an attr cell can still leave).
7. Pin: vamp plant conversion (`setType(TYPE_VAMPLANT)` +
   `map.set(TILES_VAMPLANT)`) loses the phasing (regression pin for the
   tile-derived identity).

## Step 5 — docs & compliance (AGENTS.md workflow)

- `cmake -B build && cmake --build build`, then
  `ctest --test-dir build` (at minimum the `t_game_ai` binary) before
  flipping anything.
- Spec/compliance: check `docs/spec/README.md` modules for the monster
  AI section and `docs/compliance/README.md` counterpart; add an open
  item "AI_GHOST phasing" pre-implementation, flip it to verified after
  tests pass, and update that module's SHA256/tally/checkpoint rows —
  never bump the hash before testing.
- When fully implemented, move this file to `docs/plans/archive/`.

## Commit suggestion

Single commit: `feat(ghost): AI_GHOST wall-phasing for TYPE_MONSTERV3`
(`src/actor.cpp` + tests; docs as a separate staged doc commit if the
compliance rows change).
