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
**FIXED 2026-11-18 (0x0011 save-format bump):** `m_chuteTarget` now serialized after gameStats in `write()`/`read()` (pair, `ENGINE_VERSION` 0x0011 + `EXPECTED_VERSION` bumped together); covered by the `t_gamemixin` save roundtrip. - L430 (`m_chuteTarget`): [bug] `m_chuteTarget` is never initialized and never serialized in `write()` (L1327-1417) while `S_CHUTE` *is* persisted via `m_gameStats`; after a save during a chute transition, `loadLevel()` uses a stale/uninitialized target (the `isValid` check at L431 only partially mitigates garbage).
- L954-968: [bug] **FIXED 2026-11-18** (maintainer ruling: "EVENT_EXIT_OPENED should trigger only once"): new per-level `CGame::m_exitAnnounced` flag gates the push; reset in `loadLevel()` at `clearEvents()`. Chosen over the `if (!revealExit)` placement so pre-revealed exits still announce exactly once. Original finding: `checkClosure()` pushes `EVENT_EXIT_OPENED` on *every* invocation while the level is complete and the player has not reached the exit yet; since `addHealth()` (L927) calls `checkClosure()` on every water/fire/swamp step, the event queue is spammed with duplicate exit-opened events. Fix: move `m_events.emplace_back(EVENT_EXIT_OPENED)` inside the `if (!revealExit)` guard (and keep `S_REVEAL_EXIT` as the once-flag).
- L683: [bug] **FIXED 2026-11-18** (`CGame::sanitiseMapTiles`, game.cpp:414, runs immediately after the map copy in `loadLevel` (:471) and on restore (:1330); clamps every non-main layer cell to `< TOTAL_TILE_COUNT`, so the direct `g_layerdata[tileID]` read is in range). Original finding: `const layerdata_t &ld = g_layerdata[tileID];` indexes the global table with a raw layer tile ID and no `TOTAL_TILE_COUNT` bounds check (every other site routes through `m_animator->getLayerTile()` first, e.g. L1931, L1974). A malformed map file with an out-of-range layer tile gives an out-of-bounds read. Fix: use `getLayerTileDef(m_animator->getLayerTile(tileID))` or add a range check (needs verification that map load validates layer tile IDs).

### Low

- L1214 / L1222-1249: [ub] **FIXED 2026-11-18** (NOTE comment added above the signature read in `read()`; format stays LE/size-dependent by design, no `ISerial` migration — local save format only). Original: `reinterpret_cast<char *>(&signature)` plus wholesale raw-POD `_R` of `int`s and `sfx_t` makes the save format little-endian/size dependent.
- L1281 / L1371 / L1383 / L1395: [bug] **FIXED 2026-11-18 (0x0011 cluster):** all save counters now typed `uint32_t` locals on both write and read; the sfx count write keeps its `uint16_t` field but now `LOGW`s before truncating a >65535 list. Original finding: `size_t` counters written/read with an explicit `sizeof(uint32_t)` — works only on little-endian and silently halves the field.
- L1284-1286: [error-handling] **FIXED 2026-11-18** (used-items loop now `if (!pos.read(sfile)) { LOGE; return false; }`, matching the established `read()` failure pattern). Original: `pos.read(sfile)` return ignored; a failed read appends a garbage `Pos` to `m_usedItems`, later fed to `m_map.set()` (L411).
- L570-573: [error-handling] **FIXED 2026-11-18** (maintainer ruling: clamp+LOGW landed; clamps only when an archive is loaded, `loadLevel()` stays authoritative). `t_game::testLevelBounds` re-pinned from "setLevel must leak bad values into loadLevel" to the new clamp contract — the public-API injection path for the BUG-C7 `at()` guard is now sealed upstream by design. Original finding: `setLevel()` accepts any int with no range check against `m_mapArch->size()`; this was the enabler for the L407 crash.
- L1512-1515: [error-handling] **FIXED 2026-11-18** (`return m_mapArch ? m_mapArch->size() : 0;`). Original: `size()` dereferences `m_mapArch` without a null check.
- L1167-1170: [bug] **BY DESIGN 2026-11-18** (maintainer ruling: no production callers in `src/`; returning `CActor &` makes a guard signature-changing. Contract documented in `game.h`: caller owns bounds checking against `getMonsters().size()`). Original finding: `getMonster(int i)` does unchecked `m_monsters[i]`.
- L1522-1525: [bug] **FIXED 2026-11-18** (clamped at use, `m_introHint % m_hints.size()`). Original finding: `getHintText()` indexes `m_hints[m_introHint]` with a value computed in a previous `loadLevel()` (L419); if `parseHints()` is later re-run with fewer lines this is out of bounds (needs verification of call ordering). Fix: clamp at use: `m_introHint % m_hints.size()`.
- L1532-1535: [error-handling] **FIXED 2026-11-18** (null guard after `clear()`; the sole caller's `data.data()` can be null for an empty file). Original finding: `parseHints(nullptr)` constructs `std::string(nullptr)` — UB; add a null guard or document the contract.
- L426 / L441 / L954-955: [bug] **BY DESIGN 2026-11-18** (maintainer ruling: "0,0 is not a valid origin or exit"; documented in `map.h` at `toKey` — state key 0 = absent, (0,0) border-reserved). Original finding: `origin != 0` / `exitKey != 0` use key 0 as "absent" sentinel, which collides with the legitimate map position (0,0) (`toKey(0,0) == 0`). Low impact; document or use a separate `hasU()` probe.
- L837-852: [error-handling] **FIXED 2026-11-18** (`break`s became `return`s; fall-through of the slot scan now emits `LOGW("key inventory full, dropping key ...")`). Original: `addKey()` silently drops the key when all `MAX_KEYS` slots are occupied.
- L1661-1663: [bug] **FIXED 2026-11-18** (`timeout == 0` purges immediately instead of being decremented; live emplace sites all use nonzero `_TIMEOUT` constants — the reachable path was save-restore records). Original finding: `purgeSfx()` predicate has a side effect (`--sfx.timeout`); any entry with `timeout == 0` at purge time wraps to 65535 and is never removed. Guard with `sfx.timeout ? --sfx.timeout : 0` or `timeout <= 1`-style check (`timeout` is `uint16_t`, gamesfx.h:29).
- L184 vs L213: [bug] **FIXED 2026-11-18** (switch-branch local renamed to `switchTile`; outer is `uint16_t tileID` in current code). Original: inner `const uint16_t tileID` shadowed the outer `tileID` inside `move()` — benign but a trap.
- L1942-1949 vs L1979-1986: [bug] **PENDING RULE 2026-11-18** — asymmetry re-verified against current code: `scanPos()` (game.cpp ~:2088) pairs AIM_UP↔{DL,DR}, AIM_DOWN↔{UL,UR}, AIM_LEFT↔{UR,DR}, AIM_RIGHT↔{UL,DL}; `isGranularSolidFromPos()` (~:2125) is the exact mirror. Gameplay-intent question parked for maintainer ("approach side vs query side" vs copy-paste inversion). No code change until ruled.

### Shrink / Dead / YAGNI

- L597-613: [shrink] **FIXED 2026-11-18** (array is now `static constexpr`; the plain loop kept — `std::any_of` adds nothing at the same size). Original: `isMonsterType()` rebuilt the `std::array` per call, once per map tile in `spawnMonsters()`.
- L109: [dead] **ACCEPTED AS-IS 2026-11-18** — the destructor-body `reset()` releases the sound backend *before* the remaining members die; removing it changes release ordering for zero gain. Original: `m_sound.reset()` in `~CGame()` is redundant — the member destructor runs anyway.
- L1732-1735: [dead] **STALE/CLOSED 2026-11-18** — current `destroy()` does `s_instance.reset()` (real teardown); the audited "calls getGame()" body no longer exists.
- L345-347: [shrink] **FIXED 2026-11-18** (kept the single clear inside `manageTrigger()`; removed the redundant caller-side `setAttr(x, y, 0)` in both `consume()` and `move()`). Original: `manageTrigger()` re-did `m_map.setAttr(x, y, 0)` that callers already performed.
- L461: [yagni] **CLOSED 2026-11-18** — moot since BUG-C7: `loadLevel()` now has real failure paths (`m_mapArch`/`at()` null checks, `LOGE` + `return false` for out-of-archive levels). The bool is live.
- L1548-1557: [shrink] **FIXED 2026-11-18** (`!m_events.empty()` + `front()`). Original: `getEvent()` used `size() > 0` + `at(0)` where the throw-check bought nothing.
- L1806-1808: [shrink] **FIXED 2026-11-18** (`if (u) --u;`). Original: `u ? --u : 0;` used as a statement.
- L1470: [shrink] **FIXED 2026-11-18** (`attach(const std::shared_ptr<ISound> &)` in game.h + game.cpp; sole caller runtime.cpp:1511 passes an lvalue — signature-compatible).

## Notes

- Runtime verification needed for: (1) whether the L922 shield truncation is intended "shields = immunity" (check against originalCaptain Camel behavior or `t_game*` suites); (2) whether layer tile IDs are validated at map load, which determines whether L683 is reachable with crafted data; (3) the scanPos vs isGranularSolidFromPos granular-pair asymmetry; (4) `findFirst()` return value when the tile is absent (L433/L448 feed it straight into `CActor(pos, ...)`).
- Cross-file interplay: several members (`m_map`, `m_keys`) are `inline static` in game.h while the class is also a Meyers singleton (L1881-1885) — any second `CGame` would silently share map/keys state. `read()` restores `S_CHUTE` through `CGameStats` but not `m_chuteTarget` or `m_mode`; how the loader drives `loadLevel()` after a restore (gamemixin/engine) decides how visible that gap is.
- Save-format changes for the fixes above require an `ENGINE_VERSION` bump (L55) and a compliance-doc hash/tally update per AGENTS.md.
- No source files were modified; report only.
