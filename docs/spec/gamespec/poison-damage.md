# Poison Damage (`S_POISON`) Specification

Status: **Implemented** (2026-11-20; plan `docs/plans/archive/poison-damage-plan.md`).
The aura presentation is specced in `extended-fx.md` §2 S5.

The player carries a pool of HP-damage-to-be-drained with a drain clock;
while the pool is nonzero the player sprite is tinted, the health hearts
are green and a two-layer aura is drawn centred on the player. The pool
is the single source of truth — `isPoisoned()` is exactly
`S_POISON != 0`; there is no separate poisoned flag.

## 1. State

- P1. `S_POISON` (remaining HP to drain) and `S_POISON_NEXT` (ticks until
  the next drain) are appended **last** in `enum GameStat`
  (`src/gamestats.h`) — raw `uint16_t` wire keys, no old key shifts.
- P2. Serialization rides the existing generic zero-suppressed kv map in
  `CGameStats`. **No `ENGINE_VERSION` bump** (maintainer ruling D8, a
  deliberate deviation from the BUG-H12 convention): an absent key
  loads as 0, so old saves stay valid. `tests/t_game.cpp`
  `EXPECTED_VERSION` stays unchanged.
- P3. Tuning constants (`GamePrivate` in `src/game.cpp`):
  `POISON_DRAIN = 8`, `POISON_INTERVAL = 12`, `POISON_DEFAULT = 32`.

## 2. Application

- P4. `CGame::addPoison(int amount)` (`src/game.h`): non-positive amounts
  are ignored; the amount stacks onto the pool; the clock is armed to
  `POISON_INTERVAL` **only when currently 0** (re-application mid-countdown
  does not reset it); `EVENT_POISON` is queued.
- P5. Tile wiring in `CGame::consume()`: `FLAG_POISON` →
  `addPoison(POISON_DEFAULT)`; `FLAG_ANTIPOISON` → pool and clock set to
  0. Both branches are independent of the mutually-exclusive powerup
  chain (a poison flag never swallows the scan and vice versa). The flags
  were declared in `src/sprtypes.h` long before having consumers
  (reserved-vocabulary rule, AGENTS.md).
- P5b. Attr wiring in `CGame::manageTrigger()`: `ATTR_POISON_TRAP`
  (`0xec`, claimed from the unassigned run in `src/attr.h`, beside
  `ATTR_TRAP 0xeb`) → `addPoison(POISON_DEFAULT)`; one-shot like the
  freeze/trap neighbours (the cell attr is cleared on entry). Ships
  2026-11-20, superseding the D5 deferral of the attr source.

## 3. Drain — `CGame::decTimers()`

- P6. While `S_POISON > 0`, the pool fires when `dec(S_POISON_NEXT)`
  reaches 0: `drain = min(POISON_DRAIN, pool)`; the pool is decremented
  **first**, then `addHealth(-drain, /*silent=*/true)`.
- P7. Forfeit model (D1): damage swallowed by god mode or i-frames is
  lost — the pool decrement already happened.
- P8. Normal damage (D2): the drain routes through `addHealth`, so
  shield and skill scaling and the `S_PLAYER_HURT` i-frame gate apply
  like any hit. Consequence: the effective max damage rate is one drain
  per `max(POISON_INTERVAL, ~12 ticks)` because an applied drain re-arms
  the 3-stage hurt decay.
- P9. Silent (D3): `addHealth(const int hp, const bool silent = false)`
  skips the hurt sound only; every legacy single-arg call is unchanged.
- P10. The clock re-arms to `POISON_INTERVAL` only if pool remains > 0;
  a pool emptied exactly leaves `S_POISON_NEXT == 0`.

## 4. Clearing (D4)

- P11. `CGame::resetStats()` clears both keys; it runs on level load and
  on the death paths, so poison never crosses a life or a level.
  `resetStatsUponDeath()` carries no poison keys.

## 5. Presentation (driven solely by `isPoisoned()`)

- P12. Player tint: `COLOR_POISON` mask / `ColorMaps::poison` /
  `[poison]` colormap section, appended **last** in both renderers'
  player-status priority chains (hurt, frozen, sugar, god, rage all
  outrank it). Values are data-driven; an empty map is a no-op, so the
  code ships ahead of the asset (same staged rollout as the aura).
- P13. Hearts (hearts mode only): SW chain
  `god ? WHITE : hurt ? PINK : poisoned ? GREEN : RED`
  (`CGameMixin::drawHealthBar`); HW bakes a fixed `RED→GREEN`
  `cacheMask(COLOR_POISON)` variant of the code-generated heart sheet
  (`EngineHW::preloadHearts`) and selects it in `EngineHW::drawHealthBar`
  at the same priority. The CLASSIC bar is unchanged — its healthy color
  is already LIME.

## 6. Deferred wiring

~~Attr~~ (shipped 2026-11-20, P5b: `ATTR_POISON_TRAP 0xec`), mob contact
(`AI_POISON`),
boss contact (`BOSS_FLAG_POISON`), antidote tiers and an `EVENT_POISON`
HUD consumer are out of scope; `EVENT_POISON` currently has no consumer.
No static tile carries `FLAG_POISON` yet, so the `consume()` branches are
covered by code inspection until the first poison tile ships.

## Tests

`t_game.cpp::testPoisonDrain` (apply/clock/event, 1:1 drain at skill 0,
god-mode forfeit P7, exact empty P10, `resetStats` clear P11),
`t_tables.cpp::testColorMaps` (`[poison]` section parse/clear),
`t_fx.cpp::test_fx_poison_aura` (aura, see extended-fx.md S5).
