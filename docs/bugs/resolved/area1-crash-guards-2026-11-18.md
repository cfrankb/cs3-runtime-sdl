# BUG-H19 / BUG-H20 — Area-1 crash guards + SDL3 lifecycle

Date: 2026-11-18
Audits closed: docs/audits/finalized/audit-src-2026-11-16.md (the marked FIXED Medium/Low items:
SDL3 lifecycle cluster, both integer modulo-by-zero sites, sugar meter OOB write,
drawSplash/getScreenSize/getSafeAreaWindow, loadColorMaps, loadScores, drawBossses
hpRatio, ~CRuntime leaks).

## BUG-H19 — crash guards (no wire-format change)

- `src/runtime.cpp`:
  - `openMusicForLevel()` — empty `m_musicFiles` + no MUSIC meta → `LOGW` + return
    (was `i % size()` SIGFPE; warns loudly per maintainer request).
  - `getScreenSize()` — NULL `SDL_GetDesktopDisplayMode` → `LOGW` + `Rez{0, 0}`.
  - `getSafeAreaWindow()` — `SDL_Rect safe = {}` zero-init.
  - `loadColorMaps()` — `userID` bounds check vs `m_userNames.size()` → `LOGW` + return.
  - `loadScores()` — `file.read` short read → `LOGE` + `clearScores()`.
    (Regression fixed 2026-11-22: check must compare `IFILE_OK`, not a byte
    count — see `docs/bugs/resolved/hiscore-read-check-2026-11-22.md`.)
- `src/main.cpp` — `maparch.size() == 0` before start-level modulo → `LOGE` +
  `EXIT_FAILURE` (was modulo-by-zero SIGFPE).
- `src/engine_hw.cpp` + `src/gamemixin.cpp` (`drawSugarMeter` twins) — sugar cube
  write clamped: `sugarCubes[(sugar < SUGAR_CUBES ? sugar : SUGAR_CUBES) - 1]`.
  Reachable from crafted saves (`CGameStats::readCommon` stores unvalidated int32s;
  `S_SUGAR > SUGAR_CUBES` → OOB write).
- `src/engine_hw.cpp`:
  - `drawSplash()` — NULL `m_textureSplash` → `LOGW` + return.
  - `drawBossses()` — `hpRatio <= 0.0f` → skip gauge (silent; per-frame loop).
- `src/engine.cpp` + `src/gamemixin.cpp` — hiscore name prints `%.16s`
  (`name[16]` loaded raw from file may lack NUL).

## BUG-H20 — SDL3 lifecycle

- `src/runtime.cpp`:
  - `initControllers()` — iterate `SDL_GetJoysticks()` **instance IDs** (was loop
    indices into `SDL_IsGamepad`/`SDL_OpenGamepad`) + `SDL_free` the array.
  - `createResolutionList()` / `listResolutions()` — `SDL_free` display and mode
    arrays on every path; NULL checks; `listResolutions` bounds-checks `displayIndex`.
    First-ever `SDL_free` calls in `src/`.
  - `~CRuntime()` — `SDL_CloseGamepad` all `m_gameControllers`, `delete m_bitmap`,
    `SDL_Quit()` (destructor runs on every `main()` return via the `unique_ptr`).

## Verification

`cmake --build build` clean; `ctest --test-dir build` 1/1 passed (2026-11-18).
Regression test added: `test_gamemixin_screen()` sets `S_SUGAR=200` and asserts the
meter write lands clamped on the last cube only (`tests/t_gamemixin.cpp`).
SDL-bound guards (getScreenSize, getSafeAreaWindow, drawSplash, initControllers,
~CRuntime) are guard-only — not headless-testable.
No serialized-state change → no ENGINE_VERSION bump.
