# Bug reports: coverage round 2 findings

Source: `docs/plans/coverage-round2-test-plan.md` phases A–E. All items below
were located and reproduced by the `cs3-runtime-tests` suite; the tests pin
the *current* behaviour and will need updating when each bug is fixed.
Spec hash at time of writing: total coverage 60.7% (commit `7ca3301`).

Severity key: **H** = memory corruption / abort reachable from game data or
normal calls; **M** = latent corruption, guarded paths today; **L** = silent
state bug.

> **Status 2026-11-18: BUG-1..BUG-6 all fixed and re-verified**
> (`cmake --build build && ctest --test-dir build` green at HEAD `512d2b7`);
> the `FINDING` workarounds in `tests/t_gamemixin.cpp` were removed. This report
> is fully implemented and lives in `docs/bugs/resolved/`. The unbounded
> full-blit `drawTile` pattern called out under BUG-1 is tracked separately in
> `docs/audits/finalized/audit-src-2026-11-16.md` (Area 1, `engine_hw.h` `drawTile`).

---

## BUG-1 (H) `drawFont` / `drawRect` write unbounded past the bitmap

- **Where:** `src/gamemixin.cpp` — `drawFont` (~101), `drawFont6x6` (~137),
  `drawRect` (~212).
- **Symptom:** destination pointer arithmetic (`getRGB().data() + x +
  y * width`) with no check of `x + len * scaleX * FONT_SIZE` against
  `bitmap.width() * bitmap.height()`. Text or rects that overflow the frame
  corrupt the heap. Observed as a real SIGSEGV in `drawScores -> drawFont`
  while constructing the Phase E test fixture.
- **Repro:** any `drawScores`/`drawFont` call whose scaled text run crosses
  the bitmap edge; `tests/t_gamemixin.cpp` currently avoids it by sizing
  fixtures (documented in the test header).
- **Status:** **Fixed.** `drawFont` now skips pixels outside the frame (per
  row/column) and `drawRect` clamps its loops to the frame in both fill and
  outline branches (`drawFont6x6` already had its `boundCheck`).
  `test_gamemixin_primitives` pins overflow and negative-origin clipping.
  Remaining unguarded pattern noted in the full-blit `drawTile` overloads is
  NOT part of this fix.

## BUG-2 (H) `getEventText` aborts on an empty MSG state string

- **Where:** `src/gamemixin.cpp:2147` (`CGameMixin::getEventText`, MSG0..MSGF
  branch) and the duplicate `src/engine.cpp` (`Engine::getEventText`).
- **Symptom:** `split(tmp, '\n')` on an empty string returns an empty
  vector; `list[0]` then trips the libstdc++ `vector::operator[]` bounds
  assert (abort). The code even checks `list.size() == 0` two lines later —
  *after* dereferencing.
- **Repro:** set a `MSG0`-range event and leave the map state string unset,
  then `drawScreen`; see `test_gamemixin_screen` (works around it by
  seeding `MSG0` text — workaround marked `FINDING` in the test).
- **Status:** **Fixed.** Both copies now guard `line1` with the same
  `size() > 0` ternary as `line2`/`line3`; `test_gamemixin` pins
  the empty-string case and the `FINDING` workaround comment in
  `test_gamemixin_screen` was removed.

## BUG-3 (M) `drawViewPortDynamic` reads `map->at(width)` with odd camera

- **Where:** `src/gamemixin.cpp` `CGameMixin::drawViewPortDynamic` tile loop.
- **Symptom:** `m_cx` is half-pixel units (`mx = m_cx / 2`, `ox = m_cx & 1`);
  with `ox == 1` the column loop runs to `x == cols`, so
  `map->at(x + mx, y + my)` indexes `width` when the window is flush to the
  map edge → `CMap::at` throws `std::out_of_range`.
- **Repro:** resize map to 10x10, set `m_cx` odd with `mx = width - cols`
  (e.g. `m_cx = 13`, 64-px viewport), call `drawViewPortDynamic`.
- **Exposure today:** `centerCamera` only writes even `m_cx`, but camera
  nudges/speed paths that add 1 can produce odd values at the edge.
- **Status:** **Fixed.** The `CGameMixin` loop now breaks on
  `x + mx >= map->width()` / `y + my >= map->height()`, mirroring the guard
  the `EngineHW` twin already had. `test_gamemixin_viewports` pins the
  flush-to-edge case (previously the `FINDING` workaround kept the camera
  one tile inside the edge).

## BUG-4 (M) `CFrameSet::operator--` is UB and leaks on a non-empty set

- **Where:** `src/shared/FrameSet.cpp` `CFrameSet::operator--`.
- **Symptom:** `delete m_frames[this->getSize()];` deleted one-past-the-end
  (valid indices are `0..getSize()-1`); `removeAt` does *not* delete — it
  returns the removed pointer — so the frame actually removed was also
  leaked. Only the empty-set early-return path was safe.
- **Repro:** `CFrameSet s; s.add(f); --s;` — ASan flags the OOB read/delete.
- **Status:** **Fixed.** Now `delete removeAt(this->getSize() - 1);`;
  `test_frameset_container` exercises the non-empty path (the old
  empty-guard-only note was removed).

## BUG-5 (fixed) missing `default:` in `drawLevelIntro`

- **Where:** `src/engine.cpp` `Engine::drawLevelIntro`, `src/gamemixin.cpp`
  `CGameMixin::drawLevelIntro`.
- **Symptom (was):** an unmatched game mode left the stack buffer `t`
  uninitialised, then used it for `strlen`/`drawFont`.
- **Status:** **Fixed.** Both switches carry `default: t[0] = '\0';`
  (`src/engine.cpp:86-87`, `src/gamemixin.cpp:1110`), committed in `c38f854`
  and re-verified 2026-11-18 (`cmake --build build && ctest --test-dir build`
  green at HEAD `512d2b7`). All six findings in this file are now resolved;
  this report moved to `docs/bugs/resolved/`.

## BUG-6 (L) `CGame::resetStats` leaves `S_PLAYER_HURT` and `S_BOAT`

- **Where:** `src/game.cpp` `CGame::resetStats`.
- **Symptom:** callers expect `resetStats()` to give a clean slate; these two
  stats survive. A leaked `S_PLAYER_HURT` silently blocks damage in
  `addHealth` (the invincibility-frame gate), and a leaked `S_BOAT` keeps
  the boat frame in `tile2Frame`. Bitten for real by
  `test_gamemixin_frames_routing` (works around it; `FINDING` comment).
- **Status:** **Fixed.** Both ids added to the `resetStats` list; the
  `FINDING` workaround in `test_gamemixin_frames_routing` was removed.

---

## Not filed (behaviour pinned by tests, likely intentional)

Menu `up`/`down` no-wrap / disabled-row hits; `ITEM_BAR::str()` returning
`"invalid type"`; `gameui` `width()/height()` as max extents; `CFrame`'s two
ambiguous `operator=` overloads; `clip` only honouring `-1`;
`abgr2argb` dropping colour at alpha 0; `setCurrFrame` no clamp; obLT chunk
headers in native byte order + unreachable version-error return;
`drawKeys` decay-after-draw; `killPlayer` spending lives only;
`drawUI` width-based `baseY`. See the Result blocks in the plan for each.

`mainLoop` / `manageGamePlay` are headless-untestable (audio/SDL fan-out) —
the deliberate gap in `gamemixin.cpp` coverage, noted in
`docs/compliance/src-checklist.md`.
