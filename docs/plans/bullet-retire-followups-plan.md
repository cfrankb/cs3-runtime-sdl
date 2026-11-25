# Plan: follow-ups to the `bullet` keyword retirement

Date: 2026-11-25
Status: proposed — all items are deferred work from
`docs/plans/archive/skill-bullet-payload-plan.md` (§6 out-of-scope + the
retirement §8). None block current behavior; nothing here is load-bearing
until a driver appears.

## F1 — `damage=` per skill slot

The retirement gave each `SKILL_BULLET` slot its own payload
(`bulletSpec_t { tile, algo, sound, ttl }`) but not its own damage: bullet
damage still resolves from `bossData_t::damage` / `damage_special1`
scalars, so a boss with two BULLET slots cannot deal different damage per
projectile. Same argument applies to two `SKILL_SPECIAL1` slots sharing
`damage_special1`.

Sketch (smaller than it looks — the payload plumbing from the retirement
already exists):

- `bulletSpec_t` gains `uint8_t damage;` — but the bullet actor is spawned
  by `spawnBullet` and damages the player through the actor's tile damage
  (`TileDef::damage`), not through a per-actor field. Blocking question:
  bullets carry no per-actor damage channel, so this item needs either a
  damage override on the bullet actor or a decision to keep tile-damage
  semantics and drop F1. Investigate `spawnBullet` → actor → hit path
  before writing any code.
- Parser: `damage=` key in the skill loop, BULLET-only (reuse the
  non-BULLET-key `LOGW` branch), default 0 = "use boss scalar damage" so
  existing ini data is untouched.
- SPECIAL1 side: `handleBossHitboxContact` already takes the slot; swap
  `damage_special1` read for `slot.damage > 0 ? slot.damage :
  bd->damage_special1`.
- `bossSkill_t` is ini-loaded data, never serialized: no
  `ENGINE_VERSION` bump (same ruling as the retirement).

Test pins: `[Multi]` fixture slot gains `damage=`; pin the dealt-delta
value in a `testBossSkillCast`-style scenario, plus a slot-less-defaults
pin. Docs: `docs/spec/gamespec/boss-skills.md` §1/§4 + recipe §1 table.

Drive this when a boss actually needs two different bullet damages.

## F2 — asset-tree ini sweep for the retired keyword

`data/bosses/bosses.ini` (untracked dev tree) is migrated. Any other
ini copies still carrying `bullet ...` lines silently lose that boss's
fire (the keyword now warns as unknown). Known copies:

- `docs/spec/bosses/bosses.ini` — intentionally NOT migrated: it is the
  spec-mirror harness's fixture (`parse.py` / `bossdata_runtime.cpp` are
  independent implementations that still implement the keyword) and its
  tests (`docs/spec/bosses/tests/main.cpp` checks `demon.bullet ==
  TILES_FIREBALL`) assert legacy semantics. Migrating it means porting
  the mirror harness to the slot dialect first. Decide: does the spec
  mirror follow the runtime dialect (port parser + fixture + its tests
  together) or is it frozen as a legacy-format oracle?
- `private/` trees outside this repo (maintainer-owned): the migration
  table in the archived plan §8 is the recipe.

No runtime-code change; only touch this when the mirror harness is
scheduled for an update anyway.

## F3 — loader-side flag mirror for SPECIAL1 proximity

The retirement gated `BOSS_FLAG_PROXIMITY_ATTACK` lunges on the
`SKILL_SPECIAL1` slot's rate at runtime; declaring the slot effectively
implies the flag's behavior. The flag bit itself is NOT set at load, so
`getBossData` consumers (tools, the spec mirror, any future animation or
UI code reading `flags`) do not see the implication.

Only build this when a second consumer of the flag appears: in
`BossAccum::build()`, after the skill loop, `if (special1Slot)
d.flags |= BOSS_FLAG_PROXIMITY_ATTACK;` — and note it would then be
indistinguishable from an authored flag when dumped. Deferred per
ponytail: one consumer, gate where the data is.

## F4 — `bossSkill_t::bullet` surface guard

`bulletSpec_t` lives inside `bossSkill_t` (not a protected struct, so the
AGENTS serialized-state rule does not catch it), but the retired keyword
means it is now the *only* bullet payload surface. If `CFrame`-style
"keep compliance documentation current" tracking is ever extended to
bossdata, record the field list (tile/algo/sound/ttl) and the parser's
key set there so accidental surface changes get caught like the frame
contract. Cheap version: a `docs/shared-surface.md` row for
`bulletSpec_t` / skill ini keys. Do alongside the next bossdata change,
not as its own commit.

## Verification (when any item lands)

```sh
python bin/refresh.py   # only if new test files were added
cmake -B build && cmake --build build
ctest --test-dir build -R "bossdata|game_ai|fx"
```

Spec/compliance hash updates per AGENTS.md after tests pass. Completed
items get struck here with a link to their plan/archive entry; move this
file to `docs/plans/archive/` when all are resolved or rejected.
