# BUG-H21 — Area-1 remaining Medium/Low findings

Date: 2026-11-18
Audits closed: docs/audits/finalized/audit-src-2026-11-16.md — this batch closed every
remaining open Area-1 finding; the audit is now fully FIXED or WON'T-FIX and was moved
to `docs/audits/finalized/`.

## Fixes

- `src/runtime.cpp`:
  - `parseHelp()` — `line.size() >= sizeof(marker)` gates the `memcmp` (heap OOB read).
  - resolution menu — `w < 640` filter moved from the menu builder into
    `createResolutionList()`; menu / `m_resolution` / handler now share one index space.
  - screenshot filename — NULL `std::localtime()` → `LOGW` + untimed fallback name.
  - `fetchFile()` — size kept `long`, rejected `<= 0`; short read → `delete[]` +
    `LOGE` + `false`.
  - `updateScroller()` — early return when `scrollerBufSize() < 2` (`size_t` wrap).
  - `handleWindowResize()` — `SDL_GetRenderOutputSize` failure → `LOGW` + keep viewport.
- `src/runtime.h` — `#pragma once` added.
- `src/main.cpp`:
  - Emscripten `emscripten_set_main_loop_arg` arg is now `g_runtime`, not `&runtime`
    (unique_ptr address). UAF scenario verified unreachable independently
    (`simulate_infinite_loop=1`; `onSDLQuit` cancels the loop).
  - removed `#define EXIT_SUCCESS/EXIT_FAILURE` (provided by `<cstdlib>`).
- `src/engine.h` — `Engine::~Engine` now `virtual` (class already polymorphic);
  `fazeScreen` pure-virtual default argument removed.
- `src/engine_hw.h` — `fazeScreen` override default argument removed (all callers
  pass `str`; zero behavior change). `drawTile()` returns early on NULL `tile`.
- `src/engine_hw.cpp`:
  - `preloadHearts()` — `CFrame` stack local instead of leaked `new`.
  - `drawKeys()` — NULL check after `m_tileset_tiles.getTile(k)`.
  - `brFactor` — `255.f / 12` (fade now reaches 255).
  - `preloadAssets()` — all `m_tileset_*.load()` / `loadTextureAsset()` results
    checked, `LOGE` on failure.

## Won't-fix (triage, rationale in the audit)

- `__argc`/`__argv` WinMain path — mingw-w64 provides them; mingw32 CI is green.
- Unbraced logger macros / `LOGF` `exit(1)` — frozen surface + documented convention.
- `filemacros.h _R` — advisory pattern note, no concrete defect left.

## Verification

`cmake --build build` clean; `ctest --test-dir build` 1/1 passed (2026-11-18).
No serialized-state change → no ENGINE_VERSION bump. No new tests: fixes are
guards/logging on SDL-bound or degenerate-input paths already covered by the
BUG-H19 suite additions.
