# Audit: src/bossdata.cpp

Date: 2026-11-17

## Summary

The loader is well-structured (recursive-descent JSON, mirrored parse.py INI rules, no unsafe C string functions, no leaks — the intentional leak-pool is matched by freeBossData()). However it is unsafe against malformed/modded data files: 4 Critical findings (null-deref of JSON lookups, hitbox `count` overrun that reads OOB in boss.cpp, unchecked `e[1]` on INI lines) and several High validation/UB gaps. Counts: 4 Critical, 2 High, 7 Medium, 11 Low.

## Findings

### Critical

- `L390-392, L396-400`: [bug][ub] **FIXED 2026-11-17** (docs/bugs/resolved/critical-triage-2026-11-17.md BUG-C5; `test_bossdata_hostile`). Original finding: `frame->find("width")->asInt()` and `hb.find("x"/"y"/"w"/"h"/"type")->asInt()` dereference `find()` results without null checks; also `hb` is never checked to be an Object. Malformed JSON (missing key, array of non-objects) crashes with a null deref. Check every `find()` result (and `hb.type == Object`) and LOGE+fail on absence.
- `L833-835`: [bug][ub] **FIXED 2026-11-17** (BUG-C5; `test_bossdata_hostile`). Original finding: `s.count = fr.boxes.size()` records the full box count while only `MAX_HITBOX_PER_FRAME` (4) boxes are copied into the fixed array. Consumer `src/boss.cpp:87` iterates `i < hbData->count` over `hitboxes[4]` → out-of-bounds read when any frame accumulates >4 boxes (possible: `hitboxFrames` accumulates across all JSON files keyed by absolute frame id, so duplicate/repeated files add up). Use `s.count = min(boxes.size(), MAX_HITBOX_PER_FRAME)` and LOGW when truncating.
- `L740, L757`: [bug][ub] **FIXED 2026-11-17** (BUG-C5; `test_bossdata_hostile`). Original finding: `handleSeq` uses `e[1]` (`e[1].front()`, `atoi(e[1])`) with no `e.size() >= 2` guard; an INI line consisting of just `moving` (or `attack # comment` after stripping) does `operator[]` past the vector's end → UB. Guard `e.size() < 2` with LOGW+return.
- `L697-712`: [bug][ub] **FIXED 2026-11-17** (BUG-C5; `test_bossdata_hostile`). Original finding: scalar attribute branches (`type`, `hp`, `score`, `path`, `attack_sound`, `is_goal`, `show_details`, `aims`) all take `e[1]` unchecked; a bare keyword line (argument eaten by comment strip or missing) indexes out of bounds. Guard `e.size() < 2` once before the chain with LOGW.

### High

**FIXED 2026-11-17 (BUG-C5 + BUG-H6b):** width/height/cols > 0 and negative coords rejected. **`L390-408`: [validation][ub] no range validation on JSON numbers: `frameWidth`/`frameHeight` of 0 → integer division by zero at L404-405 (`x / frameWidth`); negative `x`/`y` produce negative col/row/id and colliding negative `frame_id`. Validate width/height/cols > 0 and x/y >= 0, LOGE+reject the box otherwise.
**FIXED 2026-11-17 (BUG-H6b):** `asInt()` saturates. **`L86, L390-400`: [ub] `asInt()` is `static_cast<int>(num)`; a JSON number outside int range (e.g. `1e20`) is UB per the standard. Clamp to INT_MIN/INT_MAX (or reject) in `asInt()`.

### Medium

- `L129-146`: [ub] **FIXED 2026-11-18** (depth cap 64 in `JsonParser::parseValue`, `test_bossdata_hostile` deep-nesting fixture). Original finding: JSON parser recurses unboundedly (`parseValue`→`parseObject`/`parseArray`→`parseValue`); `[[[[...` in a modded file overflows the stack. Add a depth cap (e.g. 64) with LOGE.
- `L736-738`: [error-handling] **FIXED 2026-11-18** (`handleSeq` now sets base/len and advances the frame counter even when `readHitboxFile` fails; LOGW + continue). Original finding: when `readHitboxFile` fails, `handleSeq` returns before setting `<seq>_base/_len` and before advancing `frame` — the boss silently loses that animation and every later sequence's frames shift down. Consider setting base/len anyway, or aborting the section with LOGE.
- `L743-754`: [error-handling] **FIXED 2026-11-18** (LOGW with line number for undefined `@ref`). Original finding: an `@ref` naming a sequence that was never defined is silently ignored (no LOGW); the field pair defaults to base=0/len=0. Add LOGW with line number.
- `L731-738`: [bug] **FIXED 2026-11-18** (verified against `docs/spec/bosses/parse.py` L483-487: the spec prints "hitbox file ... ignored" for `@ref` lines; `handleSeq` now skips and warns instead of keying boxes at the wrong frame). Original finding: (needs verification) a hitbox file on an `@ref` line is registered with `baseFrame = frame` (the current counter), but the animation itself reuses the referenced sequence's base — hitboxes land on frame ids the animation never reaches. Compare against parse.py's handling of `@ref` + hitbox file.
- `L761-762`: [validation] **FIXED (in code before this stamp; LOGW clamps at `handleSeq` and `build()`)**: `aims` <= 0 normalized to 1 at load. Original finding: `aims` of 0 makes `frame` stall (sequences overlap); negative makes it go backwards. Clamp to >= 1 with LOGW.
- `L462-491, L630, L757`: [validation][error-handling] **FIXED 2026-11-18** (`toInt`/`toU8` helpers using `strtol` with full-consume + range checks and LOGW replace bare `atoi`/`strtoul`-to-`uint8_t` in `build()`; negative sequence frame counts clamp to 0 with LOGW; garbage `hp` covered by `test_bossdata_hostile`). Original finding: every numeric field goes through bare `std::atoi`, which returns 0 for garbage ("hp = fast" → hp 0 silently); `L465/L474` cast `strtoul`/`atoi` results to `uint8_t`, silently wrapping negatives and >255. At this trust boundary, validate with `strtol`/range checks and LOGW on failure.
- `L79-85`: [validation] **FIXED 2026-11-18** (`parseObject` LOGWs duplicate keys; first-wins documented). Original finding: duplicate keys in a JSON object: `find()` silently returns the first occurrence (JSON convention is last-wins). Low impact but worth one line in `parseObject` or a comment.

### Low

- `L546, L640, L647`: [dead] `defines` is collected "for completeness / diagnostics" and never read anywhere else — delete.
- `L447, L622`: [dead] `BossAccum::sheet` is assigned but never read (`build()` takes `sheetIndex` as a parameter) — delete the member.
- `L611-612`: [dead] `if (sec.front() == '[')` re-tests what L607 already guaranteed.
- `L576-582`: [shrink] `flush()` does `seq = BossAccum{}` after `pushBoss()` already reset it at L560 — drop one of the two resets.
- `L35, L39`: [dead] `<algorithm>` and `<cstdio>` unused.
- `L807-809, L840-842, L859-861`: [shrink] three heap-`new`ed static vectors with hand-rolled delete/reset; plain file-static `std::vector`s with `.clear()`/`.swap()` remove the `new`/`delete` and the leak-pool comment.
- `L766-782`: [shrink] `splitLines` copies the whole file into a `vector<string>`; iterating `text.find('\n')` (or `std::string_view`) in the load loop avoids the copy.
- `L563-565`: [yagni] `hitboxDir` parameter accepted then `(void)`-discarded; either use it or drop it from the API (header L177 documents it as meaningful).
- `L422-430`: [shrink] linear scan over `frames` per hitbox is O(n²); fine for 4 bosses' data, revisit only if hitbox counts grow (a `map<int,size_t>` index is the upgrade path).
- `L607-624`: [error-handling] duplicate section names (same boss twice) are silently both appended; `getBossData` returns the first, the second is dead weight. LOGW on duplicate.
- `L219-260`: [validation] `parseString` has no `\uXXXX` support and the default escape branch (L248-250) silently maps unknown escapes to the raw char (e.g. `\u` → `u`); acceptable for the fixed schema but should LOGW or reject.
- `L659-672`: [error-handling] a composite token like `color =red` (empty key before `=`) or a typo'd composite key stores a garbage field name that is never read and never warned about.

## Notes

- All C string handling is `std::string`-based; no strcpy/sprintf/fixed buffers found, no `reinterpret_cast`, no unchecked `fopen` (FileWrap checked). The leak-pool (L805-809) is deliberate and freed by `freeBossData()`.
- L844-847 correctly re-points `d.name` into `g_pooledNames` after the move; `namePool` and `bosses` are only mutated together in `pushBoss`, so the `.at(i)` pairing holds — but it is fragile if pushBoss ever grows a branch.
- L302 `strtod(start, nullptr)` relies on the caller's buffer being NUL-terminated at `end`; true for the only call site (L376, `std::string::data()`), but the parser API is unsafe for arbitrary buffers — worth a comment.
- Runtime-verify against `techdocs/bosses/parse.py`: (1) **RESOLVED 2026-11-18** — spec (`docs/spec/bosses/parse.py` L483-487) ignores a hitbox file on an `@ref` line; loader now matches; (2) whether a section with a header but zero fields should produce a default `bossData_t` (pushBoss L555 admits it; `type` 0 then collides in `getBossData` scans); (3) whether `>>>sheet` mid-section is legal in real bosses.ini — if so, `build(sheet)` at L557 uses the sheet at flush time while frames/hitboxes were keyed with the old sheet.
- The `count` overrun (Critical #2) is reachable only if a frame accumulates >4 boxes; check real hitbox JSONs for frames with 5+ same-cell boxes to gauge exposure.
