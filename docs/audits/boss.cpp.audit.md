# Audit: src/boss.cpp
Date: 2026-11-17

## Summary
Overall the file is compact and mostly correct, but the save/load path (`read()`) performs validation whose results are thrown away, and the constructor leaves `m_aim` uninitialized while accepting a potentially null `bossData_t*`. Frame indexing trusts data-driven `aims`/`base`/`lenght` values with no bounds guard, which is a latent crash at the render call sites. Counts: Critical 1, High 3, Medium 5, Low 7.

## Findings

### Critical
- L43–52: [bug/ub] **FIXED 2026-11-17** (docs/bugs/resolved/critical-triage-2026-11-17.md BUG-C6). Original finding: Constructor dereferences `data` unconditionally (`data->speed` L47, and via `setSolidOperator()` L51→L495 `m_bossData->type`) while boss.h L58 gives the parameter a default of `nullptr` — `CBoss()` or any call with a failed `getBossData()` result is a guaranteed null deref. Fixed by dropping the defaulted param (explicit defaulted ctor + member initializers serve the save-load resize), and `read()` now calls `setSolidOperator()`.

### High
**FIXED 2026-11-17 (BUG-C6):** default member initializer `m_aim = AIM_NONE`. **L43–52: [bug/ub] `m_aim` is never initialized in the constructor (only set by `move()` L230, `setAim()`, or `read()`). A freshly spawned boss rendered/animated before its first move reads an indeterminate `uint8_t` in `currentFrame()` (L309) → UB and a garbage frame index. Fix: initialize `m_aim = AIM_DOWN` (or `AIM_NONE` with the guard below) in the constructor.
**FIXED 2026-11-17 (BUG-H6a):** `aims <= 0` guard in `currentFrame()`. **L309: [ub] `m_aim % m_bossData->aims` — `aims` comes from `bosses.ini` via `std::atoi` (bossdata.cpp:491) with no `>= 1` validation; `aims == 0` (missing/garbage key) is integer division by zero → UB/crash. Fix: `const int aims = m_bossData->aims > 0 ? m_bossData->aims : 1;` (and/or validate in the loader).
**FIXED 2026-11-17 (BUG-H6a):** all seven results now enforced. **L443, L445, L447, L451, L454, L458, L461: [error-handling] All seven `checkBound()` return values are ignored — out-of-range save-game values are logged and then accepted anyway. A corrupt `m_state` (L453–454) or oversized `m_framePtr` (L443) flows straight into `animate()`/`currentFrame()` and the renderers. Fix: `if (!checkBound(...)) return false;` at each site.

### Medium
- L304–312: [ub] `currentFrame()` returns `seq->base + seq->lenght * normalizedAim + m_framePtr` with no clamp against the actual frame-set size. Callers (`gamemixin.cpp:971`, `engine_hw.cpp:1024`) do `*(*frames)[num]` and `CFrameSet::operator[]` returns `nullptr` out of range → null deref on any mismatch between `bosses.ini` sequence tables and the JSON-derived frame counts. Fix: clamp to the sequence's frame count, or have the loader cross-check `base/lenght/aims` totals against the sheet and `LOGE` at load time. (needs verification of exact frames-per-sheet totals at runtime)
**RESOLVED since BUG-C6-era triage: aim is validated with `>=` and the
documented `AIM_NONE` exception (`boss.cpp:475`); the other sites keep the
documented inclusive `MAX_*` sanity bounds. Stale as of 2026-11-18 review.**
- L461: [bug] Off-by-one: `checkBound(static_cast<uint8_t>(m_aim), JoyAim::TOTAL_AIMS)` fails only on `a > b`, so `m_aim == 4` (not a valid aim; valid are 0–3) passes validation. Same inclusive-boundary ambiguity for `MAX_FRAME`/`MAX_POS`/`MAX_HP`. Fix: pass `TOTAL_AIMS - 1` or make `checkBound` fail on `>=`.
- L441–458, L479–485: [bug] `m_framePtr`, `m_hp`, `m_speed` are `int` but serialized as 2 bytes (`_R(&m_hp, DATA_SIZE)`); correctness relies on little-endian plus the pre-zeroing at L449/456 (and on `m_hp` never exceeding 65535 at L482, which is unchecked — `maxHp()` can exceed it on high `hp`/skill). Comments at L444/446 say "uint16_t" but `m_x`/`m_y` are `int16_t`. Fix: serialize via `uint16_t` temp variables. (needs verification of save-format expectations)
- L111–114: [bug] Comments claim `floor()` but C++ integer division truncates toward zero, so negative half-tile coordinates (possible from data-driven `c.x - hbMain.x` offsets at L91–92, or a negative `m_x` via unchecked `move(x,y)` from boss.h L89) compute the wrong tile range (e.g. `right == 0` gives `(right-1)/2 == 0`, scanning a spurious tile 0). Fix: arithmetic-shift or an explicit floor-div helper. (needs verification)
- L284–301: [bug] **FIXED 2026-11-18** — `animate()` now early-returns (`m_framePtr = 0`) when `maxFrames <= 0`; the transitions can no longer fire through the `0 >= 0` fallthrough. Original finding: when `maxFrames == 0` the "sanity check" reset `m_framePtr`, then `m_framePtr >= maxFrames` was true, so the Hurt→Patrol / Attack→Chase / Death→Hidden transitions ran anyway. Reachable when `read()` loads a state whose seq has zero length.
- L62–70: [shrink] `isGhostBlocked()` calls `game->scanPos()` (a line scan) for every candidate tile inside the `canMove()` tile loop (L194–206) → O(tiles × scan) per boss per tick, just to read `result.isWater`. Fix: derive water from the tile def already fetched at L68, or cache. (needs runtime cost verification)

### Low
**DECLINED 2026-11-18 (maintainer ruling — restored byte-identical): the chain is future-proofing — "coded that way to make it easier to extend later should there be newer operators added", one named branch per boss type as the wiring point. A DO-NOT-collapse comment now guards the function. - L495–508: [shrink] Four branches of `setSolidOperator()` all assign `&CBoss::isSolid`; the whole chain is `m_solidCheck = (m_bossData->type == BOSS_GHOST) ? &CBoss::isGhostBlocked : &CBoss::isSolid;`.
- L495: [error-handling] **DECLINED 2026-11-18 (same ruling)** — `LOGI` stays with the branch chain.
- L323–327: [shrink] **FIXED 2026-11-18** — `maxHp()` is now `hp * (2 + skill) / 2` (integer; identical results for all non-negative inputs, verified against the old fp form's floor).
- L243: [shrink] **DECLINED 2026-11-18** — `sqrt` truncation left as-is; the audit itself rates it tolerable and callers only compare against `distance_*` thresholds.
- L329: [dead] **PARTIAL 2026-11-18** — negative-`lostHP` heal fixed (`std::max(lostHP, 0)` guard; no caller passes negatives today). Misspelling `subtainDamage` NOT renamed: 6 sites across 4 files including test comments — separate rename concern, needs maintainer OK on the API name.
- L387–468: [error-handling] **CLOSED 2026-11-18 — already covered**: the `_R` macro (`filemacros.h:30`) already `LOGE`s the failing field name/size/line and returns false at every site; the half-load-without-rollback half was rated acceptable by the audit itself.
- L72–74: [shrink] **FIXED 2026-11-18** — `testHitbox1` takes both callbacks by `const &` (boss.h:68 + boss.cpp); call sites unchanged.
- L365–368: [bug] **FIXED 2026-11-18** — `toPos` clamps to `INT16_MIN/MAX` before the cast instead of wrapping.

## Notes
- `docs/spec` compliance: `read()` validation being decorative (High #3) undermines any "save-game validation" claim; flip the corresponding compliance item to Open until the returns are honored.
- Runtime verification wanted: (1) whether any boss can be rendered before its first `move()`/`setAim()` (makes the uninitialized `m_aim` observable, not just latent); (2) total frame counts per boss sheet vs. `base + lenght*aims` maxima from `bosses.ini` (determines whether the unclamped `currentFrame()` is reachable); (3) whether `BossState::Flee` (boss.h L52) is ever set — nothing in the game AI greps calls `setState(Flee)`, and `getCurrentSeq()` has no `Flee` case (L270–271 returns `nullptr`), so the state appears dead.
- `distance()` (L241–242) correctly mixes actor tile units with the boss's half-tile coordinates via `/ BOSS_GRANULAR_FACTOR`; precedence is fine.
- `patrol()` `rng.range(0, 4)` is `[min, max)` per randomz.h L31 — the `dirs[]` index is safe.
- `MAX_POS`/`MAX_HP` bounds in `read()` are save-format ceilings, not runtime invariants (`setHP()` in boss.h L74 is unchecked).
