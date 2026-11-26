# Plan — traps.ini: large irregular traps (CTrap, minimal)

Status: **PROPOSED** (v8, 2026-11-26). Maintainer rulings this plan
encodes:

1. **Do not modify `CBoss`** — no member extraction, no delegates, no
   shared-view refactor. Traps are standalone.
2. Hierarchy stays flat: `ISprite → CActor | CBoss | CTrap` (`CActor`
   is the sibling class, not a base — traps do not reuse `CActor`'s
   tile-based movement).
3. **Traps have no score, no details** (no hp bars, no name overlay,
   no hp/destructibility in the initial cut).
4. **Traps initially share ONE sprite sheet** — no sheet-generalization
   work; one new sheet asset.
5. Traps move parametrically (lateral saws) and **never pathfind**
   (v4 ruling). **No `bullet` in trap definitions** (v8): the initial
   cut has no caster traps — projectile mechanics deferred (§7).
6. Traps do not require `CActor`-style AI — confirmed: `CTrap` has no
   pathfinder, no target, no aggro; `update()` is timer + rectangle
   math only.
7. Attr band `0xd8-0xdf` is a good start; more bands get pegged later
   if needed — not this plan's problem.
8. **Ini grammar identical to `bosses.ini`: `[name_of_trap]` sections
   with a `type 0xd8` key line.** v7's `[0xNN]`-as-type fork was
   rejected; no per-file syntax divergence.

History: v1 tile premise wrong (tile hazards = `CActor` territory);
v2 rode traps inside `CBoss`; v3/v5 sharing bases — rejected
(maintainer: don't modify CBoss, flat hierarchy); v6 drew on `CActor`
movement conventions — corrected (sibling class, wrong model); v7's
`[0xNN]` section syntax and trap `bullet` line — both rejected here.
Traps are **irregular-sized entities larger than 16×16 px on the
half-tile grid** (`GRANULAR_FACTOR = 2` — the convention bosses
proved). Out of scope, unchanged: attr-step stat traps
`ATTR_TRAP`/`ATTR_FREEZE_TRAP`/`ATTR_POISON_TRAP`
(`src/attr.h:41-43`, `CGame::manageTrigger` `src/game.cpp:370-418`).

## 0. Ground (verified 2026-11-26)

- Half-tile conventions to copy (not share) — all from **`CBoss`**,
  nothing from `CActor` (whose movement is full-tile and
  pathfinding-driven): anchor/hitbox math `CBoss::canMove`/`move`
  (`src/boss.cpp:160-240`), map solidity check `CBoss::isSolid` is a
  3-line `TileDef` lookup (`src/boss.cpp:58-64`), 8-px render grid +
  anchor rect (`src/gamemixin.cpp:1060-1117`, twin
  `src/engine_hw.cpp:1087-1208`), aim-indexed frame selection from
  `boss_seq_t` (`CBoss::currentFrame`).
- Repeat-hit throttling is free: `addHealth`'s `S_PLAYER_HURT`
  invincibility window (`src/game.cpp:1034-1037`) — the same reliance
  boss contact damage has.
- Pump (`src/gamemixin.cpp:1529-1533`), spawn attr scan
  (`CGame::spawnMonsters` `src/game.cpp:739-834`), free codes
  `0xd8-0xdf` (`src/attr.h:38`).
- Save: `CGame::read/write` version-gated per-class records
  (`src/game.cpp:1447-1465,1594-1605`); new container ⇒
  `ENGINE_VERSION` (`src/game.cpp:60`) + `tests/t_game.cpp`
  `EXPECTED_VERSION` bump together (AGENTS.md).
- Ini loader precedent: `src/bossdata.cpp` tokeniser (`:409-425`);
  its duplicate *section-name* warning (`:908-910`) is exactly the
  check `trapdata.cpp` gets right the first time.

## 1. traps.ini

**Grammar identical to `bosses.ini` (decision 8):** `[name_of_trap]`
sections, `type 0xd8` key line, `name value key=value` tokens, `#`
comments. Labels are comments-only (traps have no runtime name —
decision 3); they exist for authors and for LOGE messages. Type
uniqueness is enforced on the **`type` key**, not the section name
(the boss loader's duplicate-*section-name* warning at
`src/bossdata.cpp:908-910` is the gap this loader closes).
Cross-file type collisions with bosses are structurally impossible
(disjoint bands, separate tables).

Live file: `<prefix>/traps/traps.ini`. Doc sample
`docs/spec/traps/traps.ini`. No `sheet` key (single shared trap sheet,
§4). No `score`, `hp`, `show_details`, `is_goal`, `speed`, `distance`,
`path`, `skill`, `bullet` (§7 for projectiles).

```ini
# traps.ini — boss-grammar ini; types in the trap band 0xd8-0xdf

[spike_pit]
type            0xd8
speed_anime     2
damage          8 attack=24          # contact damage on Idle / Strike
hitbox          0, 4, 4, 4           # half-tile units (boss convention)
trigger         proximity=5
aims            1
idle            6                    # frame count; sheet base derived at load
attack          10

[buzz_saw]
type            0xd9
speed_anime     1
damage          24 attack=24
hitbox          1, 2, 2, 2
motion          axis=h range=8 speed=2
aims            1
idle            8                    # the spin IS the idle seq

[pop_up_spikes]
type            0xda
speed_anime     2
damage          0 attack=28          # harmless while retracted
hitbox          0, 2, 4, 2
trigger         cycle=30             # periodic lunge: Strike = spikes out
aims            1
idle            2
attack          4
```

Loader (`src/trapdata.{h,cpp}`, ~200 lines; forked tokeniser, not the
boss `Loader`):

```cpp
struct trapData_t
{
    const char *name;               // section label, LOGE messages only
    uint8_t type;
    int speed_anime;
    int damage, damage_attack;
    hitbox_t hitbox;              // reuse existing struct
    uint8_t trigger;              // NONE | PROXIMITY | CYCLE | STEP
    uint8_t trigger_param;        // radius / period ticks
    uint8_t motion_axis;          // NONE | H | V
    int16_t motion_range; uint8_t motion_speed;
    int aims;
    boss_seq_t idle, attack;      // reuse existing struct; base = cumulative frame offset
};
extern std::vector<trapData_t> g_trapData;
const trapData_t *getTrapData(uint8_t type);
int loadTrapData(const std::string &iniPath);   // missing file: LOGW, 0
void freeTrapData();
```

Validation: `type` required and must fall in `ATTR_TRAPSPAWN_MIN..MAX`
(else LOGE + section dropped); **duplicate `type` across sections →
LOGE naming both sections + lines, later row dropped**
(maintainer-requested detection; a first-match lookup must never
silently shadow a second declaration); `motion` + `trigger` mutex;
boss-only keys (`score`, `hp`, `bullet`, ...) → LOGE + row dropped.

## 2. `CTrap : public ISprite` (`src/trap.{h,cpp}`, ~150 lines)

`ISprite` is the abstract interface (`src/isprite.h:27`); `CActor`
and `CBoss` are its existing concrete implementors; `CTrap` is a
third peer implementing the same pure-virtual surface with its own
half-tile members.

```cpp
class CTrap : public ISprite
{
public:
    enum TrapState : uint8_t { Idle, Strike, Done, MAX_STATES = Done };
    CTrap() = default;
    CTrap(int16_t x, int16_t y, const trapData_t *data);
    // ISprite: full override set. x/y/pos/type/setAim/getAim from own
    // members; getGranularFactor() = 2; isBoss() -> false;
    // getTTL() -> NoTTL; distance() = CBoss formula (4 lines).
    // canMove/move: own half-tile step + map-solid check copied from
    // CBoss::isSolid pattern (map.at -> TileDef type lookup); traps
    // only ever aim along their motion axis.
    void update(const uint32_t ticks, const CActor &player);
    bool isDone() const { return m_state == Done; }
    bool read(IFile &); bool write(IFile &);
    const trapData_t *data() const { return m_data; }
    int currentFrame() const;   // idle/attack seq base + aim*len + ptr
private:
    const trapData_t *m_data = nullptr;
    int16_t m_x = 0, m_y = 0;   // half-tile
    uint8_t m_aim = 0;
    int m_framePtr = 0;
    TrapState m_state = Idle;
    int8_t m_dir = 1;           // sweep direction
    int16_t m_steps = 0;        // half-tiles traveled this sweep
    uint16_t m_cool = 0;        // cycle countdown
};
```

`update()` per tick (~50 lines, no AI, no pathfinding, no CPath
member):

- **animate** on `speed_anime` divisor; `Strike` seq wraps → `Idle`
  and reload `m_cool`; `Idle` seq loops.
- **motion** (if axis): every `motion_speed` ticks aim = `m_dir` along
  axis; `canMove`/`move` or flip; flip at `motion_range`.
- **trigger**: `PROXIMITY`: `distance(player) <= trigger_param` →
  `Strike` (aim at player, lunge-style quadrant math); `CYCLE`:
  `m_cool` expiry → `Strike` (periodic lunge, e.g. pop-up spikes);
  `STEP` deferred (§7).
- **contact**: rect-overlap of `m_data->hitbox` (at `m_x/m_y`,
  half-tile units, same anchor convention) against the player's tile
  → `addHealth(-(Strike ? damage_attack : damage))`; repeat hits
  throttled by `S_PLAYER_HURT`. 0-damage traps inert. Damage fires
  from the rect only — no per-frame JSON boxes yet (§7).

## 3. CGame integration (the only edits to existing files)

1. **Boot** `src/engine_hw.cpp:236`: `loadTrapData(prefix +
   "traps/traps.ini")` after `loadBossData`.
2. **Spawn** `CGame::spawnMonsters` (`src/game.cpp:762+`): new
   `RANGE(attr, ATTR_TRAPSPAWN_MIN, ATTR_TRAPSPAWN_MAX)` branch (new
   `#define`s `0xd8`/`0xdf`, avoiding legacy `ATTR_TRAP 0xeb`) →
   `getTrapData(attr)` → `m_traps.emplace_back(tile*2...)`; unknown →
   LOGW + ignored; attr consumed; `m_traps.clear()` beside the others
   (`:741-744`). Member `std::vector<CTrap> m_traps` + `traps()` /
   `getTraps()` accessors per repo test convention.
3. **Pump**: `manageTraps(ticks)` (`src/game_ai.cpp`, next to
   `manageBosses`) → per trap `update()`; called at the pump
   (`src/gamemixin.cpp:1529`).
4. **Render**: new `drawTraps()` per renderer (`gamemixin` +
   `engine_hw`), modeled on each renderer's `drawBossses` body minus
   hp-bar/mirroring/sheet branches: single trap `CFrameSet`, 8-px
   anchor rect, skip `isDone()`. Two small copies — existing
   double-render convention.
5. **Save/load** `CGame::read/write`: new trap block after bosses.
   Record: `type u8 | x i16 | y i16 | framePtr u16 | state u8 | aim u8
   | dir i8 | steps i16 | cool u16` = **13 bytes**; `read` re-derives
   `m_data` via `getTrapData(type)` (fail → LOGE + reject, same join
   as `src/boss.cpp:483`), bounds-checked (`dir ∈ {±1}`, `steps ≤
   range`, `state < MAX_STATES`); `MIN_TRAP_RECORD = 13` beside
   `MIN_BOSS_RECORD` (`src/game.cpp:64-66`). **Same commit:
   `ENGINE_VERSION` bump (`src/game.cpp:60`) + `EXPECTED_VERSION`
   mirror (`tests/t_game.cpp`)**; previous version still loads (no
   trap block), version-gated per existing pattern; no back-compat
   removal (AGENTS.md).

## 4. Assets

**One** new trap sheet: manifest entry + `m_trapSheet` in both
renderers, loaded like sheets 0-2 (`engine_hw.cpp` boot,
`ASSET_SHEET*` pattern). `trapData_t` frame bases are assigned at load
per section (cumulative frame counts within the trap sheet) — the ini
lists only frame counts, so trap art order on the sheet is a loader
detail, not an authoring burden. Sheet membership is then implicit in
the frame-id range — same trick bosses use to share one physical
strip across sheets 0-2 (`docs/spec/bosses/bosses.ini:15`). Deferred
alternative: reuse an existing sheet index if the artist packs traps
onto sheets 0-2.

## 5. Rollout

1. `trapdata` + band `#define`s + tests. Nothing queries it — no
   behavior change.
2. `CTrap` + spawn + `manageTraps` + `PROXIMITY` + contact damage +
   `drawTraps` + save/load + version bump. First playable spike pit.
3. Saw `motion`; `CYCLE` trigger (`pop_up_spikes`).

## 6. Tests & docs

- `tests/unit/t_trapdata.cpp`: fixture ini — values, band validation,
  duplicate-`type`-key drop (two sections, same type), rejected keys
  (`bullet`, `score`, ...), `motion`+`trigger` mutex, missing file =
  empty + success.
- `tests/unit/t_trap.cpp`: proximity → `Strike` → wrap → `Idle`; saw
  trace (flip at range, flip at wall); `CYCLE` period → `Strike`;
  contact damage only when rects overlap, 0-damage inert; record
  round-trip mid-sweep.
- `tests/t_game.cpp`: save+load with a live trap; `EXPECTED_VERSION`
  mirror; old-version save loads.
- Spec `docs/spec/gamespec/trap-content.md` + compliance mirror
  (SHA256 checkpoint per docs rules).

## 7. Deferred (each needs its own decision, not this plan)

- Per-frame JSON hitboxes for strike reach (contact stays main-rect
  until an authored trap needs more — then reuse `getHitboxes` keying
  with a trap sheet offset).
- Destructible traps (hp + blast interaction; note: **no score** per
  ruling even then).
- `STEP` trigger (player inside MAIN rect — 3 lines from contact
  check; ship when needed).
- Projectile traps (dart holes): no `bullet` key per ruling. When an
  author needs one, decide the mechanism then — reuse the shared
  `spawnBullet` (`src/game_ai.cpp:96-121`) with a trap-side
  `bulletSpec_t` member (bosses route the same POD through
  `skill BULLET` slots; traps need no skill-slot machinery).
- Second trap sheet → sheet index in ini + renderer branch.
- More trap types than 8 → peg another attr band (maintainer-approved
  path; the trap loader band-checks against the widened range, and
  any pegged band must stay disjoint from the boss band and every
  other attr consumer).
- Traps blocking player movement (contact model today, like bosses).
- Aims-3 mirroring.

## 8. Skipped on purpose

- Any `CBoss` change (maintainer ruling); shared helpers/views/
  extraction — the copied patterns are a handful of lines each.
- `CActor`-derived movement/AI reuse — `CActor` is a sibling class of
  `CBoss`, not a base; its full-tile pathfinding movement is the
  wrong model for traps (v6 wording error, corrected).
- Tile-sized hazards — `CActor` territory.
- Pathfinding — ruling; motion is parametric only.
- Score, hp bars, name details — ruled out.
- `bullet` / `skill` keys in traps.ini — ruled out (v8); projectiles
  deferred (§7).
