# Plan: merge the `bullet` ini line into `skill BULLET` (per-slot bullet payload)

Date: 2026-11-25
Status: implemented — all three milestones landed 2026-11-25; the legacy
`bullet` keyword was retired the same day (§8 below supersedes D1/D3).

Progress:
- **Milestone 1 (done 2026-11-25):** §1 `bulletSpec_t` + slot payload,
  §2 parser keys + D2 `bullet_rate` mirror (recommendation taken; D3
  inert-slot taken), §4 parser pins in `test_bossdata_skill_ini`
  (incl. `[Two]` independent payloads and `[Inert]`). Runtime untouched;
  full `ctest` 112/112.
- **Milestone 2 (done 2026-11-25):** §3 `handleBossBullet(boss, spec)` +
  cast call site, §5 two-slot payload pin in `testBossSkillCast` (both
  payload tiles seen within 200 ticks; `spawnBullet` refuses a shared tile
  in one tick, so same-tick co-spawn is not observable).
- **Milestone 3 (done 2026-11-25, maintainer rulings):** the `bullet` ini
  keyword and the `bossData_t::bullet*` fields are retired (D1 moot — there
  is no legacy line left to coexist with). `tile=` is mandatory and
  non-blank for BULLET and `rate > 0`, else the slot is skipped with `LOGE`
  (supersedes D3's inert-slot). `BOSS_FLAG_PROXIMITY_ATTACK` is now gated
  by the `SKILL_SPECIAL1` slot rate, never by bullet data, so the derived
  `bullet_rate` field was deleted; the two `bullet TILES_BLANK rate=5`
  lines in the data tree were dropped, not converted. Docs:
  `docs/spec/gamespec/boss-skills.md` §1–§3/§6,
  `docs/briefs/boss-skill-recipe.md` §1/§4. Full `ctest` 112/112.

## Problem

A boss's bullet attack is split across two ini dialects:

```ini
bullet          TILES_FIREBALL rate=15 algo=Path::NONE sound=SOUND_SHOOT3 ttl=NoTTL
skill           BULLET rate=15
```

the legacy line owns the payload (`bossData_t::bullet/bullet_algo/
bullet_sound/bullet_ttl`, `src/bossdata.h:179-183`), while the `skill
BULLET` slot owns only `{skill, rate}` (`bossSkill_t`, `bossdata.h:159-163`)
and, once declared, suppresses the legacy rate gate
(`hasBulletSlot` yield, `src/game_ai.cpp:357-372`) and re-fires the *same*
payload through the skill loop (`case SKILL_BULLET`, `game_ai.cpp:384-386`).

Consequences:

- The recipe demands redundancy: "Keep the old `bullet ... rate=` fields
  filled — the slot reuses them" (`docs/briefs/boss-skill-recipe.md` §4).
- A second bullet attack on the same boss is impossible via slots: all
  `SKILL_BULLET` slots share the one `bossData_t` payload store, so two
  slots just double-fire one bullet. A variant attack must be faked as an
  fx skill.

## Proposal

Per-slot bullet payload. The `skill BULLET` line absorbs the legacy keys
and becomes self-contained; multiple `BULLET` slots per boss then describe
independent attacks (own tile/algo/sound/ttl, own rng `rate` gate):

```ini
skill           BULLET rate=15 tile=TILES_FIREBALL algo=Path::NONE sound=SOUND_SHOOT3 ttl=NoTTL
skill           BULLET rate=40 tile=TILES_BULLETY1 algo=Path::LOS sound=SOUND_NONE ttl=NoTTL
```

The legacy `bullet` line stays parsed and functional (compat layer, AGENTS
rule — never remove). Bosses with no `skill` lines keep byte-identical
legacy behavior; do not migrate them as a side effect.

Model ruling implied: the payload moves **into the slot**; `bossData_t::
bullet*` remains the storage for the legacy line only.

## Versioning ruling

`bossSkill_t` growing payload fields is a `bossData_t`-embedded struct
change. It is **not** an `ENGINE_VERSION` trigger: the protected list is
`CActor`/`CMap`/`CLayer` only (AGENTS.md), and `bossData_t` + skill slots
are ini-parsed at startup, never serialized (`bossdata.h:112-116` "never
serialized"; pending casts are transient per `boss-skill-recipe.md` §3).
No savegame/recorder format is touched.

## §1 Payload struct (`src/bossdata.h`)

New aggregate beside `bossSkill_t`:

```cpp
// bullet attack payload; one owner per source (ini skill slot or the
// legacy bossData_t bullet* fields). Built at load, read-only at runtime.
struct bulletSpec_t
{
    uint8_t tile = 0;         // TILES_BLANK = never fires
    uint8_t algo = 0;         // BossData::Path value
    uint8_t sound = 0;        // firing sound
    int ttl = 0;              // BossData::NoTTL or tick count
};
```

`bossSkill_t` gains `bulletSpec_t bullet;` next to `{skill, rate}`.
`skills[4]` array size unchanged; the per-boss slot cap already enforces
the bound (`bossdata.cpp:768-773`).

## §2 Parser (`src/bossdata.cpp`, `BossAccum::build()` skill loop)

In the `skill` entry loop (`bossdata.cpp:746-784`), after `resolveSkill`,
when `g_skillDefs[idx].kind == SKILL_BULLET`, extend the existing kv scan
(currently only `rate=`, L774-780) to accept:

| key | resolution | target |
|-----|-----------|--------|
| `tile=` | `resolveTile` | `slot.bullet.tile` |
| `algo=` | `resolvePath` | `slot.bullet.algo` |
| `sound=` | `resolveSound` | `slot.bullet.sound` |
| `ttl=` | `"NoTTL"` → `BossData::NoTTL`, else `toInt` | `slot.bullet.ttl` |

Rules:

- Unknown keys on a skill line: `LOGW` + ignore (mirrors the composite
  line's tolerance).
- A payload key on a *non*-BULLET skill line: `LOGW` + ignore.
- Defaults when a key is absent: `tile = TILES_BLANK` + `LOGW` (a BULLET
  slot that can never fire is a data error, cf. the aims-garbage
  convention, `bossdata.cpp:676`); algo/sound/ttl default to
  `Path::NONE` / `SOUND_NONE` / `NoTTL` like the legacy line without
  those keys.
- The `bullet` composite branch (`bossdata.cpp:897-922`) and the
  `build()` legacy field fills (`bossdata.cpp:652-659`) are **untouched**.
- D1: whether a legacy `bullet` line *and* a BULLET slot coexisting is
  allowed, or `LOGW` "duplicate bullet attack". Recommended: allow — the
  `hasBulletSlot` yield already prevents double-fire, and D2's proximity
  mirror may still read the legacy rate.

## §3 Runtime (`src/game_ai.cpp`, `src/game.h`)

- `handleBossBullet(CBoss &)` (L173-207) takes the payload: signature
  becomes `handleBossBullet(CBoss &boss, const bulletSpec_t &spec)` and
  the four `boss.data()->bullet*` reads (L180, L201, L204-206) become
  `spec.*` reads. Update the declaration in `src/game.h`.
- Cast site `case SKILL_BULLET` (L384-386): pass `slot.bullet`. Each
  BULLET slot stays independently rng-gated by its own `slot.rate` in
  the existing loop (L376-381) — two slots *can* fire in the same tick;
  that is the point of the feature (see §5 pin).
- Legacy gate (L363-372): unchanged condition; at the call build a local
  `bulletSpec_t{bd->bullet, bd->bullet_algo, bd->bullet_sound,
  bd->bullet_ttl}` and pass it. Byte-identical legacy behavior.
- D2 — proximity-attack rate (`game_ai.cpp:405-408`): the
  `BOSS_FLAG_PROXIMITY_ATTACK` contact gate reads `bd->bullet_rate`
  *unconditionally*, including for skill-declared bosses. A boss written
  only with merged `skill BULLET` lines has `bullet_rate == 0`, and
  `rng.range(0, 0) == 0` is always true → contact attack every tick.
  Options:
  - **a (recommended):** mirror at load — first BULLET slot's `rate`
    fills `d.bullet_rate` in `build()` when the legacy key is absent;
    keeps proximity behavior sane, one-line diff, legacy line wins when
    present.
  - b: gate the proximity branch additionally on the legacy `bullet`
    line having been used — changes behavior for a boss that has the
    flag but only skill lines; needs its own compat audit.
- The legacy `bossData_t::bullet*` fields and the `hasBulletSlot` yield
  (L357-372) stay as-is; the yield only matters for legacy+skill mixed
  files and is itself compat surface.

## §4 Tests (`tests/unit/`)

- Extend `test_bossdata_skill_ini` (`t_bossdata.cpp:507-586`) fixture:
  - `[Multi]` first `skill BULLET rate=10` gains
    `tile=TILES_FIREBALL algo=Path::A* sound=SOUND_SHOOT3 ttl=20`; assert
    `skills[0].bullet.*` round-trips, slot order/cap/unknown-token pins
    unchanged.
  - New second BULLET slot with a different tile/rate; assert independent
    payloads and that the 5-slot cap still applies to BULLET pairs.
  - `[None]` legacy-only boss: existing L577 pin (`bullet_rate 15`,
    `bullet != 0`) must stay green untouched.
  - Absent `tile=` on a BULLET slot: slot kept, `tile == TILES_BLANK`
    (warning path asserted by absence of crash only).
- `t_game_ai.cpp`: a determinism pin that a two-BULLET-slot boss can
  spawn two *different* tiles across ticks, and a single-BULLET-slot
  boss fires byte-identically to the pre-change legacy path (reuse the
  `testHiddenBossBlast`-style harness). Predicate at L913 reads
  `d.bullet`/`d.bullet_rate` — legacy fields, unaffected.
- Run: `cmake -B build && cmake --build build && ctest --test-dir build
  -R "bossdata|game_ai|fx"`.

## §5 Behavior decisions (pin in tests)

- **Same-tick double-fire:** two BULLET slots are independently gated and
  may both cast on one tick. Accepted as the feature; one test pins the
  two-different-tiles outcome so a future "fix" is a deliberate change.
- **Tick determinism:** unchanged for single-slot and legacy bosses —
  the rng draws in the skill loop already run per slot per tick.

## §6 Out of scope

- `docs/spec/bosses/` mirror (`parse.py`, `bossdata_runtime.cpp`,
  `tests/main.cpp`, `bosses.ini`): the spec harness has **no** skill
  parser at all; the legacy `bullet` composite path there is untouched
  and its ini stays legacy-syntax. Mirroring the skill subsystem into
  the spec harness is a separate project.
- Migrating the private asset-tree `bosses.ini` files: optional; the
  legacy line remains valid. Migrate one demo boss at most, in its own
  commit.
- `damage=` per BULLET slot: bullets deal damage as actors via the tile
  definitions, not the skill row — no damage key here.

## §7 Documentation

- `docs/briefs/boss-skill-recipe.md` §4: replace "Keep the old `bullet
  ... rate=` fields filled — the slot reuses them" with the self-contained
  slot syntax; note the legacy line still parses.
- `docs/spec/gamespec/boss-skills.md`: `SKILL_BULLET` payload description
  moves from `bossData_t` to the slot.
- `docs/compliance/`: flip affected open items, update the module SHA256
  and tally/checkpoint rows *after* §4 tests pass (AGENTS.md: testing
  proves correctness; the hash merely detects change).

## Open decisions

| # | Question | Recommendation |
|---|----------|----------------|
| D1 | Legacy `bullet` line + BULLET slot on one boss: allowed or warned? | Closed: moot — line retired (§8) |
| D2 | Proximity gate `bullet_rate == 0` hazard (§3) | Closed: field deleted; gate reads the SPECIAL1 slot rate (§8) |
| D3 | Missing `tile=` on a BULLET slot: error-skip slot or keep inert? | Closed: maintainer ruling overrode the recommendation — `LOGE` + skip |

## §8 Retirement (maintainer ruling, 2026-11-25)

The `bullet` ini keyword was removed outright once slots could carry the
payload: it is no longer in the composite-key list (old lines now warn as
unknown fields), and `bossData_t::bullet`, `bullet_rate`, `bullet_algo`,
`bullet_sound` and `bullet_ttl` are gone from the struct. That struct is
loaded ini data, never serialized, so no `ENGINE_VERSION` bump applies.

`BOSS_FLAG_PROXIMITY_ATTACK` never described a projectile — it drives the
attack sequence holding the SPECIAL1 hitboxes — so its rng gate now reads
the first `SKILL_SPECIAL1` slot's rate. A flagged boss with no SPECIAL1
slot simply never lunges instead of firing every tick.

`TILES_BLANK` is not a bullet, so a blank `tile=` is a load error rather
than an inert rate carrier; the two data-tree lines that existed only to
hold a rate were deleted. Data migration for asset trees:

| legacy | new |
|--------|-----|
| `bullet T rate=N algo=A sound=S ttl=T` | `skill BULLET rate=N tile=T algo=A sound=S ttl=T` |
| `bullet TILES_BLANK rate=N ...` | dropped |

## Verification

```sh
python bin/refresh.py   # only if new test files were added
cmake -B build && cmake --build build
ctest --test-dir build -R "bossdata|game_ai|fx"
```

Then §7 doc + compliance updates. On completion move this plan to
`docs/plans/archive/`.
