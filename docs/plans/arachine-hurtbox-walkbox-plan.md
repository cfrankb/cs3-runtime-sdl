# Plan: per-frame MAIN hurtboxes + `walkbox` option (Arachine support)

Date: 2026-11-24
Status: proposed — open decisions D1 and D3 (§6) block §2 and §5 only;
§1, §3, §4 are unblocked.

## Problem

The boss hurt/contact model is a single axis-aligned MAIN rect
(`bossData_t::hitbox`, injected unconditionally by
`CBoss::testHitbox1` `src/boss.cpp:83-87`), sized for dense center-mass
monsters. Arachine (sheet 2, aims 3) is art with long slender legs and
large gaps:

- The fat MAIN box damages/hits across empty space (shots that visibly
  pass between legs register; contact damage triggers in air).
- The legs extend beyond any narrow body rect, and `CBoss::canMove`
  uses `hitbox.width/height` as the motion footprint
  (`src/boss.cpp:161-162`) — shrinking the box without a separate
  footprint lets the body walk into solid tiles while the legs clip
  walls.

## Model ruling (maintainer 2026-11-24)

- The INI `hitbox` is the **fixed anchor/cell-reference rect** and is
  never overridden by per-frame data. Its `x/y` are the sprite draw
  anchor (`gamemixin.cpp:1111-1117`, `engine_hw.cpp:1142` — draw uses
  only `x/y` plus frame size, never `w/h`), the basis for authored-box
  placement (`m_x + c.x - hbMain.x`, `boss.cpp:98`) and the aims-3
  mirror axis.
- The hurt model becomes **additive**: the (shrinkable) always-on body
  rect + per-frame MAIN boxes authored in the §0b hitbox JSONs.
  `testHitbox1` already injects then appends — no replace/fallback
  semantics are needed or wanted. No authored box ⇒ legacy behavior.
- Leg contact damage and shootable legs are the same MAIN boxes
  (`handleBossHitboxContact` damages on any box type).
- The `hitbox` ini key stays **required in every boss definition**:
  per-frame MAIN boxes never substitute for the INI MAIN (anchor,
  always-on core hurtbox, fallback box for derived walkbox modes).
- Motion legality moves to an explicit per-boss **`walkbox` ini
  option** (§4) with four modes; key absent = `normal` = today's
  arithmetic. No live boss definition carries the key, so every boss
  in existence stays on `normal`.

## Ini representation

```ini
walkbox         normal      # DEFAULT = current behavior (footprint = hitbox w/h)
walkbox         combined    # derived: union of the MAIN-type hitbox frames
walkbox         1, 2, 5, 6  # static x,y,w,h in 8px TILES — one box, whole sprite
walkbox         custom      # per-frame from JSON hitbox boxes of the new WALK type
```

## Existing flow (verified)

- Type-0 (MAIN) boxes in hitbox JSONs are dropped as markers:
  `readHitboxJson` (`src/bossdata.cpp:479-481`, callers: boss table +
  fx skills). The mirror in `docs/spec/bosses/parse.py:205-208` is
  **deprecated, frozen, and stays unchanged** (spec-era tooling, not a
  runtime code path).
- Data audit (2026-11-24): every ini-referenced boss JSON (live assets
  via the `data` → `cs3-data-private` symlink) and every `fx/*.json`
  contains **zero** type-0 boxes ⇒ removing the drop is a provable
  no-op for all existing content. The legacy untrimmed
  `beholder1_attack.json` (48 type-0 anchor-marker strips) was deleted
  from `cs3-data-private`; reference-only copies remain in
  `docs/spec/bosses/data/` and external tools (`private_tools`,
  `cs3-map-edit`) — none ini-referenced.
- Pinned expectations to flip: `tests/unit/t_bossdata.cpp:380`
  (`{"type": 0}` fixture asserts the drop).
- `canMove` (`src/boss.cpp:158-217`): half-tile step, map-bound check
  with hitbox `w/h` (`:186-188`), then solid-checks only *newly
  covered* world tiles via `m_solidCheck` (`isSolid` /
  `isGhostBlocked`, `setSolidOperator` `:570-578`). Player tiles pass
  the solid predicates (contact-damage model) — a player standing
  between legs cannot deadlock the spider.
- Pathfinding probes `canMove` per candidate node
  (`src/ai_path.cpp:185-188, 289-292, 343-365, 500-503, 552`) — the
  walkbox lookup must stay a cheap table read per probe (int rect /
  margin quads, no geometry computed during pathfinding).
- `HitBoxType` is `MAIN=0, ATTACK=1, SPECIAL1=2, SPECIAL2=3`
  (`src/bossdata.h:76-82`); `damage()` switches over it
  (`src/boss.cpp:396-406`).
- `MAX_HITBOX_PER_FRAME = 4` (`src/bossdata.h:51`) sizes the fixed
  array in `sprite_hitbox_t`; clamp-to-4 + LOGW is pinned by the
  hostile-triage tests.
- `hitbox` ini key parsed at `src/bossdata.cpp:719-727`; when absent
  it currently defaults silently to `{0,0,0,0}` (degenerate anchor +
  zero-size always-on box). All four live boss definitions declare it.
  No live definition carries a `walkbox` key (new key).
- Non-negative half-tile coordinate contract for hitboxes (maintainer
  ruling 2026-11-18; trunc==floor in `testHitbox1` `:117-119`) — no
  walkbox mode may rasterize signed tile ranges; negative edges must
  be rejected by the bounds check *before* any division.
- No `ENGINE_VERSION` bump anywhere in this plan: `bossData_t` /
  `sprite_hitbox_t` are static ini/JSON-loaded data, not serialized
  state (`CActor`/`CMap`/`CLayer` untouched).

## Changes

### 1. `src/bossdata.cpp` — type-0 legal

- Remove the type-0 drop in the shared `readHitboxJson` core.
- Deprecated `docs/spec/bosses/parse.py` stays unchanged — it keeps
  its marker-drop and is no longer the behavior of record; the loader
  is. Flag the divergence in the spec text so nobody re-mirrors it.
- Keep all existing validation (negative coords rejected, frame-block
  bounds, clamp per §6 cap decision).
- Enforce the required-`hitbox` contract: key missing → `LOGE` at
  load (garbage convention: data keeps the silent `{0,0,0,0}`
  default). Zero behavior diff — every live definition declares it.
- Authoring note for the spec: per-frame anchor-marker overlays leave
  the JSON; type 0 now means live MAIN damage. Old habit (the deleted
  beholder file) becomes a footgun otherwise.

### 2. `src/bossdata.{h,cpp}` — box cap (decision D1, §6)

- Raise `MAX_HITBOX_PER_FRAME` so 8-legged art never has to cluster
  legs into a 4-box budget. Both MAIN (combined) and WALK (custom)
  boxes count toward the cap. Touch points: `sprite_hitbox_t` array,
  clamp/LOGW tests. (`parse.py` frozen — not touched.)

### 3. Tests — per-frame MAIN + walkbox modes end-to-end

- Flip `t_bossdata.cpp:380` fixture: type-0 box is carried through,
  not dropped.
- New `t_boss` case: authored MAIN box hits via `testHitbox1`
  alongside the injected anchor rect (additive coexistence), plus
  mirror reflection of a MAIN box in boss space (reuse the
  `t_boss.cpp:266-317` stub pattern).
- fx regression: existing `t_fx` suites must stay green (shared
  `readHitboxJson` core; fx JSONs carry no type-0; type-4 WALK boxes
  are never authored in fx JSONs and never damage).
- `t_bossdata` hostile cases: boss block without a `hitbox` key loads
  with the `{0,0,0,0}` default as today and the new `LOGE` fires;
  unknown `walkbox` token → `LOGW` + `normal`; static `walkbox` with
  `w/h < 1` → `LOGE` + `normal`.
- Mode pins (`t_boss`):
  - `walkbox` key absent and `walkbox normal` → `canMove` footprint
    bit-identical to the current `(m_x, m_y, hb.w, hb.h)` arithmetic,
    bounds check included (`:186-188`).
  - `combined` → margin chain: stride frame with its own MAIN box →
    per-frame quad; walk frame without boxes (or non-walk state) →
    aim-family aggregate; no MAIN data → plain anchor box. Assert
    per-frame ⊆ aggregate nesting.
  - static rect → same box for every aim/frame; mirrored-horizontal
    aim reflects it about the anchor centre; box smaller than the
    anchor is honored (authored intent); negative edge at map border
    rejects before division.
  - `custom` → WALK-type boxes drive `canMove`; `testHitbox1` never
    yields a WALK hit (no damage, no contact, no ice-cube/barrel);
    mirrored-horizontal swaps left/right margins; chain fallback same
    as `combined` but from WALK boxes.

### 4. `src/boss.{h,cpp}` + `src/bossdata.{h,cpp}` — `walkbox` option

New `HitBoxType::WALK = 4` (JSON `type: 4`): ingested by the boss
hitbox table, consumed **only** by motion legality. `testHitbox1`
filters WALK boxes out before rasterizing (never damage/contact/
ice/barrel); `damage()` never sees it; the fx skill loader drops type
4 (boss-motion-only concept).

`bossData_t` gains `walkboxMode` (enum `normal|combined|static|custom`,
default `normal`) plus, for `static`, a half-tile rect. All derived
tables (margin quads, static rect) are computed once in
`loadBossData`; not serialized → no `ENGINE_VERSION` bump.

INI parsing: `walkbox` value is either a keyword token or four
numbers (reuse the `parseNumberList` pattern from `hitbox`/
`healthbar_offset`, `src/bossdata.cpp:719-742`). Unknown token →
`LOGW` + `normal`. Static rect `w/h < 1` → `LOGE` + `normal`.
`parse.py` frozen — loader is the behavior of record.

**Mode semantics in `canMove` (probed footprint):**

- `normal` — exact today: box `(m_x, m_y, hitbox.w, hitbox.h)`.
- `combined` — anchor box grown by four non-negative **extension
  margins** derived at load from the walk-sequence MAIN boxes
  (`left = max(0, hb.x - minX)` etc.), two granularities: a margin
  quad per walk frame carrying ≥1 MAIN box (keyed by absolute
  frameID, same keying as `getHitboxes`) and one per aim-family
  aggregate (mirrored horizontal = left/right swapped). Selection at
  probe time, most specific first: probed aim + current
  `m_framePtr`'s quad → aim-family aggregate → zero margins. Because
  every quad is a `max(0, …)` growth of the anchor box and frames/
  aims nest into aggregates, all stages are supersets of the box
  used today: no boss gains movement freedom or new wall-adjacent
  paths.
- `static x,y,w,h` — authored in **8px world tiles** (×2 to half-
  tiles), same cell-relative anchor semantics as the `hitbox` line:
  box = `(m_x + 2*(x - hb.x), m_y + 2*(y - hb.y), 2*w, 2*h)`. One
  box for the entire sprite: no per-frame or per-aim derivation.
  Authored intent is authoritative — it may exceed **or shrink**
  below the anchor box. Negative left/top edges (offset or position)
  fail the bounds check before tile rasterization, keeping the
  trunc==floor contract untouched. Mirrored-horizontal aims reflect
  `x` about the anchor centre like all cell geometry.
- `custom` — like `combined` but the margin source is **WALK-type**
  (4) JSON boxes instead of MAIN; the same two-granularity margin
  chain and probed-aim/frame selection apply.

Shared rules for the derived modes (`combined`/`custom`):

- Derivation reads the **walk (`moving`) sequence only** — one
  wide-stance attack frame must not groom the boss out of corridors
  it can walk through, and non-walk states fall through to the
  aim-family aggregate (per-frame legality breathes with the stride
  pose, never mid-attack).
- `canMove(aim)` uses the **probed** aim's data, not the boss's
  current `m_aim` (pathfinding probes steps before the boss turns);
  the per-frame stage varies only with `m_framePtr`, so a probe is a
  plain int-table lookup.

Unchanged inside `canMove` for every mode: half-tile step, "newly
covered tiles only" rule (full-box validation would freeze legacy
bosses already overlapping obstacles on existing maps; its
advisory-at-overlapped-edges cost is mitigated by a one-shot
spawn-time overlap `LOGW`), and the `m_solidCheck` per-tile
predicate (ghost/swamp/ice/water rules inherited for free; player
tiles pass, so a player between the legs cannot deadlock the spider).

`canMove` is the **only** consumer of the walkbox. Untouched
consumers, enumerated so the blast radius is reviewable: `testHitbox1`
injection (which gains only the WALK filter), draw anchor (`x/y`
only), bullet spawn / AI aim (`game_ai.cpp:178-192`), `distance()`,
`engine_hw.cpp` draw rect.

### 5. `cs3-data-private/bosses/bosses.ini` + JSON rewrite (external)

- Arachine: keep `hitbox 6, 4` anchor `x/y`, shrink `w/h` toward
  center mass (stay x-centred in the 13-half-tile cell so
  `warnAsymAnchor` stays quiet) and set the `walkbox` mode (decision
  D3, §6 — recommended `custom` once the per-frame WALK boxes are
  authored; `combined` if MAIN leg coverage is meant to double as
  footprint; `normal` is the correct interim value while the fat
  anchor box stays). The hitbox shrink, the `walkbox` line, and the
  new WALK JSON land in ONE data change; until then Arachine plays
  exactly as today.
- Bullet spawn offsets / AI aim classification keep consuming the
  anchor rect's `w/h` (`game_ai.cpp:178-192`) — body-centered spawns,
  accepted.
- Rewrite `ARACHINE_{WALK,ATTACK,IDLE}.json` against the settled
  contract (MAIN leg/body boxes for the hurt model; WALK-type boxes
  per frame if mode `custom`; drop the parked `#data/...` ini comment
  markers when wiring each sequence). Existing files are declared
  incorrect by the maintainer — no migration, no back-compat.

### 6. Open decisions

- **D1 — `MAX_HITBOX_PER_FRAME`:** raise to 8 (recommended: one row of
  leg boxes + body, cost is a fixed array + test bump) or keep 4 and
  force author-time clustering. Must rule before the JSON rewrite.
- ~~D2 — walkbox encoding~~ **superseded 2026-11-24** by the explicit
  `walkbox` ini option (this supersedes the "no ini data, always
  derived" draft; margins remain the internal mechanism for
  `combined`/`custom`).
- **D3 — Arachine mode:** `custom` (per-frame WALK boxes; footprint
  authored independently of hurt — recommended) vs `combined` (MAIN
  union doubles as footprint; less authoring, couples hurt tuning to
  navigation). Decide before the §5 data change.

## Regression guards (no behavior change for existing content)

Every step is inert for data that exists today; the guards below are
what make that claim checkable rather than assumed.

- **Mode precondition:** no live boss definition carries a `walkbox`
  key ⇒ every boss runs `normal`, defined as bit-identical arithmetic
  to today (§3 pin). This is the primary regression fence; the mode
  enum merely makes it data-visible.
- **Data precondition (audited 2026-11-24):** no ini-referenced boss
  or fx JSON contains type-0 boxes, and the only file that did was
  deleted from `cs3-data-private`; no JSON anywhere contains type-4.
  Therefore §1 changes nothing and §4 derivation has no input.
- **Derived-mode monotonicity (§4, `combined`/`custom` only):**
  derived footprint ⊇ anchor box at every chain stage, by
  construction; a test asserts margins never shrink the anchor box.
  The `static` mode is exempt — shrink/grow is authored intent,
  opt-in per boss, and hostile-pinned.
- **WALK inert outside motion:** tests assert type-4 boxes never
  reach `testHitbox1` results, `damage()`, contact/ice/barrel
  handling, or fx skill damage.
- **`canMove` is the single rerouted consumer** (list in §4); a diff
  touching any other `hitbox.w/h` consumer during this work is out of
  scope by definition.
- **Cap bump (§2) is data-inert:** every live frame holds ≤ 4 boxes;
  the clamp/LOGW hostile fixtures are updated to the new cap and keep
  asserting the clamp, just at the new number.
- **Fixture flips are the only allowed test-expectation edits** for
  the §1/§2 steps: `t_bossdata.cpp:380` (marker → carried) and the
  clamp-to-N fixtures. Full `ctest` green with no other expectation
  changes is the per-step proof (§ Verification).
- **Sequenced data risk:** §5 (Arachine shrink + `walkbox` mode + new
  JSONs) is the only step that changes live behavior, is external to
  this repo, and lands atomically; before it, Arachine plays exactly
  as it does today.
- **Stale staging:** `build/ems/ems_data` still holds the deleted
  untrimmed beholder JSON — restage before trusting a wasm smoke run
  as evidence.

## Verification (per AGENTS.md build/test regime)

1. `cmake -B build && cmake --build build`
2. `ctest --test-dir build` — full suite green after each of §1, §3, §4.
3. §1/§2 alone must produce **zero** behavior diff for legacy bosses
   (no `walkbox` key, no ini-referenced JSON carries MAIN or WALK
   boxes until the Arachine rewrite) — a green ctest run with
   unchanged `t_boss` expectations except the flipped fixture is the
   proof.
4. HW smoke: Demon/Beholder/Harpy + Arachine draw unchanged; Arachine
   walk-with-legs-in-wall reproduction closed by §4 + §5 data.
5. Docs: update `docs/spec/bosses` module text (`walkbox` key,
   `WALK` type, type-0 legality; parse.py untouched), then refresh
   the compliance hash/checkpoint rows only after the suites pass.

## Out of scope

- Footprint precision beyond walk frames (per-frame attack/hurt
  strides driving movement legality — rejected: never tighten or
  loosen movement mid-commit).
- Per-aim static rects (the `static` mode is one box for the whole
  sprite by contract; per-aim needs go through `custom`).
- Tile-mask hurtboxes (revisit only if box-per-leg leaves measurable
  dead space after the rewrite).
- Draw/culling path changes — full frame rect already draws legs over
  walls by design.
- `docs/spec/bosses/data/beholder1_attack.json` reference copy and
  external-tool copies: left in place; flag to `cs3-map-edit` owner
  that type-0 is no longer inert.
