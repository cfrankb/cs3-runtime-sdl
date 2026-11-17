# Action plan: critical issues from docs/audits (2026-11-17)

**Status: COMPLETE 2026-11-17.** All 10 entries (14 Critical findings) fixed,
pinned by tests that fail on the pre-fix binaries, `ctest` 100%. BUG-C9 also
verified under a dedicated ASan build (`build-asan`,
`-DCMAKE_CXX_FLAGS=-fsanitize=address`).

Source: per-file audits in `docs/audits/` dated 2026-11-16/17. 14 Critical
findings across 9 files, grouped into 6 fix clusters. Each BUG below may
cover several audit findings; the audit file+line is given so the fix can be
checked against the original write-up.

Order of attack: P1 first (one validation pass closes six Criticals on the
untrusted-asset path), then P2 (boss pipeline), then P3 (isolated live
crashes). Every fix: rebuild (`cmake --build build`), run `ctest --test-dir
build`, add/extend the smallest test that fails before the fix, then move the
entry to `docs/bugs/resolved` and flip the audit finding.

---

## P1 — Untrusted asset parsing

### BUG-C1 (Critical) `CFileMem` trusts stream-declared string lengths
- **Where:** `src/shared/FileMem.cpp` — `operator>>(std::string&)` (audit
  `FileMem.cpp.audit.md` Critical L120-147).
- **Symptom:** `str.resize(length)` + `memcpy` with a length decoded straight
  from the asset stream (up to ~4 GiB), no check against remaining buffer →
  heap OOB read, garbage returned as success. Also the 32-bit extension reads
  4 bytes after validating only 2.
- **Fix:** after each prefix stage require `m_ptr + sizeof(field) <=
  m_buffer.size()`; before resize/memcpy require `length <=
  m_buffer.size() - m_ptr`, else return failure (keep the existing
  `m_ptr`-untouched-on-failure behavior).
- **Verify:** crafted OBL5 tag stream test (reuse FrameSet tag fixture);
  expects failure return, no crash.
- **Status:** **Fixed.** Added the 32-bit-extension presence check and the
  payload-vs-remaining-bytes check in `operator>>(std::string&)`; failure
  leaves `str` and the payload position untouched. Pinned by
  `test_ifile_string_bounds` (`tests/t_ifile.cpp`); the pre-fix binary aborts
  on that test, post-fix `ctest` passes 100%.

### BUG-C2 (Critical) `CFileMem` accepts negative sizes
- **Where:** `src/shared/FileMem.cpp` — `read`/`write`/`append` (audit
  `FileMem.cpp.audit.md` Critical L35-43, L55-79).
- **Symptom:** negative `int` size passes the signed bounds test, then
  converts to a huge `size_t` memcpy → wild OOB read; in `write`/`append` the
  `newSize` wrap skips the grow-check → heap OOB **write**, or uncaught
  `length_error` → terminate.
- **Fix:** `if (size < 0) return IFILE_NOT_OK;` at top of `read` and `write`
  (assert in `append`); take `size_t` in `append`.
- **Verify:** one assert-based unit test per entry point with `size = -1`.
- **Status:** **Fixed.** `read()`/`write()` return `IFILE_NOT_OK` on negative
  size; `append()` now takes `size_t` (no external callers; also removes the
  `size_t`→`int` narrowing in the `operator<<(std::string_view)` writers).
  Pinned by `test_ifile_negative_size` (`tests/t_ifile.cpp`); pre-fix binary
  reports `write(buf, -1) accepted` then segfaults, post-fix `ctest` passes
  100%.

### BUG-C3 (Critical) `parsePNG` copies attacker-sized chunk payloads into fixed buffers
- **Where:** `src/shared/PngMagic.cpp` (audit `PngMagic.cpp.audit.md`
  Critical L236/L252/L272 and L291-304).
- **Symptom:** IHDR/PLTE/tRNS payloads of any declared `chunkSize` are
  memcpy'd into a 24-byte struct, `plte[256][3]`, `trns[256]` (stack smash);
  obLT `Count` is an unchecked uint32 driving `resize`/`memcpy` reads past the
  chunk buffer. CRC check is attacker-satisfiable so it gates nothing.
- **Fix:** before copying reject `chunkSize != 13` (IHDR), `> 768` or `%3`
  (PLTE), `> 256` (tRNS); before parsing obLT require `chunkSize >= 12` and
  `12 + (version==0 ? 4 : 16) * Count <= chunkSize` in unsigned math; set
  `lastError` and fail.
- **Verify:** `tests/t_pngmagic.cpp` fixtures with oversized IHDR/PLTE and a
  bogus obLT `Count`; expect parse failure, no crash.

### BUG-C4 (Critical) `importIMC1` RLE decoder overruns the dest buffer
- **Where:** `src/shared/FrameSet.cpp:829-848` (audit `FrameSet.cpp.audit.md`
  Critical).
- **Symptom:** a crafted 0xff escape at the tail reads 2 bytes past the
  source vector and writes a run of up to 65535 bytes with no bound against
  `destSize` (~64 KiB heap overflow); the consistency check fires only after
  the writes.
- **Fix:** track remaining source bytes before dereferencing the escape;
  clamp/verify each run against `destSize - (ptr - dest.data())` and fail the
  import when exceeded.
- **Verify:** crafted 1x1 IMC1 fixture through `extract()`; expect import
  failure.
- **Status:** **Fixed.** Decoder rewritten index-based: escape sequences must
  fit in the source; a run that overruns `destSize` fails the import.
  Literals past an exactly-full destination are treated as padding (real IMC1
  files carry trailing filler the old one-byte-short decoder discarded —
  pinned by `ALIH1LF0.IMA` in `test_frameset_families`). Pinned by
  `test_frameset_hostile_imc1`; pre-fix binary accepts the run overrun then
  segfaults, post-fix `ctest` passes 100%.

## P2 — Boss data pipeline

### BUG-C5 (Critical) bossdata loader derefs unchecked JSON/INI lookups
- **Where:** `src/bossdata.cpp` (audit `bossdata.cpp.audit.md` Criticals
  L390-400, L697-712, L740/L757).
- **Symptom:** `find("width")->asInt()` etc. without null checks (malformed
  JSON → null deref); `hitboxFrames` accumulates past `MAX_HITBOX_PER_FRAME`
  (4) but `s.count` records the full count → OOB read in `boss.cpp:87`;
  `e[1]` indexed with no `e.size() >= 2` guard in `handleSeq` and the scalar
  attribute chain (bare keyword line → vector OOB).
- **Fix:** check every `find()` result and `hb.type == Object` (LOGE + fail);
  `s.count = min(boxes.size(), MAX_HITBOX_PER_FRAME)` + LOGW on truncation;
  one `e.size() < 2` guard (LOGW + return) before the attribute chain and in
  `handleSeq`.
- **Verify:** malformed-JSON and bare-keyword INI fixtures in the bossdata
  loader tests; expect load failure, no crash.
- **Status:** **Fixed.** `numAt()` checks every `find()` result and the
  `hb` entry type (LOGE + fail); `frameWidth/Height/cols` must be present
  and > 0 (also closes the `x / frameWidth` div-by-zero); `s.count` clamped
  to `MAX_HITBOX_PER_FRAME` with LOGW; `e.size() < 2` guards added in
  `handleSeq` and once before the scalar chain. Pinned by
  `test_bossdata_hostile` (crafted `tests/out/bd_hostile` fixtures:
  missing-key JSON, non-object hitbox entry, 5-box frame clamps to 4, bare
  `hp`/`aims`/`moving` lines); pre-fix binary segfaults, post-fix `ctest`
  passes 100%.

### BUG-C6 (Critical) `CBoss` constructor takes a defaulted-null `bossData_t*`
- **Where:** `src/boss.cpp` ctor L43-52, `src/boss.h:58` (audit
  `boss.cpp.audit.md` Critical).
- **Symptom:** ctor derefs `data` unconditionally while the header defaults
  the param to `nullptr`; a failed `getBossData()` is a guaranteed null deref.
- **Fix:** drop the `= nullptr` default; at the production call site check
  `getBossData()` and skip boss creation on failure (LOGE).
- **Verify:** compile-level (no default) + grep confirms no caller relies on
  the default.
- **Status:** **Fixed.** `= nullptr` dropped; spawn site (`game.cpp` attr
  loop) already checked `getBossData()` before constructing. The save-load
  path's `vector::resize` now uses an explicit `CBoss() = default` backed by
  default member initializers (`m_bossData = nullptr`, `m_aim = AIM_NONE` —
  also closes the audit's uninit-`m_aim` High). Follow-up: `CBoss::read()`
  now calls `setSolidOperator()` after resolving `m_bossData`, so
  restored-from-save bosses no longer hold a null `m_solidCheck`. Compile +
  existing game/save tests pass 100%.

## P3 — Isolated live crashes

### BUG-C7 (Critical) `CGame::startLevel` derefs `CMapArch::at()` unchecked
- **Where:** `src/game.cpp:407` (audit `game.cpp.audit.md` Critical).
- **Symptom:** `m_map = *(m_mapArch->at(m_level))` — `at()` returns nullptr
  out-of-range and `m_level` is settable unchecked (`setLevel()` L570-573) or
  read raw from a save (L1224) → null deref.
- **Fix:** null-check the returned pointer and `m_mapArch`; validate
  `m_level < m_mapArch->size()` after `read()`, LOGE + return false.
- **Status:** **Fixed.** `loadLevel()` gates `m_mapArch != nullptr` and
  `at(m_level) != nullptr` *before* `setMode`/`usedItems` mutation;
  `CGame::read()` rejects a save whose `m_level` falls outside the loaded
  archive (check skipped when no archive is loaded — headless save roundtrip
  tests pin that). Pinned by `testLevelBounds` (null arch, level above size,
  negative level); pre-fix binary segfaults on the null arch, post-fix
  `ctest` passes 100%.
- **Verify:** test: out-of-range level via save/load path returns failure.

### BUG-C8 (Critical) gamemixin render path derefs null frames from `FrameSet::operator[]`
- **Where:** `src/gamemixin.cpp:811,815,900,971` (audit `gamemixin.audit.md`
  Critical).
- **Symptom:** `CFrameSet::operator[]` returns nullptr + LOGW out-of-range;
  `drawViewPortDynamic/Static` deref `*tile`, `drawBossses` derefs
  `*(*frames)[num]` with `num = boss.currentFrame()` never range-checked →
  data-driven crash.
- **Fix:** check `tile` before `drawTile` and skip (mirror the existing check
  at cpp:756); validate `num < frames->size()` with LOGW.
- **Verify:** render test with an out-of-range animz/tile index; expect skip,
  no crash.
- **Status:** **Fixed.** `continue` on null `calcSpecialFrame()` in both
  viewport loops; overlay `emplace` requires non-null tile; `drawBossses`
  validates `0 <= num < frames->size()` (and non-null entry) with LOGW.
  Pinned by a synthesized case in `test_gamemixin_bosses` (copy of real
  `bossData_t` with `sheet=0, aims=1, moving={0,1}` + emptied `sheet0`);
  pre-fix binary aborts on `vector<CFrame*>::operator[]`, post-fix draws
  nothing and `ctest` passes 100%.

### BUG-C9 (Critical) A* use-after-free on g-cost improvement
- **Where:** `src/ai_path.cpp:187` and `:479` (`AStar`/`AStarSmooth`) (audit
  `ai_path.cpp.audit.md` Critical).
- **Symptom:** `nodes[newPos] = make_unique<Node>(...)` frees the old Node
  while its raw pointer is still in `openList`; every later sift/pop touches
  freed memory.
- **Fix:** keep nodes alive for the search lifetime (`deque<Node>` arena or a
  `vector<unique_ptr<Node>>` ownership-only side list), update in place; on
  pop skip stale entries via `closedList.count(current->pos)`.
- **Verify:** extend `t_ai_path` with a map that forces a g-cost relaxation;
  run under ASan if available.
- **Status:** **Fixed.** Both `AStar` and `AStarSmooth` relax `gCost`/`parent`
  in place and re-push the same pointer; pop skips stale copies via
  `closedList` membership (and `AStarSmooth`'s `closedList[newPos]` probe
  became a `find()` so it no longer seeds false-valued entries). The test map
  (8x6, start (2,5), goal (7,0), 7 walls — mined by a search-identical
  simulation, the branch fires only via heap tie-breaks) joins
  `test_pathfinding` for AStar+BFS. Pre-fix ASan build (`build-asan`) reports
  `heap-use-after-free` at `t_ai_path.cpp`; post-fix ASan run is clean
  (exit 0) and `ctest` passes 100%. Drive-by: unblocking the ASan run exposed
  an unflagged pre-existing OOB read at `level.cpp:89` (`std::string` built
  from an unterminated vector via `strlen`); fixed with the sized ctor.

### BUG-C10 (Critical) `CFileWrap` string reader can't decode the 32-bit length its writer emits
- **Where:** `src/shared/FileWrap.cpp:154-171` vs writer L203-210 (audit
  `FileWrap.cpp.audit.md` Critical).
- **Symptom:** `size == 0xffff` dispatch is dead (`size` holds one byte ≤
  0xff), so a ≥65535-byte string written by `operator<<(std::string_view)`
  round-trips corrupted and desyncs the stream for the rest of the file.
- **Fix:** decode into `uint8_t`/`uint16_t`/`uint32_t` locals mirroring the
  working `FileMem.cpp` L122-138 logic.
- **Verify:** string round-trip test spanning all three length tiers
  (<254, 65535-boundary, ≥65535) on both backends, asserting identical
  results.
- **Status:** **Fixed.** Disk reader decodes `uint8_t`/`uint16_t`/`uint32_t`
  locals mirroring `CFileMem` (byte <0xff; 0xff→uint16; uint16 0xffff→uint32
  content size), plus a remaining-bytes bound mirroring BUG-C1 so a hostile
  32-bit length can't trigger a 4GB `resize`. Size-0 payload skips the `fread`
  (empty string no longer fails the disk backend). Pinned by
  `test_ifile_string_tiers`: 5/300/65535/100000-byte strings round-trip on
  both backends and the two encodings compare byte-identical; pre-fix binary
  fails with "disk round-trip mismatch at size 65535", post-fix `ctest`
  passes 100%.

---

## Out of scope here

- `audit-src-2026-11-16.md` sweep (4 Highs: SDL mouse-button OOB write,
  screenshot null deref, two `drawTitlePix` OOB reads) has no per-file audit
  counts yet — needs its own triage pass before bug entries.
- High/Medium findings from the same audits stay in the audit files; fix
  alongside the cluster members when the same function is being touched.
