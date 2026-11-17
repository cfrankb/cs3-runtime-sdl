# Action plan: High findings from docs/audits (2026-11-17)

**Status: COMPLETE 2026-11-17.** All High findings from the per-file audits
and the `audit-src-2026-11-16.md` sweep are fixed, triaged, or downgraded
below. `cmake --build build` clean, `ctest --test-dir build` 100%.

Companion to `docs/bugs/resolved/critical-triage-2026-11-17.md` (all 14
Criticals). Order of attack: the sweep report's 4 untriaged Highs first
(P1, live crash/corruption paths), then the untrusted-asset cluster (P2),
then file-I/O and gameplay logic (P3).

Triage markers used in the audit files: **FIXED 2026-11-17** (code changed,
pinned by a test or compile-level verification) or **TRIAGED 2026-11-17**
(reasoned no-change, e.g. latent/unreachable, dead code kept on purpose).

---

## P1 — Sweep report (`audit-src-2026-11-16.md`)

### BUG-H1 (High) SDL mouse-button array OOB write
- **Where:** `src/runtime.h` / `src/runtime.cpp` `onMouseEvent` (sweep High).
- **Symptom:** `m_mouseButtons[3]` indexed by `event.button.button` — right
  click (3), X1 (4), X2 (5) write past the array.
- **Status:** **Fixed.** Array is `uint8_t m_mouseButtons[SDL_BUTTON_X2 + 1]`
  with a `button > 0 && button < sizeof(m_mouseButtons)` guard on every
  write; motion branch uses `event.motion.state & SDL_BUTTON_LMASK`.
  Gamepad default-log branch also fixed (`event.gbutton.button`, null-safe
  `SDL_GetGamepadName`). Manual-input path; no unit harness for SDL events.

### BUG-H2 (High) `takeScreenshot` null-deref chain
- **Where:** `src/runtime.cpp` `takeScreenshot()` (sweep High).
- **Status:** **Fixed.** NULL check on `SDL_RenderReadPixels` result;
  `toPng()` return now checked (LOGE + `getLastError()` + return).

### BUG-H3 (High×2) `drawTitlePix` asset/frame OOB reads
- **Where:** `src/engine_hw.cpp` `drawTitlePix()` (sweep Highs).
- **Status:** **Fixed.** Field loop bounded by both `fields.size()` and
  `QUAD_COUNT`; `src_low` built only when `quad.size() == QUAD_COUNT`;
  early guard on `ASSET_TITLESCREEN >= m_assetFiles.size()`.

## P2 — Untrusted asset path

### BUG-H4 (High×2) `CFrame::explode` throws on hostile obLT; `toPng` swallows failures
- **Where:** `src/shared/Frame.cpp` (audit `Frame.cpp.audit.md` Highs),
  `src/shared/PngMagic.cpp` (audit Highs L338, L187-217 chunk lengths —
  the latter closed under BUG-C3).
- **Status:** **Fixed.** Both `explode()` overloads pre-validate (v0 rects
  vs `m_width`; v2 unit rect inside the frame, unit dims vs new
  `CFrame::MAX_FRAME_DIM = 4096`) and fail soft: partial set + `m_lastError`,
  no `std::out_of_range`/`invalid_argument` at the PNG boundary. `toPng`
  returns `false` on zlib failure and `FrameSet::toPng` propagates.
  `parsePNG` gates IHDR/derived dims at `MAX_FRAME_DIM` ("png dimensions
  too large") before `make_unique<CFrame>` and propagates `frame->getLastError()`
  after `explode()`. Pinned in `test_png_hostile_chunks`
  (`tests/t_pngmagic.cpp`: obLT v2 unit outside frame; IHDR width 65536
  with recomputed CRC).

### BUG-H5 (High×4) `CFrameSet::readSolid`/`writeSolid`/`toSubset` validation
- **Where:** `src/shared/FrameSet.cpp` (audit `FrameSet.cpp.audit.md` Highs).
- **Status:** **Fixed.** Tag reads check `file >>` results; decompressed
  `totalSize` capped at `srcSize * 1032 + 1024` (deflate's best ratio) before
  the 1 GiB pre-allocation; compressed blob read checked; `writeSolid` now
  caps dims at `MAX_IMAGE_SIZE` like `readSolid` (no more save-files-the-
  runtime-can't-load); `toSubset` clamps `start`/`end` in int math and
  returns empty on inverted ranges.

### BUG-H6a (High×2 + 1 closed by BUG-C6) `CBoss` save-read bounds enforcement
- **Where:** `src/boss.cpp` (audit `boss.cpp.audit.md` Highs L43-52, L309,
  L443-461).
- **Status:** **Fixed.** All seven `checkBound()` results in `read()` are
  now enforced (`if (!checkBound(...)) return false;`); `aim` validated as
  `< TOTAL_AIMS` with `AIM_NONE` explicitly legal; `currentFrame()` guards
  `aims <= 0` before `m_aim % aims` (div-by-zero). The uninit-`m_aim` High
  was closed by BUG-C6's default member initializer. Pinned in
  `tests/t_boss.cpp` (hostile save-field record: state `0x7f` rejected,
  `AIM_NONE` record accepted). Follow-up 2026-11-17: garbage/missing
  `aims` is now detected and LOGW-warned at *load* time
  (`bossdata.cpp` `BossAccum::build` + the parse.py frame-layout
  walker, each normalizing to 1); the `boss.cpp` clamp remains as
  defense-in-depth. Invariant pinned in `tests/t_bossdata.cpp`
  (all loaded bosses have `aims >= 1` after the BareWords fixture).

### BUG-H6b (High×2) `bossdata` hostile JSON numbers
- **Where:** `src/bossdata.cpp` (audit `bossdata.cpp.audit.md` Highs).
- **Status:** **Fixed.** Negative hitbox `x/y/w/h` rejected (LOGE + fail;
  width/height/cols `> 0` validation already landed under BUG-C5);
  `asInt()` saturates to `INT_MIN`/`INT_MAX` instead of UB-casting
  out-of-range doubles. Pinned by `test_bossdata_hostile`.

## P3 — File I/O contract and gameplay logic

### BUG-H7 (High) `readVector` trusts the stream count for `reserve`
- **Where:** `src/shared/ISerial.h` (audit `ISerial.h.audit.md` High).
- **Status:** **Fixed.** `vec.reserve(count)` removed; growth now happens
  only after successful element reads, so memory stays bounded by actual
  stream content.

### BUG-H8 (High×3 + Mediums) `CFileWrap`/`CFileMem` robustness
- **Where:** `src/shared/FileWrap.cpp`, `src/shared/FileMem.cpp` (audits).
- **Status:** **Fixed.** Zero-size `read`/`write` are success no-ops on the
  disk backend (was false-failure on empty save names); `m_file` null guards
  in `read`/`write`/`getSize`(-1)/`seek`/`tell`/string `>>`; `open()` closes
  any previously open `FILE*` (fixes the record→playback reopen leak),
  rejects empty names, and copies name/mode to NUL-terminated strings;
  `CFileMem::seek` rejects negatives; the BUG-C1 payload check in
  `CFileMem::operator>>` no longer underflows after a legal past-EOF seek.

### BUG-H9 (High×3) `IFile` contract pinned in the header
- **Where:** `src/shared/IFile.h` (audit `IFile.h.audit.md` Highs).
- **Status:** **Fixed (contract doc).** Wire format (1/3/5-byte LE length
  prefix, raw int passthrough), the `{IFILE_OK, IFILE_NOT_OK}` flag-not-
  byte-count rule, zero-size no-op semantics, and seek/open/close semantics
  are now stated at the interface. Codec de-duplication across backends
  stays as the audit's Medium follow-up.

### BUG-H10 (High×3) `game.cpp` logic Highs
- **Where:** `src/game.cpp` (audit `game.cpp.audit.md`).
- **Status:** **Fixed.** `validateSignature()` LOGW now prints the
  NUL-terminated `tmp` (was the raw unterminated buffer); shield-damage
  truncation fixed to integer math `hp * (MAX_FACTOR - shields) * (1 + skill)
  / MAX_FACTOR` (pinned by `testShieldDamage` in `tests/t_game.cpp`);
  `shadowActorMove()` restores the monster-grid entry on the failed
  `canMove` path — **TRIAGED** as defense-in-depth: `CGame::translate()`
  clamps to map edges and `rebuildMonsterGrid()` only inserts in-map actors,
  so the erase-before-validate window is unreachable from gameplay (the
  attempted regression test failed for exactly that reason and was removed).

### BUG-H11 (High×3) `game_ai.cpp` double-kill guards
- **Where:** `src/game_ai.cpp` (audit `game_ai.cpp.audit.md`).
- **Status:** **Fixed.** `handleBullet` icecube/vamplant splash branches and
  `handleVamPlant` (signature extended with the deleted set) now apply the
  same `!deletedMonsters.count(id)` guard as `blastRadius`, preventing
  duplicate respawn entries and clobbered restored tiles. `spawnBullet`
  vector-reallocation hazard documented at the call site (latent: nothing
  touches the actor reference after the call). Drive-by: `speeds[def.speed]`
  index guarded (Medium L400).

### BUG-H12 (High) `CActor` `m_frame`/`m_algo` union clobber
- **Where:** `src/actor.h`, `src/actor.cpp` (audit `actor.cpp.audit.md`).
- **Status:** **Fixed.** Union split into two `uint16_t` members; ctors
  zero-init `m_frame`; move ctor/assignment carry and reset both members
  (previously `m_frame` rode along inside the `m_algo` slot).
- **Regression caught same day:** the entry claimed "wire format unchanged
  (only `m_algo` is serialized, same as before)" — false. Under the union
  the single serialized `m_algo` word implicitly carried `m_frame`; after
  the split `m_frame` was dropped from save/load entirely with no size
  change to detect it. Fix: `readCommon`/`writeCommon` now serialize
  `m_frame` as its own `uint16_t` after `m_algo`, `ENGINE_VERSION` bumped
  `0x0200000e` → `0x0200000f` (old saves rejected via
  `validateSignature`), `t_game.cpp` `EXPECTED_VERSION` mirrored, and
  `t_actor.cpp` round-trips a distinct `setFrame(7)`/`setAlgo(ASTAR)`
  pair. Compliance hashes regenerated.

### BUG-H13 (High) `CPath::followPath` unchecked downcast
- **Where:** `src/ai_path.cpp` (audit `ai_path.cpp.audit.md`).
- **Status:** **Fixed.** `dynamic_cast<CActor *>` + null-check + LOGE +
  `Result::Blocked` replaces the blind `static_cast`.

### BUG-H14 (High×3) `gamemixin` UI/health render bugs
- **Where:** `src/gamemixin.cpp` (audit `gamemixin.audit.md`).
- **Status:** **Fixed.** `drawUI` baseY uses `getHeight()` (matches
  `whichButton` hit-testing); `joyState` `uint32_t` type-pun replaced with
  byte-wise OR; `drawHealthBar` caps heart count to the bitmap width so
  `CFrame::at()` can never throw from the render path.

### BUG-H15 (High×2) `helper.cpp` unchecked copy/read utilities
- **Where:** `src/shared/helper.cpp` (audit `helper.cpp.audit.md`).
- **Status:** **Fixed.** `copyFile`/`concat` check `read`/`write` results
  (errMsg + failure), `readFile` rejects `getSize() < 0` / `> INT_MAX`
  before vector sizing (was `SIZE_MAX` → `length_error` crash) and widens
  the size through `long`.

### BUG-H16 (High×3) `logger.h` duplication, Qt block, unbraced `LOGF`
- **Where:** `src/logger.h` + `src/shared/logger.h` (audit `logger.h.audit.md`).
- **Status:** **Triaged.** The `USE_QFILE`/Qt compatibility block and the
  non-Qt `LOGF` macro were initially deleted as dead code but **restored
  verbatim on maintainer instruction 2026-11-17** — they are intentional
  compatibility layers. Both headers now carry DO-NOT-DELETE comments (with
  the unbraced-`if`/always-exits caveat) so future agents do not re-delete
  them; the Qt block was widened to `QtLogging` + `QDebug` so it compiles
  if the flag is ever enabled. The two files are byte-identical with a
  keep-in-sync note. Full single-header consolidation stays open (audit
  Medium); the unbraced `LOGF` hazard stays known (call it as a standalone
  statement).

### BUG-H17 (High×2 + triage) `CDotArray` rasterizers
- **Where:** `src/shared/DotArray.cpp` (audit `DotArray.cpp.audit.md`).
- **Status:** **Fixed.** `lineTab` iterates the dominant axis
  (`<= std::max(dx, dy)`) so steep/vertical lines reach their endpoint;
  `circle` `plot_sym` gained the four swapped-octant reflections (was two
  C-shapes open left/right). Both pinned in `tests/t_dotarray.cpp`
  (vertical 11-dot line ending at (5,10); r=5 circle contains the
  horizontal extrema (15,15)/(25,15)).
- **TRIAGED:** the dead-class deletion finding — `CDotArray` has no caller
  in this repo but is rsync'd to the external LGCK editor
  (`bin/utils/sync.sh`), whose usage cannot be verified here; kept.

---

## Out of scope here

- All Medium/Low findings stay in the per-file audit files for a future pass.
- `IFile` codec de-duplication and `long`/`int` widening (audits' contract
  follow-ups) not undertaken — behavior fix landed, type migration is a
  separate, larger change.
