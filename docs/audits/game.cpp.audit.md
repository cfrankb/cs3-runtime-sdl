# Audit: src/game.cpp

Date: 2026-11-16

## Summary

`src/game.cpp` (CGame core: level load, movement, consumables, stats, save/load) is generally readable and consistently uses the `_R`/`_W` error macros, but has one crash path (null deref of `CMapArch::at()` with unvalidated `m_level`), three high-severity issues (UB over-read in `validateSignature`, a shield-damage truncation bug, and a monster-grid entry lost on failed move), plus a cluster of save/load robustness gaps. Counts: 1 Critical, 3 High, 6 Medium, 12 Low, 8 shrink/dead/yagni notes.

## Findings

### Critical

- L407: [bug] **FIXED 2026-11-17** (docs/bugs/resolved/critical-triage-2026-11-17.md BUG-C7; `testLevelBounds`). Original finding: `m_map = *(m_mapArch->at(m_level));` — `CMapArch::at()` (maparch.h:41) returns `nullptr` for out-of-range indices, so an out-of-range `m_level` (settable unchecked via `setLevel()` L570-573, or read raw from a save file at L1224 with no bound check) causes a null-pointer dereference. Also `m_mapArch` itself is never null-checked here. Fixed: `loadLevel()` checks `m_mapArch` and `at()` result before mutating state; `read()` validates `m_level` against `m_mapArch->size()` when an archive is loaded.

### High

**FIXED 2026-11-17 (BUG-H10):** LOGW prints `tmp`. **L1176-1184: [bug][ub] `validateSignature()` builds NUL-terminated `tmp` but never uses it: `LOGW(... "%s" ..., signature, ...)` prints the caller's non-NUL-terminated 4-byte buffer with `%s` — reads past the buffer (UB). Fix: pass `tmp` to `LOGW` (that is what it was built for); or use `std::string_view(signature, sizeof(GAME_SIGNATURE))`.
**FIXED 2026-11-17 (BUG-H10):** integer math; `testShieldDamage`. **L922-924: [bug] `const int hpToken = (int)damageFactor * (hp * (1 + 1 * skill));` — the cast binds to `damageFactor` only: with 1 shield the factor is 0.75 and with 2 shields 0.5, both truncating to 0, so any shield makes the player fully immune to damage while still playing SOUND_OUCHFAST (L925). Almost certainly intended `(int)(damageFactor * (hp * (1 + 1 * skill)))`. Fix (needs intent verification): parenthesize the product, or use integer math `hp * (MAX_FACTOR - shields) / MAX_FACTOR * (1 + skill)`.
**FIXED 2026-11-17 (BUG-H10, defense-in-depth):** grid entry restored on failure; window unreachable via `translate()` clamping. **L1871-1886: [bug] `shadowActorMove()` erases the actor's `m_monsterGrid` entry (L1876) *before* checking `m_map.isValid(newPos)`; if the target is off-map it returns false with the entry gone — the actor stays in `m_monsters` but is invisible to `findMonsterAt()`/collisions until some later `rebuildMonsterGrid()`. Fix: validate `newPos` first and only erase/reinsert when the move will succeed (or reinsert at `oldKey` on the failure path).

### Medium

**FIXED 2026-11-18 (maintainer option 1): both lists now gated by
`CGame::countWithinBudget(count, getSize()-tell(), MIN_*_RECORD)` before
`resize` — corrupt counts are rejected with LOGE instead of a `bad_alloc`
spike. Validation-only: no serialized fields touched, no `ENGINE_VERSION`
bump.**
- L1250-1261 / L1265-1276: [bug][error-handling] `m_monsters.resize(actorCount)` and `m_bosses.resize(bossCount)` with counts read straight from the save file, no sanity bound — a corrupt save with a huge count triggers a massive allocation / uncaught `std::bad_alloc`. Fix: read each element and `push_back`, or bound the count against remaining file size.
**FIXED 2026-11-18: boss count now read via `_R`; truncated saves return
false. Check pinned by `CGame::countWithinBudget` cases in
`tests/t_game.cpp`.**
- L1266: [error-handling] `readfile(&bossCount, sizeof(uint32_t));` return value is the only I/O result in `read()` not wrapped in `_R` — a truncated file silently continues with `bossCount == 0` and `read()` returns true. Fix: use `_R(&bossCount, sizeof(bossCount))`.
**FIXED 2026-11-18 (BUG-H18):** `m_mode = MODE_TITLE`, `m_report = {}` in header. - game.h:221/234 (constructed at L89-101): [bug] `m_mode` and `m_report` have no default initializer and are not set in the constructor; `mode()` / `originalMapReport()` before the first `setMode()`/`loadLevel()` read uninitialized values.
**PARTIALLY FIXED 2026-11-18 (BUG-H18):** `m_chuteTarget = {}` default-init added; serialization still missing (deferred — save-format change needs `ENGINE_VERSION` bump). - L430 (`m_chuteTarget`): [bug] `m_chuteTarget` is never initialized and never serialized in `write()` (L1327-1417) while `S_CHUTE` *is* persisted via `m_gameStats`; after a save during a chute transition, `loadLevel()` uses a stale/uninitialized target (the `isValid` check at L431 only partially mitigates garbage).
- L954-968: [bug] `checkClosure()` pushes `EVENT_EXIT_OPENED` on *every* invocation while the level is complete and the player has not reached the exit yet; since `addHealth()` (L927) calls `checkClosure()` on every water/fire/swamp step, the event queue is spammed with duplicate exit-opened events. Fix: move `m_events.emplace_back(EVENT_EXIT_OPENED)` inside the `if (!revealExit)` guard (and keep `S_REVEAL_EXIT` as the once-flag).
- L683: [bug] `const layerdata_t &ld = g_layerdata[tileID];` indexes the global table with a raw layer tile ID and no `TOTAL_TILE_COUNT` bounds check (every other site routes through `m_animator->getLayerTile()` first, e.g. L1931, L1974). A malformed map file with an out-of-range layer tile gives an out-of-bounds read. Fix: use `getLayerTileDef(m_animator->getLayerTile(tileID))` or add a range check (needs verification that map load validates layer tile IDs).

### Low

- L1214 / L1222-1249: [ub] `reinterpret_cast<char *>(&signature)` plus wholesale raw-POD `_R` of `int`s and `sfx_t` makes the save format little-endian/size dependent. Acceptable as a local save format; note it explicitly or use `ISerial` helpers if cross-platform saves ever matter.
- L1281 / L1371 / L1383 / L1395: [bug] `size_t` counters written/read with an explicit `sizeof(uint32_t)` — works only on little-endian and silently halves the field; use a local `uint32_t` temp instead. L1401: `(uint16_t)m_sfx.size()` silently truncates a >65535-entry sfx list.
- L1284-1286: [error-handling] `pos.read(sfile)` return ignored; a failed read appends a garbage `Pos` to `m_usedItems`, later fed to `m_map.set()` (L411). Fix: `_R`-style check or skip on failure.
- L570-573: [error-handling] `setLevel()` accepts any int with no range check against `m_mapArch->size()`; this is the enabler for the L407 crash. Fix: clamp/log here.
- L1512-1515: [error-handling] `size()` dereferences `m_mapArch` without a null check (returns 0 or `LOGE` is cheap).
- L1167-1170: [bug] `getMonster(int i)` does unchecked `m_monsters[i]`; add the same bounds guard `deleteMonster()` (L1838) already has.
- L1522-1525: [bug] `getHintText()` indexes `m_hints[m_introHint]` with a value computed in a previous `loadLevel()` (L419); if `parseHints()` is later re-run with fewer lines this is out of bounds (needs verification of call ordering). Fix: clamp at use: `m_introHint % m_hints.size()`.
- L1532-1535: [error-handling] `parseHints(nullptr)` constructs `std::string(nullptr)` — UB; add a null guard or document the contract.
- L426 / L441 / L954-955: [bug] `origin != 0` / `exitKey != 0` use key 0 as "absent" sentinel, which collides with the legitimate map position (0,0) (`toKey(0,0) == 0`). Low impact; document or use a separate `hasU()` probe.
- L837-852: [error-handling] `addKey()` silently drops the key when all `MAX_KEYS` slots are occupied; add a `LOGW`.
- L1661-1663: [bug] `purgeSfx()` predicate has a side effect (`--sfx.timeout`); any entry with `timeout == 0` at purge time wraps to 65535 and is never removed. Guard with `sfx.timeout ? --sfx.timeout : 0` or `timeout <= 1`-style check (`timeout` is `uint16_t`, gamesfx.h:29).
- L184 vs L213: [bug] inner `const uint16_t tileID` (L213) shadows the outer `const uint8_t tileID` (L184) inside `move()`; the shadow is currently benign (the switch branch re-reads deliberately) but is a trap — rename to `switchTile`.
- L1942-1949 vs L1979-1986: [bug] the granular corner-pair mappings in `scanPos()` and `isGranularSolidFromPos()` differ for the same `aim` (e.g. AIM_UP tests {DL,DR} vs {UL,UR}); one is a plausible copy-paste inversion (needs gameplay verification — the difference may be intentional "approach side vs query side").

### Shrink / Dead / YAGNI

- L597-613: [shrink] `isMonsterType()` rebuilds a `std::array` and hand-rolls a linear scan on every call, once per map tile in `spawnMonsters()`. Make it `static constexpr std::array` + `std::any_of` (or `std::find`).
- L109: [dead] `m_sound.reset()` in `~CGame()` is redundant — the member destructor runs anyway.
- L1732-1735: [dead] `destroy()` just calls `getGame()` — a no-op that claims to destroy the singleton (the function-local `unique_ptr` at L1883 only dies at exit). Either reset the unique_ptr or delete `destroy()`.
- L345-347: [shrink] `manageTrigger()` re-does `m_map.setAttr(x, y, 0)` that caller `consume()` (L341) already performed; drop one.
- L461: [yagni] `loadLevel()` always returns `true` — the bool is dead flexibility until the L407 path actually validates; once error handling lands it becomes real (keep after fixing Critical).
- L1548-1557: [shrink] `getEvent()` can be `event = m_events.front();` (`at(0)` + throw-check buys nothing after the `size() > 0` test).
- L1806-1808: [shrink] `u ? --u : 0;` used as a statement — write `if (u) --u;`.
- L1470: [shrink] `attach(std::shared_ptr<ISound> &s)` takes a non-const reference for no reason — take by value (or `const &`).

## Notes

- Runtime verification needed for: (1) whether the L922 shield truncation is intended "shields = immunity" (check against originalCaptain Camel behavior or `t_game*` suites); (2) whether layer tile IDs are validated at map load, which determines whether L683 is reachable with crafted data; (3) the scanPos vs isGranularSolidFromPos granular-pair asymmetry; (4) `findFirst()` return value when the tile is absent (L433/L448 feed it straight into `CActor(pos, ...)`).
- Cross-file interplay: several members (`m_map`, `m_keys`) are `inline static` in game.h while the class is also a Meyers singleton (L1881-1885) — any second `CGame` would silently share map/keys state. `read()` restores `S_CHUTE` through `CGameStats` but not `m_chuteTarget` or `m_mode`; how the loader drives `loadLevel()` after a restore (gamemixin/engine) decides how visible that gap is.
- Save-format changes for the fixes above require an `ENGINE_VERSION` bump (L55) and a compliance-doc hash/tally update per AGENTS.md.
- No source files were modified; report only.
