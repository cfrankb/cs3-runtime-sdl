# Action Plan: Poison Damage (S_POISON), Poison Tint, Poison Aura Draw

Status: **Approved for implementation** (2026-11-20). Phase 0 decisions
ruled by maintainer 2026-11-20; integration points verified against `src/`
at working-tree state of 2026-11-20.

Feature: the player carries a pool of damage-to-be-drained (`S_POISON`) with
a drain clock (`S_POISON_NEXT`), a palette tint while poisoned, and a
two-layer aura **drawn** centered on the player whenever the pool is
nonzero (no follow machinery — see D7).

## Phase 0 — Ruled decisions (maintainer, 2026-11-20)

| # | Ruling |
|---|--------|
| D1 | **Forfeit model.** No pre-gating at the drain site: every `POISON_INTERVAL` the pool is decremented and `addHealth` is called; damage swallowed by god mode or i-frames is forfeited with the pool deduction. Documented behavior, not a bug. |
| D2 | **Normal damage.** Drains route through `addHealth` so `S_SHIELD` mitigation and `S_SKILL` scaling apply; shields reduce the total poison damage taken. |
| D3 | **Silent drain.** `CGame::addHealth` gains `bool silent = false`; poison passes `true` to skip `SOUND_OUCHFAST`. Default keeps every existing call site unchanged. |
| D4 | Poison cleared on death **and** level load: both keys go in `resetStats()` only (`src/game.cpp:615-629`). |
| D5 | First cut ships the **entry point only**: `CGame::addPoison(int amount)`. Wiring via `FLAG_POISON` / `FLAG_ANTIPOISON` tile flags (already declared at `src/sprtypes.h:45-46`, consumers zero as of 2026-11-20 — reserved vocabulary) drops straight into `consume()`; attr, mob (`AI_POISON`, `src/sprtypes.h:33`) and boss wiring stay deferred. |
| D6 | Aura uses **both layers** (`back` + `front`) of the fx table entry. |
| D7 | **Derived draw, no instance, no `FX_FOLLOW`.** "Follow" means *always drawn centered on the player*: no `fx_t` instance is spawned and no per-tick re-anchoring exists. `getFxToDraw()` emits the aura draw entries iff `S_POISON != 0`, anchored at player-center px, looping frames off a tick clock. (Withdrawn: the earlier `FX_FOLLOW` flag proposal — the need it served no longer exists.) |
| D8 | **No `ENGINE_VERSION` bump** (maintainer ruling; maintainer of record accepts the deviation from BUG-H12 discipline: `CGameStats` is a self-describing zero-suppressed kv map, `src/gamestats.cpp:181-206`, old saves load with `S_POISON == 0` and unknown keys are re-saved harmlessly). `EXPECTED_VERSION` in `tests/t_game.cpp:52` stays untouched. |

Constants (`GamePrivate` enum block, `src/game.cpp:73-94`):
`POISON_INTERVAL` (ticks between drains), `POISON_DRAIN` (HP per drain,
pre-shield), `POISON_DEFAULT` (pool added by one `addPoison` default call).
**Tuning constraint:** an applied drain re-arms the i-frame window (≥3
stages × 3-tick decay ≈ 9+ game ticks, `game.h:95-101`,
`gamemixin.cpp:1429-1448`); drains scheduled inside it are forfeited, so
effective max damage rate is one drain per `max(POISON_INTERVAL, ~12)`.
Set `POISON_INTERVAL` ≥ ~12 or accept forfeits by design.

## Phase 1 — State + entry point + drain

1. `src/gamestats.h:46` — append `S_POISON, S_POISON_NEXT` **last** in
   `enum GameStat` (keys serialize raw; inserting mid-list corrupts saves).
2. `src/game.h:140` / `src/game.cpp` —
   `void addHealth(const int hp, const bool silent = false);` (D3):
   gate `playSound(SOUND_OUCHFAST)` on `!silent`. All existing call sites
   compile unchanged.
3. `src/game.h` / `src/game.cpp` (near `addHealth`) —
   `void addPoison(const int amount)`:
   `m_gameStats->get(S_POISON) += amount;`
   `if (!m_gameStats->get(S_POISON_NEXT)) set(S_POISON_NEXT, POISON_INTERVAL);`
   `m_events.emplace_back(EVENT_POISON);` public (tests + future wiring).
4. `src/game.cpp` `decTimers()` (:594-607) — drain block appended:
   ```cpp
   int &poison = m_gameStats->get(S_POISON);
   if (poison > 0 && !m_gameStats->dec(S_POISON_NEXT))
   {
       const int drain = std::min(POISON_DRAIN, poison);
       poison -= drain;                      // forfeited if addHealth swallows (D1)
       addHealth(-drain, /*silent=*/true);   // D2/D3
       if (poison)
           m_gameStats->set(S_POISON_NEXT, POISON_INTERVAL);
   }
   ```
   `dec()` clamps at 0 (`src/gamestats.cpp:69-76`), so zero-crossing is
   the trigger. Death by poison is automatic (`addHealth` → `checkClosure`).
5. `src/game.cpp` `resetStats()` (:615-629) — both keys cleared (D4).
6. `src/game.cpp` `consume()` (:319-348) — **flag wiring, unblocked by
   existing vocabulary**: two standalone `if` blocks **after** the
   god/speed/rage `else-if` chain (poison is not mutually exclusive with
   them):
   - `if (def.flags & FLAG_POISON)` → `addPoison(POISON_DEFAULT);`
   - `if (def.flags & FLAG_ANTIPOISON)` → `m_gameStats->set(S_POISON, 0);
     set(S_POISON_NEXT, 0);` (full cure ruling pending — see Deferred).
   Consumed via the same PU path as `FLAG_RAGE`; no tile ids hardcoded
   anywhere.
7. `src/game.cpp` near `isRageMode()` (:1956) — `bool isPoisoned() const`.
8. `src/events.h:21-33` — append `EVENT_POISON` after `EVENT_BOAT`.
   `EVENT_ANTIPOISON` optional, only if the HUD wants a cue.

Gate: `cmake -B build && cmake --build build && ctest --test-dir build`.

## Phase 2 — Poison tint (twin edits mandatory)

1. `src/colormasks.h:21-30` — append `COLOR_POISON` to `enum ColorMask`
   (render-only).
2. `src/colormap.h/.cpp` — `poison` colormap beside `godMode`/`rage`
   (green-shifted; copy rage map structure).
3. `src/engine_hw.cpp:611-629` — append `else if (m_game->isPoisoned())
   colorMask = COLOR_POISON;` at the **end** of the status chain
   (hurt/frozen/sugar/god/rage keep priority).
4. `src/gamemixin.cpp:655-669` — identical branch with
   `colorMap = &m_colormaps.poison;`.
5. No sprite-sheet change (palette tint, per god-mode/rage precedent).

## Phase 3 — Poison aura: derived two-layer draw (D6/D7)

No `fx_t` instance, no `FX_FOLLOW`, no spawn, no wire-format change.

Art landed (2026-11-20): imageID runs 65-76 on the fx sheet, continuing
the contiguous run after `FX_BOMB_EXPLOSION` front 57..64
(`src/gamesfx.cpp:44`): **front 65..70 (6 frames), back 71..76
(6 frames), 32x32** (`..._Poisioned_EffectFront.png` / `_EffectBack.png`).

1. `src/gamesfx.h:80-89` — append `FX_POISON_AURA` after
   `FX_BOMB_EXPLOSION`, before `MAX_FX` (id space stable; this effect is
   never instantiated so its id never travels the wire).
2. `src/gamesfx.cpp:24-45` (`g_fxData`) — new entry, matching the
   two-layer `FX_EXPLOSION` pattern (back drawn first, per spec):
   ```cpp
   {.id = FX_POISON_AURA, .sheet = 0, .speed = 6, .flags = 0,
    .ttl = 0, .w = 32, .h = 32, .back = {71, 6}, .front = {65, 6},
    .onEnd = nullptr},
   ```
   `flags = 0`: instance lifecycle flags irrelevant — the entry is a
   draw recipe only. `speed = 6` matches the table's norm → 36-tick loop.
3. `src/game.cpp` — one non-serialized anim clock: `uint16_t m_fxTick;`
   member, `++` in `decTimers()`; derive frame the spec-I4 way:
   `idx = (m_fxTick / def->speed) % layer.count`.
4. `src/game.cpp` `getFxToDraw()` (:1928-1944) — appended special case:
   if `statsConst().at(S_POISON)` and the aura entry exists, compute
   player-center px (`pos * TILE_SIZE + TILE_SIZE/2`), run the same
   view-cull as regular fx, and emit `fxDraw_t` for `front`/`back` per
   the `front` parameter. Player-centered by construction: it *is* at
   the player every frame, so nothing follows anything.
   **Staged-rollout guard:** skip emitting unless
   `g_fxSheet_data.size() > 76` — an unextended private sheet otherwise
   LOGW-spams `CFrameSet::operator[]` (`src/shared/FrameSet.cpp:549`)
   every frame while poisoned. Null `m_fxSheet` already no-ops
   (`gamemixin.cpp:738-740`, `gamemixin.h:262`).
5. **Out-of-repo asset task** (gate for enabling, not for code):
   `cs3-data-private/pixels/sfx.idx` + `sfx.png` must grow the 12 frames
   and their obLT meta records (version-2 chunk, appended in imageID
   order, `src/gamesfx.h:179-181`). `data/` is a symlink absent in CI —
   the guard in item 4 keeps every environment green until it lands.
6. Behavior notes (document, don't fix): aura frame clock freezes with
   `decTimers()` under the pause/menu paths that skip it — cosmetic;
   aura appears/disappears instantly with the pool, nothing to fade.
7. Skipped: `FX_FOLLOW` flag + `tickFx()` re-anchoring (superseded by
   D7), generic "attach fx to entity" (YAGNI — one consumer that is
   positional by definition).

## Phase 4 — Tests + docs

1. `tests/t_game.cpp` — `addPoison(POISON_DEFAULT)` then run
   `decTimers()` POISON_INTERVAL×N ticks: HP drops by `min(drain, pool)`
   per interval with shield/skill scaling (D2), pool hits 0 exactly,
   `isPoisoned()` flips false; god mode → pool still drains, HP frozen
   (pins the D1 forfeit ruling); `addHealth` overload default-arg leaves
   sound path of legacy calls unchanged. No version constant changes (D8).
2. `tests/t_gamestats.cpp` — new-key round-trip; legacy record (no keys)
   loads `S_POISON == 0`.
3. `tests/t_game.cpp` — flag-wiring check per the `t_game_ai.cpp`
   table-scan rule: synthesize a PU carrying `FLAG_POISON`, consume,
   assert pool grew by `POISON_DEFAULT`; same with `FLAG_ANTIPOISON`,
   assert pool and clock zeroed; assert a flag carrying both keeps the
   documented consume order.
4. `tests/t_fx.cpp` — `getFxToDraw` emits aura entries for **both** layer
   calls iff `S_POISON != 0`, anchored at player-center px; none when
   pool is 0; existing instance-fx draw/save tests pass **unchanged**.
5. `python bin/refresh.py`, then
   `cmake -B build && cmake --build build && ctest --test-dir build`.
6. Spec: poison rules → new module file under `docs/spec/` (entry via
   `docs/spec/README.md`); aura special case + `m_fxTick` clock →
   amendment in `docs/spec/gamespec/extended-fx.md` (draw-entry class:
   emitted per-frame, never instantiated, never serialized). Flip
   compliance open items only after tests pass; update hashes last
   (AGENTS.md: testing proves, hash detects).
7. Move this plan to `docs/plans/archive` when implemented.

## Suggested commits (staged by concern)

1. `feat: S_POISON drain state, addPoison entry point, FLAG_POISON/FLAG_ANTIPOISON consume wiring, silent addHealth` (Phase 1)
2. `feat: poison palette tint (HW + SW renderers)` (Phase 2)
3. `feat: player-centered poison aura fx draw` (Phase 3)
4. `docs: poison spec + compliance hashes` (Phase 4 doc half; tests ride along in 1–3)

## Deferred wiring (explicitly out of scope)

- ~~Tile-flag poison sources~~ **moved into Phase 1** —
  `FLAG_POISON 0x20` / `FLAG_ANTIPOISON 0x10` already declared in
  `TileFlag` (`src/sprtypes.h:45-46`), name-stringed in
  `src/tilesdebug.cpp:228-229`, awaiting their first consumers.
- Antidote depth: full clear (planned) vs partial reduction — needs a
  ruling when an item tier exists.
- Attr poison source: free slot `ATTR_POISON_TRAP 0xec` between
  `ATTR_TRAP 0xeb` and `ATTR_MSG_MIN 0xf0` when wiring lands.
- Mob poison contact: `AI_POISON` (`src/sprtypes.h:33`, declared, no
  consumer) — poison on monster touch.
- Boss poison contact: `BOSS_FLAG_POISON` beside `BOSS_FLAG_ICE_DAMAGE`
  (`src/bossdata.h:60`) in `handleBossHitboxContact`.
- `EVENT_POISON` consumer in HUD/announcer text.

## Cross-platform watch-list

No new dependencies, no SDL API surface, no new raw new/delete. POD/enum
changes only (`enum GameStat : uint16_t` fixed underlying type preserved).
`m_fxTick` deliberately absent from the save stream — confirm the fx save/
load block sizes (`src/game.cpp:1431-1440`, `:1556-1562`) stay byte-identical.
Verify `poison` colormap byte order matches `godMode` on the wasm/emscripten
smoke build.
