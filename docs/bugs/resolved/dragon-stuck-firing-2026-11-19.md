# Dragon (TYPE_MONSTERV3) stuck after firing at player

**Date:** 2026-11-19 — **Status:** RESOLVED (ctest green, negative control fails on old code)

## Symptom

The dragon stops moving as soon as the player enters its `range` (7) and just
stands there firing fireballs every `rate` (16) ticks. Pathfinding "seems
broken".

## Root cause

In the live game the player's cell is stamped with their map tile
(`TILES_ANNIE2`, `TYPE_PLAYER` — `loadLevel` `src/game.cpp`, maintained by the
`CActor::move()` tile swap). `CActor::canMove()` (`src/actor.cpp`) admitted
`TYPE_MONSTERV3` onto `TYPE_BACKGROUND` cells only, so the search goal (the
player's cell) was never enterable: `AStar`/`BFS`/`LineOfSight` could never pop
the goal and `findPath()` returned empty. `CPath::followPath()` then returned
`NoValidPath`, and `handleMonsterV3()` (`src/game_ai.cpp`) treats any non-`Blocked`
result as "ok to shoot" — so the dragon fired but never moved.

`t_game_ai.cpp` missed it because `player().move(x, y)` teleports the player
without stamping the map, leaving the goal cell as background.

## Fix

1. `src/actor.cpp` `CActor::canMove()`: `TYPE_MONSTERV3` may also enter
   `TYPE_PLAYER` cells, so the search goal is reachable.
2. `src/ai_path.cpp` `CPath::followPath()`: a pathing `CActor` still never
   steps onto the player's cell (the `move()` swap would displace the player's
   stamped map identity); returns `NoValidPath` instead. Contact damage /
   halt at the player is the existing `AI_STICKY` check in `handleMonsterV3()`.
3. `src/game.cpp` `CGame::shadowActorMove()`: chokepoint guard — refuses to
   move any `TYPE_MONSTERV3` actor onto a `TYPE_PLAYER`-stamped cell. Covers
   both movement routes (path execution and the out-of-range `findNextDir`
   wander, which matters for the `ai=0` zombie tile that lacks `AI_STICKY`).
   Crusher/boss overlap semantics are untouched.
4. `tests/t_game_ai.cpp` `testMonsterV3()`: stamps `TILES_ANNIE2` on the
   player's cell, asserts the dragon advances to adjacency, halts there,
   leaves the player's stamp intact, and that `shadowActorMove` itself
   refuses the cell.

## Verification

- `cmake --build build && ctest --test-dir build` — 100% passed.
- Negative controls: reverting the `src/actor.cpp` hunk fails the advance
  check (`actor at (5,5); expecting (5,6)`); reverting the
  `src/game.cpp` guard fails the chokepoint check (`shadowActorMove stamped
  over the player`).

No serialized-state change (behavior-only in `CActor` member function), so no
`ENGINE_VERSION` bump per AGENTS.md.
