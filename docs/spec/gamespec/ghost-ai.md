# Ghost (`TILES_GHOST`, `TYPE_GHOST`) Specification

Status: **Implemented** (2026-11-26; plan `docs/plans/archive/tiles-ghost-ai-plan.md`).
Tests: `testGhost` in `tests/unit/t_game_ai.cpp` (suite `t_game_ai`).
Implementation: `CActor::canMove` ghost gate + shared player-cell branch
(`src/actor.cpp`), `TYPE_GHOST` dispatch/guards (`src/game_ai.cpp`,
`src/game.cpp`); tile data `TILES_GHOST = 0x70`, `type=TYPE_GHOST`
(`src/tilesdata.cpp`, repurposed from `TILES_YELKILLER`).

`TILES_GHOST` is a `TYPE_GHOST` mob dispatched to `handleMonsterV3`
pursuit (`PATH_ASTAR`, `range=7`, no bullet). The type alters its
movement legality: the ghost passes **through solid walls** when
reaching the player, and nothing else about the pursuit behavior
changes.

## 1. Identity

- G1. Ghost identity is the actor's **own type**: `m_type == TYPE_GHOST`
  (`TileDef` of `TILES_GHOST` declares `type=TYPE_GHOST`). The gate never
  reads the map cell or `m_pu` (the under-tile — never identity), so
  pathfinding probes that teleport the sprite without a map swap keep
  ghost mode. `AI_GHOST` (`src/sprtypes.h`) is descriptive tile data; the
  flag itself gates nothing.
- G2. A `handleVamPlant` conversion `setType(TYPE_VAMPLANT)` drops ghost
  mode with no extra state; the current conversion list
  (`src/game_ai.cpp`) does not include `TYPE_GHOST`, so ghosts are not
  converted at all.

## 2. Phasing grants (walls only)

- G3. Ghost mode bypasses exactly two solidity gates in
  `CActor::canMove`: `scan_t::isSolid` (layer solidity) and
  `CGame::isGranularSolidFromPos` (granular quadrants).
- G4. The ghost early-allow branch admits any destination whose tile
  type is not swamp, not the player's, and not a monster type (covers
  wall tile types ordinary monsters reject).

## 3. Refusals kept

- G5. **Water/deadly:** the existing
  `(result.isWater || result.isDeadly) && m_type != TYPE_PLAYER` gate is
  not bypassed — ghosts never cross swamp/water or deadly tiles.
- G6. **Monster occupancy:** a ghost never enters a cell whose tile
  type is a monster type (`CGame::isMonsterType` — includes bullets,
  barrels, eggs); under the swap model the stamped tile **is** the
  occupancy.
- G7. **Attr cells:** a ghost never enters a cell with a non-zero map
  attribute (`ATTR_IDLE_*`, `ATTR_CRUSHER_*`, spawn/auto markers).
  Attr cells are re-stamped by their owning logic, which would fight
  the tile-swap over the cell's map tile. The gate checks the
  destination cell, so a ghost already standing on an attr cell can
  leave. Ghost-only; other actors unaffected.
- G8. **Player cell:** shared `TYPE_MONSTERV3`/`TYPE_GHOST` rule — the
  stamped player cell stays path-goal-reachable in `canMove`, while
  `CPath::followPath` (type-agnostic `isPlayerThere` final-step guard)
  and `CGame::shadowActorMove` (`TYPE_MONSTERV3`/`TYPE_GHOST` guard)
  refuse the actual move. Contact damage is `handleMonsterV3`'s
  `isPlayerThere` branch.

## 4. Known-scope behavior (accepted, not defects)

- K1. Because identity is the actor's type (G1), A* probes keep ghost
  mode and paths phase through walls of **any** thickness.
- K2. While a ghost stands inside a wall cell the wall tile is held in
  `m_pu`; the cell briefly reads as the ghost tile to `scanPos` (same
  swap semantics every actor uses; restored on leaving).
- K3. Ghost wander (`findNextDir` outside `range`) phases too — the
  gate is always-on.
