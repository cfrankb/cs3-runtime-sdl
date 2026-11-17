# Audit: src/shared/ss_limits.h
Date: 2026-11-17

## Summary
The header is a 4-macro cap registry (L20–L23) and is itself clean: every macro is a plain positive integer literal with zero references to parameters, so there is no parenthesization, double-evaluation, or overflow hazard, and no macro duplicates a standard-library limit. Every symbol is live. The one substantive issue is what the header does *not* contain: a second, competing dimension cap (4096) exists as an undocumented magic number in callers, contradicting MAX_IMAGE_SIZE=512 and causing the round-trip loss already reported in docs/audits/FrameSet.cpp.audit.md. Counts: 0 Critical, 0 High, 1 Medium, 1 Low.

## Findings

### Medium
- L21: [shrink] The header is the repo's cap registry for image data yet only defines the read-side cap (512); the write/ctor-side cap 4096 lives as duplicated magic numbers at Frame.cpp:43 and FrameSet.cpp:128, and the two disagree — that mismatch is the root of the writeSolid-accepts-4096/readSolid-rejects->512 round-trip bug (already filed in docs/audits/FrameSet.cpp.audit.md, L128-vs-L326, not duplicated here). Fix: define both caps in ss_limits.h (e.g. keep MAX_IMAGE_SIZE as the file-format cap, add a named ctor/write cap) and make Frame.cpp:43 and FrameSet.cpp:128 reference the macro instead of the literal 4096.

### Low
- L21: [stale] Trailing comment `// 256` is ambiguous — a historical value or a suggested tighter cap — with no consumer or record supporting either reading (needs verification against legacy asset files; nothing in src/ or tests/ references 256 as an image cap). Fix: delete the comment or record what 256 refers to.

## Notes
- Per-symbol reference counts (whole-repo grep of src/, tests/, examples/; definitions excluded):
  - MAX_IMAGES (L20): 6 refs, all FrameSet.cpp (L111, 287, 454, 919, 1017, 1191). Consistent everywhere; L919 also gates the MCX header count against the same cap.
  - MAX_IMAGE_SIZE (L21): 5 refs — Frame.cpp L94 (write), L156 (read); FrameSet.cpp L326 (readSolid), L1037, L1199. Read/write pairs that use it are symmetric at 512.
  - TAG_KEY_MAX (L22): 5 refs — FrameSet.cpp L203, 214, 219, 222 (write), L402 (read). Write filter and read validation are symmetric.
  - TAG_VAL_MAX (L23): 5 refs — same sites as TAG_KEY_MAX. Tags are std::string (no fixed buffers allocated against these caps); cross-file fact: TAG_VAL_MAX=1024 < 65535 is what keeps FrameSet tags below the dead 16-bit escape branch in CFileWrap's disk string reader (docs/audits/FileWrap.cpp.audit.md L154-171).
- Zero refs in tests/ and examples/; the caps are exercised only via Frame/FrameSet behavior, not named directly by any test.
- No duplicate definitions: MAX_IMAGES/MAX_IMAGE_SIZE/TAG_KEY_MAX/TAG_VAL_MAX are defined only here (the sole other MAX_IMAGES hit is SDL3's unrelated `AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED` enum in external/, not a collision). No macro collides with system headers or std::numeric_limits equivalents; nothing here re-implements a stdlib limit.
- Macro hygiene: no function-like macros, so the parenthesization/double-evaluation audit dimensions are vacuously clean. All values fit comfortably in `int`; comparisons against `size_t` at FrameSet.cpp L203/214/219/402 convert the positive int to `size_t` — no sign-compare hazard.
- Header reachability: included by Frame.cpp:35 and FrameSet.h:27 (the only two consumers); bin/utils/sync.sh:56 copies it verbatim to the editor project, so any cap change must be synced there.
- docs/compliance/src-checklist.md:134 tracks this file under hash e62b799121380518 — per AGENTS.md, do not bump the hash without rebuild + retest.
