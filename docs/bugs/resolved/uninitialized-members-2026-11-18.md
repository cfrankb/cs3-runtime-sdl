# BUG-H18 — Uninitialized members (NSDMI sweep)

Date: 2026-11-18
Audits closed: docs/audits/audit-src-2026-11-16.md (engine.h:220, engine.h:263-272),
docs/audits/gamemixin.audit.md (Medium m_cx/m_cy/m_timer),
docs/audits/game.cpp.audit.md (Medium m_mode/m_report; m_chuteTarget partially —
serialization still open, save-format change deferred),
docs/audits/FrameSet.cpp.audit.md (Medium m_currFrame).

## Fix (header default member initializers only; no wire-format change)

- `src/engine.h`: `EngineState` all members, `Engine::m_ticks/m_game/m_state/
  m_width/m_height/m_gameMixin`, POD structs `visualStates_t::sugarCubes[]`,
  `cameraContext_t`, `sprite_t`, `visualCues_t`, `message_t`, `hiscore_t`.
- `src/gamemixin.h`: `visualStates_t::sugarCubes[]`, `message_t`,
  `m_hiscores/m_joyState/m_vjoyState/m_buttonState`, `m_cx/m_cy/
  m_currentEvent/m_eventCountdown/m_timer/_WIDTH/_HEIGHT`.
- `src/game.h`: `m_chuteTarget{}` (ponytail comment: still unserialized),
  `m_mode = MODE_TITLE`, `m_report{}`. `m_nextLife` left ctor-set
  (`SCORE_LIFE` not visible in the header).
- `src/shared/FrameSet.h`: `m_currFrame = -1`.
- `src/actor.h`: all scalar members default-initialized as defense-in-depth
  (ctors already set them).

## Verification

`cmake -B build && cmake --build build` clean; `ctest --test-dir build`
1/1 passed (2026-11-18). No serialized-state change, so no
ENGINE_VERSION bump.
