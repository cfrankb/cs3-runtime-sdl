# BUG-H19 regression — read() flag compared against bytecount

Date: 2026-11-22
Symptom reported by user: `hiscore read failed. resetting to default.` logged on
every startup with a valid hiscore file; scores wiped.

## Cause

The BUG-H19 pass (commits `3ba53b5`, `5cd5c42`; 2026-11-17) added `read()`
return checks but compared the return against a byte count. Per the
`src/shared/IFile.h` contract, `CFileWrap::read()`/`write()` return the
`IFILE_OK`/`IFILE_NOT_OK` flag (1/0), never a byte count. A successful read
returns 1, so the check failed for any size > 1.

- `src/runtime.cpp` `loadScores()`: `read(...) != static_cast<int>(sizeof(m_hiscores))`
  → every successful load logged the error above and called `clearScores()`.
  Introduced in `3ba53b5`.
- `src/runtime.cpp` `fetchFile()`: `read(...) != static_cast<int>(size)`
  → would reject every file > 1 byte ("short read"). Never surfaced: the
  function has no call sites. Introduced in `5cd5c42`.

## Fix

Both comparisons → `!= IFILE_OK`. Additionally `saveScores()` now checks the
`write()` return (`LOGE` + `return false`) instead of ignoring it — the other
half of what BUG-H19 was meant to guard.

## Verification

`cmake --build build` clean; `ctest --test-dir build` all passed (2026-11-22).
Regression test: `test_runtime()` gained a saveScores→clearScores→loadScores
round-trip asserting slot 3 survives; fails if the bytecount comparison is
restored (`tests/unit/t_runtime.cpp`).

No serialized-state change → no ENGINE_VERSION bump.
