# Audit: src/boss.cpp
Date: 2026-11-17

## Summary
Overall the file is compact and mostly correct, but the save/load path (`read()`) performs validation whose results are thrown away, and the constructor leaves `m_aim` uninitialized while accepting a potentially null `bossData_t*`. Frame indexing trusts data-driven `aims`/`base`/`lenght` values with no bounds guard, which is a latent crash at the render call sites. Counts: Critical 1, High 3, Medium 5, Low 7.

## Findings

### Critical
- L43–52: [bug/ub] **FIXED 2026-11-17** (docs/bugs/critical-triage-2026-11-17.md BUG-C6). Original finding: Constructor dereferences `data` unconditionally (`data->speed` L47, and via `setSolidOperator()` L51→L495 `m_bossData->type`) while boss.h L58 gives the parameter a default of `nullptr` — `CBoss()` or any call with a failed `getBossData()` result is a guaranteed null deref. Fixed by dropping the defaulted param (explicit defaulted ctor + member initializers serve the save-load resize), and `read()` now calls `setSolidOperator()`.

### High
- L43–52: [bug/ub] `m_aim` is never initialized in the constructor (only set by `move()` L230, `setAim()`, or `read()`). A freshly spawned boss rendered/animated before its first move reads an indeterminate `uint8_t` in `currentFrame()` (L309) → UB and a garbage frame index. Fix: initialize `m_aim = AIM_DOWN` (or `AIM_NONE` with the guard below) in the constructor.
- L309: [ub] `m_aim % m_bossData->aims` — `aims` comes from `bosses.ini` via `std::atoi` (bossdata.cpp:491) with no `>= 1` validation; `aims == 0` (missing/garbage key) is integer division by zero → UB/crash. Fix: `const int aims = m_bossData->aims > 0 ? m_bossData->aims : 1;` (and/or validate in the loader).
- L443, L445, L447, L451, L454, L458, L461: [error-handling] All seven `checkBound()` return values are ignored — out-of-range save-game values are logged and then accepted anyway. A corrupt `m_state` (L453–454) or oversized `m_framePtr` (L443) flows straight into `animate()`/`currentFrame()` and the renderers. Fix: `if (!checkBound(...)) return false;` at each site.

### Medium
- L304–312: [ub] `currentFrame()` returns `seq->base + seq->lenght * normalizedAim + m_framePtr` with no clamp against the actual frame-set size. Callers (`gamemixin.cpp:971`, `engine_hw.cpp:1024`) do `*(*frames)[num]` and `CFrameSet::operator[]` returns `nullptr` out of range → null deref on any mismatch between `bosses.ini` sequence tables and the JSON-derived frame counts. Fix: clamp to the sequence's frame count, or have the loader cross-check `base/lenght/aims` totals against the sheet and `LOGE` at load time. (needs verification of exact frames-per-sheet totals at runtime)
- L461: [bug] Off-by-one: `checkBound(static_cast<uint8_t>(m_aim), JoyAim::TOTAL_AIMS)` fails only on `a > b`, so `m_aim == 4` (not a valid aim; valid are 0–3) passes validation. Same inclusive-boundary ambiguity for `MAX_FRAME`/`MAX_POS`/`MAX_HP`. Fix: pass `TOTAL_AIMS - 1` or make `checkBound` fail on `>=`.
- L441–458, L479–485: [bug] `m_framePtr`, `m_hp`, `m_speed` are `int` but serialized as 2 bytes (`_R(&m_hp, DATA_SIZE)`); correctness relies on little-endian plus the pre-zeroing at L449/456 (and on `m_hp` never exceeding 65535 at L482, which is unchecked — `maxHp()` can exceed it on high `hp`/skill). Comments at L444/446 say "uint16_t" but `m_x`/`m_y` are `int16_t`. Fix: serialize via `uint16_t` temp variables. (needs verification of save-format expectations)
- L111–114: [bug] Comments claim `floor()` but C++ integer division truncates toward zero, so negative half-tile coordinates (possible from data-driven `c.x - hbMain.x` offsets at L91–92, or a negative `m_x` via unchecked `move(x,y)` from boss.h L89) compute the wrong tile range (e.g. `right == 0` gives `(right-1)/2 == 0`, scanning a spurious tile 0). Fix: arithmetic-shift or an explicit floor-div helper. (needs verification)
- L284–301: [bug] In `animate()`, when `maxFrames == 0` the "sanity check" resets `m_framePtr`, then `m_framePtr >= maxFrames` (`0 >= 0`) is true, so the Hurt→Patrol / Attack→Chase / Death→Hidden transitions run anyway — the check does not do what it reads as. Reachable when `read()` loads a state (L453 bypasses `setState`) whose seq has zero length. Fix: `if (maxFrames <= 0) { m_framePtr = 0; return; }` or gate the transition block.
- L62–70: [shrink] `isGhostBlocked()` calls `game->scanPos()` (a line scan) for every candidate tile inside the `canMove()` tile loop (L194–206) → O(tiles × scan) per boss per tick, just to read `result.isWater`. Fix: derive water from the tile def already fetched at L68, or cache. (needs runtime cost verification)

### Low
- L495–508: [shrink] Four branches of `setSolidOperator()` all assign `&CBoss::isSolid`; the whole chain is `m_solidCheck = (m_bossData->type == BOSS_GHOST) ? &CBoss::isGhostBlocked : &CBoss::isSolid;`.
- L495: [error-handling] `LOGI` fires on every boss construction *and* every `read()` — save/load log spam; demote or remove.
- L323–327: [shrink] `maxHp()` uses floating point (`skill * 0.5`) for integer math; `hp * (2 + skill) / 2` avoids the truncation-on-double ambiguity.
- L243: [shrink] `return std::sqrt(...)` silently truncates double→int; `std::lround` or a squared-distance comparison at call sites would be more honest. (callers compare against `distance_*` thresholds — truncation is likely tolerable)
- L329: [dead] Misspelled API `subtainDamage` (sustain) — rename while touching; also L332 allows negative `lostHP` to push `m_hp` above `maxHp()` unclamped (no current caller does this).
- L387–468: [error-handling] A mid-stream `_R` failure leaves the boss half-loaded (old x/y already overwritten with new/partial values, no rollback); acceptable for a fatal load error, but worth a `LOGE("boss save corrupt")` context line.
- L72–74: [shrink] `hitboxTestCallback_t`/`hitboxActionCallback_t` taken by value → two `std::function` allocations per call; take by `const&`.
- L365–368: [bug] `toPos(int, int)` silently narrows to `int16_t`; add an assert or clamp.

## Notes
- `docs/spec` compliance: `read()` validation being decorative (High #3) undermines any "save-game validation" claim; flip the corresponding compliance item to Open until the returns are honored.
- Runtime verification wanted: (1) whether any boss can be rendered before its first `move()`/`setAim()` (makes the uninitialized `m_aim` observable, not just latent); (2) total frame counts per boss sheet vs. `base + lenght*aims` maxima from `bosses.ini` (determines whether the unclamped `currentFrame()` is reachable); (3) whether `BossState::Flee` (boss.h L52) is ever set — nothing in the game AI greps calls `setState(Flee)`, and `getCurrentSeq()` has no `Flee` case (L270–271 returns `nullptr`), so the state appears dead.
- `distance()` (L241–242) correctly mixes actor tile units with the boss's half-tile coordinates via `/ BOSS_GRANULAR_FACTOR`; precedence is fine.
- `patrol()` `rng.range(0, 4)` is `[min, max)` per randomz.h L31 — the `dirs[]` index is safe.
- `MAX_POS`/`MAX_HP` bounds in `read()` are save-format ceilings, not runtime invariants (`setHP()` in boss.h L74 is unchecked).
