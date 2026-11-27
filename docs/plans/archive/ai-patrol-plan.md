# AI_PATROL — idle patrol supplementing wall-hugging: detailed implementation plan

Status: draft (2026-11-27). Implements Task 4 of
[ai-enemy-movement.md](ai-enemy-movement.md); that summary's "0x80"
bit choice is superseded — the maintainer ruled **`AI_PATROL = 0x10`**.
Maintainer ruling #2 (this plan): **patrol supplements wall-hugging,
it never replaces or mutates it.** With the flag off (all shipped
content) behavior is byte-for-byte today's; with the flag on, a
patrol leg is *tried first* each tick and the **unchanged** wall-hug
tail is the fallback. No source files added → `bin/refresh.py` not
required. No `CActor` member added → **no `ENGINE_VERSION` bump**
(verify, §6).

## 0. Problem and shape of the fix

Out of `def.range`, `handleMonsterV3` (`src/game_ai.cpp:546-599`)
clears the path and runs the legacy tail: `CActor::findNextDir`
(`src/actor.cpp:323-344`) walks the fixed `g_aims` preference table and
takes the first `canMove` step — deterministic, wall-hugging. Opt-in
supplement, gated by one new flag:

- **Flag on, player out of range:** each tick, try one step of a
  patrol leg toward a cardinal post anchored on `CActor::m_origin`
  (spawn cell, set in `actor.cpp:85`, already serialized) via the
  existing `startPath`/`Path::LOS` stack. If that leg cannot step,
  execution falls through to the **existing tail, unmodified** — the
  wall-hug is the fallback, not the thing being fixed away.
- **Flag off, or leg failed:** identical code path as today, for
  `TYPE_MONSTERV3`, `TYPE_GHOST` and the load-bearing legacy
  `TYPE_MONSTER` dispatch alike.

No RNG anywhere: post selection is a pure function of tick + origin
(deterministic by construction — replays, save/load and tests are
bit-identical with no seeding contract). A previously-considered
Tier A coin-flip of `findNextDir`'s turn preference was **dropped by
maintainer ruling**: it mutated the wall-hug. Per-tick-locals only:
**no new members, no new wire fields, no save-format change.**

## 1. Flag (`src/sprtypes.h`)

`0x10` is the existing `AI_SMART` value. `AI_SMART` has **zero
consumers** in `src/` and `docs/` — alias it, do not delete it
(compatibility rule; the `AI_ROUND`/`AI_CLASSIC` alias at `0x02` is
the precedent):

```cpp
    AI_SMART                  = 0x10,
    AI_PATROL                 = 0x10, // alias: AI_SMART has no consumers;
                                      // repurposed bit for idle patrol
                                      // (docs/plans/ai-patrol-plan.md)
```

Caveat pinned in the comment: if a future change ever gives `AI_SMART`
semantics, patrol must move to a free bit (`0x80`). `TileDef.ai` is
filled only by the hand-written table in `src/tilesdata.cpp` — no ini
parser or `names.cpp` name map for AI flags exists, so there are no
fixtures to mirror. Content wiring is out of this change's default
(maintainer picks demo rows, e.g. a `zombie`-class entry); the tests
below construct flags directly.

## 2. Constants and waypoint helper (`src/game_ai.cpp`, file-static)

```cpp
static constexpr int PATROL_RADIUS = 6;     // cells from m_origin
static constexpr int PATROL_LEG_TICKS = 10; // post rotation cadence;
                                            // mirrors CPath's
                                            // PATH_TIMEOUT_MAX (10)

// Cardinal posts only (4-direction world), clamped to map bounds so
// LineOfSight::findPath never reaches its out-of-bounds LOGE path (it
// bounds-validates the goal, ai_path.cpp bounds contract). The
// origin-derived phase desynchronizes neighboring patrollers without
// state or RNG.
static Pos patrolPost(const Pos &origin, const uint32_t ticks,
                      const int w, const int h)
{
    static constexpr JoyAim g_posts[4] = {AIM_UP, AIM_RIGHT,
                                          AIM_DOWN, AIM_LEFT};
    const size_t phase = static_cast<size_t>(
        (ticks / PATROL_LEG_TICKS + origin.x * 3 + origin.y * 7) & 3);
    const Pos step = CGame::translate(origin, g_posts[phase]);
    Pos post{static_cast<int16_t>(origin.x + (step.x - origin.x) * PATROL_RADIUS),
             static_cast<int16_t>(origin.y + (step.y - origin.y) * PATROL_RADIUS)};
    post.x = static_cast<int16_t>(std::clamp<int>(post.x, 0, w - 1));
    post.y = static_cast<int16_t>(std::clamp<int>(post.y, 0, h - 1));
    return post;
}
```

`std::clamp` needs `<algorithm>` (add include if absent).

## 3. Call site — a supplement bolted onto the tail's front

Inserted in `handleMonsterV3` **between** the existing
`actor.clearPath();` (`game_ai.cpp:576`, out-of-range fall-out) and the
untouched `bool reverse = def.ai & AI_REVERSE;` tail head:

```cpp
    if (def.ai & AI_PATROL &&
        actor.startPath(patrolPost(actor.origin(), ticks,
                                   m_map.width(), m_map.height()),
                        BossData::Path::LOS, -1))
        return; // patrol stepped this tick; otherwise the wall-hug
                // tail below runs today's code, unchanged
    bool reverse = def.ai & AI_REVERSE;
```

That is the entire gameplay edit — two inserted lines of control flow,
zero edits to the tail. Design rulings, each deliberate:

- **Fresh path every tick.** `isFollowingPath()` is just
  `m_path != nullptr` (`actor.cpp:650`), so a kept-alive patrol path
  would keep walking stale cached steps toward a rotated-away post and
  would resurrect chase steps after the player leaves range (the
  existing `clearPath()` at :576 exists precisely to drop those).
  Re-running `startPath` per tick costs one `LOS` greedy walk of
  `<= 2 * PATROL_RADIUS` cells plus one small vector — bounded and
  below one chase search. Upgrade path only if profiling says
  otherwise: keep the path alive and key the recompute to
  `m_pathTimeout` instead.
- **`Path::LOS`**: greedy O(length) router, no open list, no
  backtracking; a post behind a wall yields a `Blocked` first step and
  a false return → wall-hug fallback that tick, post rotates every
  `PATROL_LEG_TICKS`. Never use `def.path` here: that is the chase
  selector and may be `ASTAR`.
- **`ttl = -1` (`NoTTL`)**: same as the chase `startPath`
  (`game_ai.cpp:572`). Monsters are not TTL-killed (that is
  `handleBullet` only).
- **Standing on the post**: `CPath::followPath`'s on-goal
  short-circuit returns `NoValidPath` → `startPath` false → the
  wall-hug tail wanders off the post until the next phase. Idle
  guards-on-post is a feature we are not paying state for.
- **Player re-enters range**: the range check at :549-551 runs before
  this block, so the chase branch takes over unconditionally; the
  per-tick `clearPath()` + `startPath` cycle leaves no stale patrol
  path to hand off.

Cost bound per out-of-range flagged patroller per action-tick: one
`LOS` walk (`<= 12` edge probes) + `<= 4` probes when the leg fails;
flag-off monsters pay exactly today's cost.

## 4. Files touched

| File | Change |
|---|---|
| `src/sprtypes.h` | `AI_PATROL = 0x10` alias line + comment |
| `src/game_ai.cpp` | `PATROL_RADIUS`/`PATROL_LEG_TICKS` consts, `patrolPost()`, the 4-line `AI_PATROL` supplement in `handleMonsterV3` |
| `tests/unit/t_game_ai.cpp` | cases in §5 |

No changes to: `actor.*` (no members), `ai_path.*` (no algo),
`bossdata.h`/`names.cpp` (no enum), map/layer serialization, and
**nothing inside the wander tail**.

## 5. Tests (`tests/unit/t_game_ai.cpp`, existing harness:
`resetWorld` / `expectPos` / `manageMonsters(ticks)`)

1. **Flagless regression (golden)**: `TYPE_MONSTERV3`, `ai = 0`, out of
   range in the existing fixture: record the current (pre-change)
   position sequence into a hard-coded table and assert it stays
   identical over the same tick series. The supplement must not move
   unflagged monsters.
2. **Supplement-not-replace proof**: same walled fixture, monster at
   patrol-unreachable origin (posts clamped into surrounding walls)
   with `ai = AI_PATROL`: assert the position sequence **equals the
   flag-off golden from case 1** — when legs never step, behavior is
   pure wall-hug.
3. **Patrol engages**: open field, `ai = AI_PATROL`, origin center;
   run `manageMonsters` over `>= 4 * PATROL_LEG_TICKS` ticks; assert
   every visited cell is Chebyshev `<= PATROL_RADIUS + 2` from origin
   and the actor moved (distinct positions > 3) and differs from the
   flag-off sequence (patrol, not the hug, is driving).
4. **Edge origin / bounds safety**: origin `(0,0)` boxed fixture, 300
   ticks: no crash, always in bounds, player's stamped tile intact
   (patrol `LOS` never steps onto the player cell — mirror of the
   existing "refuses player cell" asserts).
5. **Determinism**: run case 3 twice in fresh worlds; identical
   sequences (pure-function check — there is no RNG to seed).
6. **Version guard**: `t_game`'s mirrored `EXPECTED_VERSION` must
   **not** change — this plan adds no serialized field. If it drifts,
   the plan was not followed.

## 6. Verification loop (AGENTS.md)

1. `cmake -B build && cmake --build build` — pedantic-clean
   g++/clang/msvc/mingw.
2. `ctest --test-dir build` — minimum `t_game_ai`, `t_game`, `t_actor`.
3. Only after green: `python itools/gen_compliance.py`; add
   `docs/spec/gamespec/patrol-ai.md` (supplement semantics, flag
   value, bounds contract) and the checkpoint row per the
   documentation rules.
4. Move this plan to `docs/plans/archive/`; flip Task 4 in
   `ai-enemy-movement.md` to done-by-reference.
