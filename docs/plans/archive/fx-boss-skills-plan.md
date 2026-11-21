# Plan — SKILL concept: fx hitbox damage + boss skills (lightning strike, meteor)

Status: **PROPOSED** (v10, 2026-11-21). Open questions resolved by the
maintainer (§8 question log). Not started.

Introduces **SKILL** as a first-class gameplay concept: a named static
entry linking **Fx (visual) + per-frame hitboxes + damage**, castable by
bosses through a **skill type + use frequency** pair in `bosses.ini`.
Binds the reserved fx damage provisions (spec
`docs/spec/gamespec/extended-fx.md` §1.4) to the player through that
machinery. Two launch skills: **lightning strike** (`FX_BLUE_LIGHTNING`)
and **meteor** (`FX_FIRE_COLUMN`, maintainer-designated analogue).

## 0. Ground (current state, verified 2026-11-20)

- `fx_t` instances spawn via `CGame::spawnFx` / `spawnFxPx`
  (`src/game.cpp:1907,1927`), tick in `CGame::tickFx()`
  (`src/game.cpp:1946`), raw-serialized 12-byte POD, origin tile in
  `ox/oy`. Frame index derives from `timer` (spec I4).
- `fxData_t.damage`, `splashRadius`, `hitboxes` declared, unused
  (`src/gamesfx.h:146-149`); they stay reserved/unused — the skill table
  (§1) owns damage and hitboxes instead.
- Player-hurt single entry point: `CGame::addHealth(-n)`
  (`src/game.cpp:1023`); its `S_PLAYER_HURT` invincibility-window guard
  (`src/game.cpp:1034-1037`) is the repo-wide repeat-hit throttle — boss
  contact damage (`handleBossHitboxContact`, `src/game_ai.cpp:209`)
  already hit-tests unthrottled every tick and relies on it.
- Boss AI: `CGame::manageBosses()` (`src/game_ai.cpp:280`); rng is
  tick-deterministic (`rng.setTick(ticks)`), rate-gate pattern
  `rng.range(0, rate) == 0` (`src/game_ai.cpp:331`).
- `bossData_t` (`src/bossdata.h:110`) ini-loaded static table, never
  serialized; loader `BossAccum::build()` (`src/bossdata.cpp:550`) maps
  `name value key=value` lines to `fields["name"]` / `fields["name_key"]`
  and resolves tokens (`resolveSound`, `resolvePath`, `resolveTile`).
- Hitbox JSON loader: `readHitboxFile` (`src/bossdata.cpp:440-539`),
  format §0b.
- Assets (`src/sfx_private.h`, `data/pixels/sfx.idx`, 77 sequences):
  `FX_BLUE_LIGHTNING` (6×32×48), `FX_FIRE_COLUMN` (8×32×48),
  `FX_MUSHROOM_EXPLOSION`, `FX_ICE_SPIKES`, ... **no telegraph /
  ground-marker art exists yet** (§5). `data/fx/` does not exist yet.

## 0b. Hitbox data convention (maintainer clarification, 2026-11-20)

Per-frame hitboxes travel in JSON files **grouped by frame strip**, the
schema the boss loader already consumes (`readHitboxFile`; example:
`cs3-map-edit/src/data/bosses/data/beholder1_attacks-trim.json`):

```json
{ "frame": { "cols": 8, "width": 64, "height": 64 },
  "hitboxes": [ { "x": 296, "y": 232, "w": 24, "h": 24, "type": 2 }, ... ] }
```

- Box coords are absolute within the file's grid; the frame each box
  belongs to is **derived** ("additional processing"):
  `col = x / frame.width`, `row = y / frame.height`,
  `raw_id = row * cols + col` — **one row = one frame strip** (Beholder
  `aims 4`: 4 strips). The box's offset inside its frame cell is
  `x - col * frame.width`, `y - row * frame.height`; the box walks
  across the strip as the animation plays.
- `type` mirrors `BossData::HitBoxType` (`MAIN=0, ATTACK=1, SPECIAL1=2,
  SPECIAL2=3`); loader drops `type: 0` (ignored marker). Fx skill JSONs
  author their damaging boxes as **`SPECIAL1` (type 2)** — maintainer
  ruling for `LIGHTNING_STRIKE` and `METEOR`. The type is shared
  *vocabulary*, not a shared consumer: SPECIAL1 boxes in a boss's
  per-aim JSONs feed the boss contact path (§1 SKILL_SPECIAL1), in
  `data/fx/` JSONs they feed the fx skill path (§4).
- Fx skill hitbox JSONs use the same format, authored in cs3-map-edit,
  live in **`data/fx/`** (new dir, one file per skill), their grid
  covering the effect's frame run on the fx sheet.

## 1. New concept: `bossSkillDef_t` — the skill registry

`SKILL` is the unit that links attack **visual + hit resolution + damage**
for one castable attack. Skill entries are **generic kinds**, never
per-boss or per-ammo:

- **`SKILL_BULLET`** — the general bullet attack. The registry entry is
  just the kind marker; the *actual bullet information (tile, algo,
  sound, ttl) still resides in `bossData_t`* (the existing `bullet*`
  fields, `src/bossdata.h:124-128`), and casting calls the existing
  attack code `handleBossBullet(boss)` **unchanged**
  (`src/game_ai.cpp:168` — it reads the boss's own fields). This reuses
  the existing attack code outright; nothing new travels, aims or hits.
- **`SKILL_SPECIAL1`** — the boss's SPECIAL1 sprite-box attack, as a
  skill (maintainer ruling; **Beholder's primary skill**). Again pure
  reuse: the actual information already resides in `bossData_t` (the
  authored per-aim hitbox JSONs with `type: 2` boxes +
  `damage_special1`), and the existing attack code is the contact
  hit-test `handleBossHitboxContact` → `boss.testHitbox1` →
  `boss.damage(r.type)` (`src/game_ai.cpp:209-226`) — no spawn, no
  cast-loop dispatch; declaring the slot instead **parameterizes** that
  hit-test: while declared, SPECIAL1-type contact hits only land on
  ticks passing `rng.range(0, rate)` (tick-deterministic rng re-seeded
  in-handler). Boss without the slot: byte-identical today's behavior.
- **`SKILL_FX`** — fx skills (lightning strike, meteor): the registry
  entry owns fx id + hitboxes + damage, since no per-boss bullet-style
  data exists for them.

Declared in `src/bossdata.h` (it's boss-cast data; `tickFx` in
`game.cpp` includes it fine), storage + loader in `src/bossdata.cpp` /
`src/gamesfx.cpp`:

```cpp
enum SkillKind : uint8_t { SKILL_BULLET, SKILL_SPECIAL1, SKILL_FX };
struct bossSkillDef_t
{
    const char *name;    // ini token: "BULLET", "LIGHTNING_STRIKE", "METEOR"
    SkillKind kind;
    // --- SKILL_BULLET: no payload — bullet data lives in bossData_t ---
    // --- SKILL_FX ---
    int16_t effectId;    // damaging Fx id (index into g_fxData)
    int16_t telegraphFx; // warning Fx id, -1 = none (§5)
    uint8_t telegraphTtl;// warning lifetime (ticks) = lead-in window
    int16_t damage;      // hp dealt to player per collision registration
    int16_t baseFrame;   // fx-sheet frameID of the hitbox run's first frame
    // SPARSE per-frame boxes (v10): most frames have NO hitboxes; the
    // few that do hold up to MAX_HITBOX_PER_FRAME (4). Only frames with
    // >= 1 box get a sprite_hitbox_t record — frames absent from the
    // vector never damage and never draw the lead marker. Same shape as
    // the existing sparse g_hitboxes store + getHitboxes() lookup
    // (src/bossdata.h:103-170).
    std::vector<sprite_hitbox_t> frames;      // keyed by fx-sheet frameID
    int16_t firstHitFrame = -1;               // min key, derived at load:
    int16_t lastHitFrame = -1;                //   lead-in + damage-window bounds
};
extern std::vector<bossSkillDef_t> g_skillDefs; // hand-authored table (§3)
const bossSkillDef_t *skillForFx(int effectID); // registry scan by effectId
```

- FX skill ↔ fx is **1:1** (`ponytail: scan-by-effectId assumes one
  skill per fx id; if two skills ever share an fx, move the skill index
  onto the fx_t instance — a wire change + ENGINE_VERSION bump`).
- Built at load time: hand-authored rows give name/kind/payload/damage;
  the §3 JSON loader fills FX `frames` from `data/fx/<skill>.json`.
  `fxData_t::damage/hitboxes` stay reserved and unused (do not remove,
  AGENTS compat rule).
- `fx_hitbox_t` (`src/gamesfx.h:124`) finally gets its consumer: box
  `dx/dy/w/h` in **px relative to the frame center anchor**, converted at
  load: `dx = offsetX - frameWidth / 2` (offsetX per §0b), no half-tile
  quantization (fx are pixel-space).

## 2. New data structure: boss skill slots (type + frequency)

```cpp
struct bossSkill_t
{
    uint8_t skill; // index into g_skillDefs (generic kind entry)
    uint8_t rate;  // use frequency: rng gate 0..rate (1 = often)
};
// bossData_t gains (max 4 skill slots per boss):
bossSkill_t skills[4];
uint8_t     skillCount = 0; // used-slot counter; skills[0..skillCount)
```

`bosses.ini` — **repeated unnumbered `skill` lines** (linkage; parser §3):

```ini
skill    BULLET              rate=10
skill    SPECIAL1            rate=30   # e.g. Beholder's primary skill
skill    LIGHTNING_STRIKE    rate=30
skill    METEOR              rate=45
```

- `skill` selects the **type** (generic registry entry); `rate` the **use
  frequency**, same semantics/tick-determinism as `bullet_rate`.
  `skillCount` tracks how many of the 4 slots are used; 0 → boss casts
  nothing new. No new `BOSS_FLAG_*` bits.
- **Backward compatibility (AGENTS rule):** the legacy
  `bullet TILES_FIREBALL rate=… algo=… sound=… ttl=…` line and the
  `bossData_t::bullet*` fields stay exactly as they are — they **are**
  the BULLET skill's data (maintainer ruling v4). The legacy fire gate
  (`src/game_ai.cpp:327-335`) keeps serving bosses without skill slots
  unchanged; it only yields when the boss declares a `SKILL_BULLET`
  slot (§7), which prevents double-firing.
- `damage_special1/2` keep their current job (sprite-box damage) and are
  not reused here — skill damage lives in the registry (§8 Q2).
- Static table growth only: no serialization, no `ENGINE_VERSION` bump.

## 3. Parser + loader updates

- `src/bossdata.cpp`: the tokenizer's `seq.fields` is a flat string map
  (repeated keys overwrite), so add a `skill` branch beside `hitbox`/
  `flags` that **accumulates**: each `skill NAME rate=N` line appends
  `joinSpace(e, 1)` to `fields["skill"]` separated by `;`. `build()`
  splits on `;` and fills `bossData_t::skills` in file order; per entry
  `resolveSkill()` maps `BULLET` / `LIGHTNING_STRIKE` / `METEOR` →
  registry index (same style as `resolveSound`; tokens also registered
  in `src/names.cpp` for the name table), `rate=N` via the existing
  `key=value` convention; entries fill `skills[skillCount++]` **up to
  the 4-slot cap** — a 5th `skill` line → `LOGW` + ignored. Unknown
  skill token → `LOGE` + entry skipped (counter not advanced).
- Fx hitbox JSON load (fx side, `src/gamesfx.cpp` or a new
  `loadFxSkills(dir)` called from the same startup point as
  `loadBossData`): for each registry row with a JSON in `data/fx/`,
  parse via the shared §0b derivation (factor `readHitboxFile`'s core
  into a small shared helper both callers use), fill `frames`.
  Missing file → `LOGW`, skill keeps no hitboxes → never damages
  (skip-guard style for the absent private `data/` tree in CI).

Files: `src/bossdata.h`, `src/bossdata.cpp`, `src/gamesfx.h`,
`src/gamesfx.cpp`, `src/names.cpp`, startup (`src/engine_hw.cpp:220`
neighbourhood).

## 4. Phase A — collision: skill hitboxes damage the player

In `CGame::tickFx()`, per spec §1.4 "tick hit-test against player",
maintainer ruling: **damage occurs when the hitbox check registers a
collision** — no dedicated damage tick:

1. For each live fx with a skill (`skillForFx(fx.effectID)`, §1):
   look up the box set of the **current frame** (`fxFrameIndex`, I4 →
   registry `frames`; a **miss is the common case** — sparse §1 — then
   just skip), place each box at `fx.x + dx, fx.y + dy`, test
   against the player's 16px tile rect
   (`pos * FX_TILE_SIZE .. +FX_TILE_SIZE`); on overlap
   `addHealth(-skill->damage)`.
2. Repeat registrations while the fx lingers are throttled by the
   invincibility window inside `addHealth` — exactly how boss contact
   damage already behaves. No per-instance hit bookkeeping, no `fx_t`
   growth, **no wire change**.
3. Frames with no authored boxes never damage: hitbox authoring **is**
   the damage window (this is also the lead-in lever for §5).
4. Damaging boxes are authored as **SPECIAL1 (type 2)** (ruling:
   `LIGHTNING_STRIKE` and `METEOR`); `type 0` is already dropped by the
   loader, and the remaining types 1/3 are accepted too — one filter in
   the shared loader, no per-type damage logic.
5. Save/load: frame identity from `timer`, collisions from live state —
   restore needs nothing new.

Files: `src/game.cpp` (+ `src/gamesfx.*` for the lookup helper).

## 5. Visual hint (telegraph) for incoming strikes

Cast spawns **two fx instances on the same tile, same tick**:

1. the damaging skill fx (`skill->effectId`), and
2. if `telegraphFx != -1`: the warning fx with `FX_REPEATABLE`,
   `ttl = telegraphTtl` — a ground marker on the target tile.

Both are ordinary fx instances → both serialize, timers run in lockstep,
**zero new state**. The dodge/telegraph window is
`min(lead to first authored hitbox frame, telegraphTtl)`: author hitboxes
onto the later frames of the strike sequence and the first frames are
free warning even with no telegraph art.

- **Art gap → red-square placeholder (v9 ruling)**: until real hint art
  exists, the fx render pass (`drawViewPortInner`'s fx iteration, same
  machinery as the sugarFx tile-tint rects, `src/engine_hw.cpp:531`)
  draws a **red rect over the fx's tile** (`ox/oy`) while a skill fx is
  in its lead-in frames — i.e. frames before the first authored hitbox
  frame. Pure read-off: driven by the registry `frames` table + `timer`
  at draw time, so **zero new state**, save/restore-safe, expires with
  the animation, and needs no CGame→renderer channel. Delete this
  branch when `telegraphFx` art lands.
- **Art gap**: `sfx.idx` (77 runs) has no ground marker/reticle. Data
  TODO: add one (e.g. cast-circle from the same vfx pack's "Area Blasts
  & Ground Fields"), re-run the `sheet` generator → new `FxLayer` enum
  entries, fill `telegraphFx`/`telegraphTtl` on the registry rows.
  Until then `telegraphFx = -1` and the hitbox lead-in is the hint.
- Telegraph fx never carries hitboxes (not in the registry as a skill;
  registry is keyed by the damaging fx id).

## 6. Fx alignment (spawn-time anchor modes)

Two placement modes for tile spawns, table field on `fxData_t`:

```cpp
enum FxAlign : uint8_t { FX_ALIGN_CENTER, FX_ALIGN_BOTTOM };
uint8_t align; // per fx row
```

- `FX_ALIGN_CENTER` (today's behavior, explosions/auras/smoke):
  anchor at tile center, `x = tx*T + T/2`, `y = ty*T + T/2`.
- `FX_ALIGN_BOTTOM` (tall skyfalls: lightning, fire column, ice spikes):
  **centered horizontally on the tile, frame bottom flush with the tile
  bottom**: `x = tx*T + T/2`, `y = (ty + 1)*T - h/2` (`h` = table max
  frame height).
- `CGame::spawnFx` consults `align` at the tile→px conversion point (the
  only change to it); `ox/oy` keep the true target tile, so skills always
  report/act on the tile under the effect. `spawnFxPx` unchanged (raw
  px). Drawing/culling/hitboxes already assume the center anchor —
  unaffected.

Files: `src/gamesfx.h`, `src/gamesfx.cpp`, `src/game.cpp` (`spawnFx`).

## 6b. Looping fx: loop cycle limit

Table field on `fxData_t` (beside `ttl`/`align`):

```cpp
uint8_t loops; // max animation cycles for FX_REPEATABLE; 0 = unlimited
```

- In `CGame::tickFx()`'s existing expiry pass: a looping fx dies when
  `timer / (speed * frameCount) >= loops` — cycle count derived from
  `timer` alone (like `fxFrameIndex`), so **no new per-instance state,
  no wire change**, and save/restore reproduces it exactly.
- `loops == 0` keeps today's behavior byte-identical (ttl-only
  lifetime) — compat for every existing repeating row
  (`FX_POISON_AURA` & co.).
- For skill fx this bounds the **damage window**: each loop replays the
  hitbox frames (§4), so a looping skill deals its hitbox damage once
  per cycle, throttled by the invincibility window, for at most `loops`
  cycles.

## 7. Phase B/C — ini wiring + cast trigger

- Phase B: registry rows (`BULLET` generic kind; lightning strike,
  meteor: effectId, damage placeholders, telegraph -1) + §2/§3 parser
  work; `data/bosses.ini` sample entries (Beholder
  `skill LIGHTNING_STRIKE rate=30`, Mr. Demon `skill METEOR rate=45`);
  authored hitbox JSONs in `data/fx/` (boxes tagged `type: 2` SPECIAL1).
- Phase C: in the `Chase` branch of `manageBosses()`
  (`src/game_ai.cpp:319-336`), beside the legacy bullet gate, iterate
  `boss.data()->skills`:

  ```cpp
  const bossData_t *bd = boss.data();
  for (uint8_t i = 0; i < bd->skillCount; ++i)
  {
      const bossSkill_t &slot = bd->skills[i];
      if (rng.range(0, slot.rate) != 0 ||
          boss.distance(player) > boss.data()->distance_pursuit)
          continue;
      const bossSkillDef_t *sk = &g_skillDefs[slot.skill];
      switch (sk->kind)
      {
      case SKILL_BULLET:
          handleBossBullet(boss); // existing attack code, reads boss's own bullet data
          break;
      case SKILL_SPECIAL1:
          break; // no cast spawn — consumed as rate-gate inside handleBossHitboxContact (§1)
      case SKILL_FX:
          spawnFx(sk->effectId, player.x(), player.y()); // strikes where he stands
          if (sk->telegraphFx >= 0)
              spawnFx(sk->telegraphFx, player.x(), player.y());
          playSound(boss.data()->attack_sound);
          break;
      }
  }
  ```

  - `handleBossBullet` is called **exactly as it is today** — bullet
    tile/algo/sound/ttl come from the boss's own `bullet*` fields
    (§1 v4 ruling).

  - **BULLET slot vs legacy gate:** the legacy fire gate grows one
    yield condition — `bool hasBulletSlot` (set when any skill is
    `SKILL_BULLET`) skips it, so a migrated boss cannot double-fire.
    Bosses without skill slots take the identical legacy path.
  - **`SKILL_SPECIAL1` consumption** (`src/game_ai.cpp:209`): the
    damage callback consults the boss's SPECIAL1 slot; when declared,
    `r.type == SPECIAL1` hits apply only if `rng.range(0, rate) == 0`
    (rng re-seeded `setTick(ticks)` in-handler, like `manageBosses`).
    MAIN/ATTACK/SPECIAL2 hits and slot-less bosses are untouched.
  - No LOS gate for now (later-stage question, §8 Q3).
  - No cooldown state (rng-rate gate) → `CBoss::save/restore` wire
    (`src/boss.cpp:444-520`) untouched, no version bump.

## 8. Question log (maintainer rulings)

- Q1 damage timing: collision registration per tick; invincibility
  frames throttle. Folded into §4.
- Q2 damage source: originally "fx table"; **superseded by v2**: the
  SKILL registry owns damage (§1). `damage_special1/2` stay sprite-box
  only.
- Q3 LOS: later-stage question — deferred, ships ungated.
- Q4 meteor art: `FX_FIRE_COLUMN` is the meteor analogue (ruling).
- Q5 hitbox JSON geometry: frame strips, boxes relative to strip, frame
  positions derived from `frame.width/height/cols` (§0b).
- Q6 fx hitbox JSON location: **`data/fx/`** (supersedes earlier
  `data/pixels/` answer).
- v2 rulings: SKILL concept links FX+hitbox+damage; boss skill = type +
  use frequency (new data structures §1/§2); telegraph hint required
  (§5); fx alignment modes center-tile / bottom-flush (§6).
- v3 ruling: **BULLET is a skill** (§1, §7).
- v4 ruling: BULLET is a **generic** skill kind — the registry holds no
  per-bullet entries; the actual bullet info (tile/algo/sound/ttl)
  **resides in `bossData_t`** and `handleBossBullet` is called
  unchanged. A declared BULLET slot takes precedence over the legacy
  rate gate (no double-fire).
- v4 syntax: linkage is **repeated unnumbered `skill NAME rate=N`
  lines** → per-line entries (parser accumulates, §3). [Storage
  superseded by v6: fixed array + counter.]
- v5 ruling: fx skill hitbox JSONs (`LIGHTNING_STRIKE`, `METEOR`)
  author their damaging boxes as **SPECIAL1 (`type: 2`)** (§0b, §4).
- v6 ruling: **max 4 skill slots per boss**, tracked by a used-slot
  counter `skillCount` (fixed `skills[4]` array in `bossData_t`, §2;
  parser caps + warns, §3).
- v7 ruling: **SPECIAL1 is a skill too** — generic `SKILL_SPECIAL1`
  kind; hitbox JSONs + `damage_special1` stay in `bossData_t`, attack
  code is the existing contact hit-test, the slot rate-gates it
  (§1, §7). **Beholder uses SPECIAL1 as its primary skill.**
- v8 ruling: **looping fx get a loop cycle limit** — `fxData_t::loops`
  caps `FX_REPEATABLE` animation cycles; `0` = unlimited keeps the
  ttl-only rule (§6b).
- v9 ruling: strikes have a **lead time**; until proper hint art exists
  the lead-in window shows a **red square over the target tile** as
  placeholder, drawn from fx draw-time state (§5).
- v10 ruling: **sparse hitbox frames** — most frames have no hitboxes,
  hit frames hold up to `MAX_HITBOX_PER_FRAME` (4); only hit frames get
  stored records; `firstHitFrame`/`lastHitFrame` cached at load (§1).

## 9. Explicitly skipped (add when the trigger fires)

- **Delayed telegraph→impact sequencing** (impact spawns after the
  marker finishes): needs pending-cast state; same-tick double spawn +
  hitbox lead-in covers it.
- **Splash to bosses/monsters** (`splashRadius`, `blastRadius`-style
  loop) — when skills must clear mobs / chain barrels.
- **Telegraph art** — data task, hook exists (`telegraphFx`).
- **Meteor travel projectile** — needs art + moving fx (fx have no
  velocity today).
- **Per-boss cooldown state** — when rate-gating feels too random; costs
  `CBoss` wire + `ENGINE_VERSION`/`EXPECTED_VERSION` bump (BUG-H12).
- **`SKILL_SPECIAL2`** — same generic-kind pattern as SPECIAL1 would
  fit `damage_special2` exactly; add the row + gate when a boss needs it.
- **Skill ↔ fx 1:N** (two bosses' different skills sharing one fx id) —
  forces skill index on `fx_t` (wire change).

## 10. Verification (repo harness style)

- `t_gamesfx`: registry lookup; spawn damaging skill fx on the player →
  first box-overlapping tick drops `damage` hp; boxes authored late
  (lead-in) → no damage before that frame; synthetic §0b JSON whose box
  misses the player although frame `w/h` overlaps → untouched; mid-life
  save/load round-trip keeps landing.
- `t_bossdata`: ini snippet with repeated lines `skill BULLET rate=10` /
  `skill LIGHTNING_STRIKE rate=30` / `skill METEOR rate=45` parses to
  three `skills` entries + `skillCount == 3` in order with the right
  rates + registry indices; a 5th `skill` line is ignored with a warning
  (4-slot cap); unknown token → entry skipped + error logged, counter
  unchanged; no `skill` line → `skillCount == 0`; legacy `bullet ...`
  line still parses unchanged (compat regression). `data/fx` JSON
  loader keeps only `type != 0` boxes (SPECIAL1 authored).
- `t_bossdata` or fx suite: `data/fx/*.json` loader fills `frames`
  (synthetic temp JSON; skip-guard when `data/` absent).
- alignment: `spawnFx` on a `FX_ALIGN_BOTTOM` row → `y == (ty+1)*T -
  def->h/2`, `x` tile-centered; `FX_ALIGN_CENTER` regression unchanged.
- loops: `FX_REPEATABLE` row with `loops = 2` expires exactly after two
  cycles even with a huge `ttl`; `loops == 0` row lives to its `ttl`
  (regression); a looping skill fx damages once per cycle, never after
  the cap.
- telegraph: lead-in predicate is unit-testable without a renderer —
  skill fx with boxes authored from frame 3 → frames 0..2 report
  "in lead" (red-square condition, via `firstHitFrame`), frame 3+ does
  not; loader stores **no record for box-less frames** and derives
  `firstHitFrame`/`lastHitFrame` correctly from a sparse synthetic JSON
  (e.g. boxes on 2 of 6 frames, up to 4 boxes on one).
- `t_game_ai`: boss `rate=1`, player in range → FX slot: `getFx()` gains
  the skill fx (+ telegraph instance iff `telegraphFx >= 0`); BULLET
  slot: `m_monsters` gains the bullet actor via the shared
  `handleBossBullet` path; SPECIAL1 slot `rate=1` + player on a
  SPECIAL1 box → `damage_special1` lands every tick, no slot →
  unchanged unthrottled contact damage; `rate=0` → nothing; legacy-gate
  boss (no skill slots) fires exactly as before.
- Gate: `cmake -B build && cmake --build build`, `ctest --test-dir
  build`; refresh lists via `python bin/refresh.py`.
- Compliance: flip `extended-fx.md` §1.4, add a skill-concept module
  note, update hashes/tally **after** tests pass (AGENTS).

## 11. Sequencing

1. §6 alignment (self-contained, testable) →
2. §4 collision with synthetic in-code hitboxes →
3. §1 registry + §3 JSON loader (`data/fx/`) →
4. §2/§3 ini + parser →
5. §7 cast trigger →
6. data: ini entries, hitbox JSONs, telegraph art task →
7. spec/compliance update. Each step builds and tests alone; nothing is
live in maps until step 5+6.
