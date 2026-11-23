# Plan: remove `g_sheet0_data` / `g_sheet1_data` (obLT-driven boss sheets)

Date: 2026-11-23
Status: complete 2026-11-23. Step 1's parity was proven byte-identical
by the python chunk-vs-table check recorded under "verified facts"
(no transient test kept). Loaders switched, tables deleted,
112/112 ctest green, dummy-video smoke run loads sheet0/sheet1 via
loadFx with no errors. Archived.

## Problem

`src/sheetdata.cpp` hand-maintains per-cell rect tables for the boss
sheets (165 rects, ~170 lines) that duplicate metadata already inside
the sheet images themselves. Sheet 2 (Arachine) already loads
table-free from the png's private `obLT` chunk (`TileSet::loadFx`,
plan boss-aims3-mirror §9) — sheets 0/1 should do the same and the
tables should go.

## Verified facts (2026-11-23)

- `data/pixels/sheet0.obl` and `sheet1.obl` are plain PNGs; both carry
  an `obLT` chunk, version 2, 65 resp. 100 records (8-byte
  `x,y,w,h` u16 LE) — the exact format `parseFxSheetMeta` decodes.
- Chunk records are **byte-identical** to the compiled
  `g_sheet0_data` / `g_sheet1_data` entries (python parity check).
- Table consumers: only `engine_hw.cpp:198/200`
  (`TileSet::load(renderer, filename, vector<tileinfo_t>)`).
  `t_tables.cpp::testSheetTiles` merely asserts the tables are
  non-empty.
- `tileinfo_t` itself stays in `sheetdata.h` (used by `TileSet`,
  `gamesfx`, `g_fxSheet_data`); only the two extern tables are cut.
- `g_scroll_data` (same file) has zero `src/` consumers — out of
  scope, untouched.
- No docs/ or spec references to the table names.

## Changes

### 1. Parity pin (before any removal)

In `t_tables.cpp` `testSheetTiles`, when
`data/pixels/sheet{0,1}.obl` exist (skip like `test_bossdata` does
for absent assets), decode the `obLT` chunk with the existing
`parseFxSheetMeta` and assert record-for-record equality with
`g_sheetN_data`. This is the safety net that the swap changes no
geometry; it is deleted together with the tables in step 3's wake if
desired, but it is what licenses the deletion.

### 2. Switch the loaders

`engine_hw.cpp`: load sheet 0/1 with
`m_tileset_sheetN.loadFx(m_renderer, name, scratch)` exactly like the
fx sheet and sheet 2 (local scratch `std::vector<tileinfo_t>`; the
TileSet itself holds the tiles). Keep the
`TileSet::load(renderer, filename, vector)` overload — public API,
other consumers may exist per shared-surface rules.

Failure mode: a sheet png without `obLT` → existing `LOGW` in
`loadFx` + empty tileset; boss draws then hit the pre-existing
out-of-range guards (`getTile` returns null / BUG-C8 skip), no crash.
FX sheet already accepts this contract.

### 3. Delete the tables

Remove `g_sheet0_data` / `g_sheet1_data` from `sheetdata.{h,cpp}` and
the `testSheetTiles` non-empty assertion for them (keep
`g_scroll_data`'s). Maintainer requested this deletion explicitly, so
the AGENTS.md compat-layer rule is satisfied here; the data stays
recoverable from git history for any environment stuck on a sheet
png lacking `obLT`.

### 4. Docs sync

Rebuild + full ctest first, then regenerate
`docs/compliance/src-checklist.md` / `src-coverage.md` (sheetdata.cpp
hash/shrink). No spec module describes the tables (verified above).

## Asset-side requirement

Every deployed sheet0/sheet1 png must carry the `obLT` chunk. Both
current assets do; the private sheet tool emits it for new sheets.

## Build/verify

`cmake -B build && cmake --build build && ctest --test-dir build`,
plus a HW smoke run (Demon/Beholder/Harpy + Arachine draw unchanged).

## Skipped (add when needed)

- `g_scroll_data` deletion — no ask, no consumer, one line of debt.
- Deleting `TileSet::load(renderer, filename, vector)` — public
  surface, not ours to cut.
- Runtime table fallback — defeats the deletion; git history +
  `LOGW` covers the old-asset case.
