# Boss Skills (`bossSkillDef_t` registry) Specification

Status: **Implemented** (2026-11-21, plan
`docs/plans/archive/fx-boss-skills-plan.md`,
steps 1-6; telegraph art deferred to
[`docs/plans/telegraph-art-task.md`](../plans/telegraph-art-task.md)).

Extract: a named skill is a registry row; a boss declares up to four
`skill NAME rate=N` slots; the Chase-branch cast trigger rolls each slot
against its rate; `SKILL_FX` strikes damage via authored hitboxes.

## 1. Registry — `g_skillDefs` (`src/bossdata.h`)

Hand-authored `std::vector<bossSkillDef_t>`; rows are looked up by ini token
(`resolveSkill` scan by `name`) and by damaging effect (`skillForFx`, a
1:1 fx<->skill scan by `effectId`, `nullptr` when no skill owns the fx).

| field | meaning |
|-------|---------|
| `name` | ini token: `BULLET`, `SPECIAL1`, `LIGHTNING_STRIKE`, `METEOR` |
| `kind` | `SKILL_BULLET` / `SKILL_SPECIAL1` / `SKILL_FX` |
| `effectId` | `SKILL_FX`: damaging Fx id (index into `g_fxData`) |
| `telegraphFx` / `telegraphTtl` | warning Fx id, `-1` = none; warning lead (ticks) before the strike spawns, `0` = cast is instant |
| `damage` | hp dealt to player per collision registration |
| `baseFrame` | fx-sheet frameID of the hitbox run's first frame |
| `hitboxFile` | hitbox JSON filename under `<fxDir>/`; `nullptr` = no file-authored boxes |
| `default_box` | `true`: synthesize one box at load on the fx's **last** frame (`fxDataAt(effectId)` layer `base + count - 1`): bottom rect of the table extents (`w x 8` px, frame-bottom flush, center-anchor-relative), appended to any file boxes |
| `frames` | sparse per-frame boxes (`sprite_hitbox_t`, keys `baseFrame + rawId`, box `x/y` px offsets from the fx center anchor); absent frames never damage |
| `firstHitFrame` / `lastHitFrame` | min/max keys, derived at load |

`BULLET` and `SPECIAL1` rows are kind markers only: their payload and attack
code stay in `bossData_t` (the existing `bullet*` fields and
`damage_special1`). The registry is a static ini/JSON-loaded table, **not**
serialized state: no `ENGINE_VERSION` coupling.

## 2. Boss slots — `bossData_t::skills` (`src/bossdata.h`)

`bossSkill_t { uint8_t skill /* g_skillDefs index */; uint8_t rate; }`,
filled from repeated ini lines `skill NAME rate=N` into `skills[0..skillCount)`
in file order. Cap 4: a 5th line `LOGW` + ignored. Unknown token: `LOGE` +
entry skipped, counter not advanced. `rate` is the rng gate bound
(`rng.range(0, rate) == 0` casts; `rate = 0` = declared, never fires —
`rng.range` degenerates at 0). Bosses without `skill` lines keep
`skillCount = 0` and legacy behavior byte-identically.

## 3. Cast trigger (`CGame::manageBosses`, Chase branch)

Per slot: `rng.range(0, rate) != 0` skips (rng seeded `setTick(ticks)` per
tick, same determinism as the legacy `bullet_rate` gate; `distance_pursuit`
already holds in Chase).

- `SKILL_BULLET` -> `handleBossBullet(boss)` unchanged. While a `SKILL_BULLET`
  slot is declared the legacy fire gate **yields** (no double-fire); slot-less
  bosses fire exactly as before.
- `SKILL_SPECIAL1` -> no cast spawn; the slot instead rate-gates the
  contact hit-test (§4).
- `SKILL_FX` -> warn-first cast: with `telegraphTtl > 0`,
  `scheduleFx(effectId, player.x(), player.y(), telegraphTtl)` schedules the
  strike against the tile the player stands on; it spawns there
  `telegraphTtl` ticks later in `tickFx` (authored rows: 6 ticks = 1/4 s at
  the 24 Hz clock). Until then both renderers tint the target tile 25% red —
  a placeholder for the missing marker art. `telegraphTtl == 0` = no
  warning, immediate `spawnFx` (legacy behavior). `telegraphFx >= 0`
  additionally spawns the marker fx immediately (same tile, same tick);
  `playSound(attack_sound)` fires with the hint. Pending strikes are
  transient CGame state: **not** serialized; save/load drops them and the
  boss simply recasts.

## 4. Damage

- **Fx collision** (`CGame::tickFx`, extended-fx 1.4 consumer): for each live
  fx with `skillForFx(effectID)`, the current frame (`fxFrameIndex`, front
  layer preferred) is looked up in `frames` (a miss — sparse authoring — is
  the common case); each box at `fx.x + box.x, fx.y + box.y` AABB-tests the
  player's 16px tile rect; overlap deals `addHealth(-damage)`. Repeat
  registrations throttle on the `addHealth` invincibility window (like boss
  contact); no per-instance hit bookkeeping, no `fx_t` growth, no wire change.
  **Box authoring is the damage window** (absent lead-in frames = telegraph
  time).
- **SPECIAL1 contact** (`CGame::handleBossHitboxContact(boss, ticks)`): while
  the boss declares a `SKILL_SPECIAL1` slot, `SPECIAL1`-type contact hits land
  only on ticks passing `rng.range(0, rate)` (rng re-seeded `setTick(ticks)`);
  `MAIN`/`ATTACK`/`SPECIAL2` hits and slot-less bosses are untouched.

## 5. Hitbox data — `data/fx/<NAME>.json`

Same §0b grid schema as boss per-aim JSONs (`readHitboxJson` shared core;
`{ "frame": { cols, width, height }, "hitboxes": [ { x, y, w, h, type } ] }`,
frame derived by `col = x / width`, `rawId = row * cols + col`, `type: 0`
dropped). Fx differences: keyed by plain fx-sheet frameID `baseFrame + rawId`
(single fx sheet, no `SHEET_SPACER`), box coords converted to px relative to
the frame center anchor (`off - frameW/2`), no half-tile quantization.
Damaging boxes are authored `type: 2` (SPECIAL1) — shared vocabulary, not a
shared consumer. The JSON filename is declared per row via
`bossSkillDef_t::hitboxFile` (under `<fxDir>/`); a skill with no hitboxes
declares `nullptr` **and** `default_box = false` — `loadFxSkills` skips it
silently. A declared file that fails to read is a `LOGW` + its boxes are
skipped. Rows declaring neither source keep no boxes and never damage.
`loadFxSkills(fxDir)` runs at startup beside `loadBossData`.

## 6. Compatibility

- The reserved `fxData_t::damage/splashRadius/hitboxes/hitboxCount` fields
  (extended-fx 1.4) stay declared/unused; the consumer above binds the
  registry's `frames`, not the table's.
- Legacy `bullet*` ini fields and the legacy fire gate are untouched for
  slot-less bosses.

## 7. Tests

`t_bossdata` (`test_bossdata_skill_ini`, `test_bossdata_fxskills`,
`test_bossdata_fx_data`), `t_fx` (`test_fx_align`, `test_fx_skill_hit`),
`t_game_ai` (`testBossSkillCast`).
