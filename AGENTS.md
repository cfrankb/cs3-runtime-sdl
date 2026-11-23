# Agent instructions

## Refresh CMakeLists.txt

run `python bin/refresh.py` to refresh stalled CMakieLists.txt in the `src/` and `tests/` folders.

## Tasks and runners

A maximum of only one task/one runner should run at all time.

## Documentation Architecture
Documentation is modular under `docs/` (migrated 2026-10-29 from the monolithic root files, which are now stubs).
- **Specification:** `docs/spec/` — 15 domain modules; `docs/spec/README.md` is the entry point.
- **Compliance:** `docs/compliance/` — modular validation sets; `docs/compliance/README.md` is the entry point.
- **Instruction:** Do not append large blocks to the legacy root stubs. Create targeted `.md` files in `docs/` subdirectories. Consult directory `README.md` files as your entry points.

plans that are fully implemented should be moved to `docs/plans/archive`.
bugs that are fixed should be moved to `docs/bugs/resolved`.
audits that are resolved should be moved to `docs/audits/finalized`.

## Ponytail

Follow the Ponytail rules in [ponytail/AGENTS.md](https://github.com/DietrichGebert/ponytail/blob/main/AGENTS.md) for all code changes.

## Error reporting in code

use LOGI, LOGW, LOGE and LOGF

## Never remove compatibility layers

Do not delete *any* compatibility layer, back-compat branch, or legacy
format handler as "dead code" — even when it has zero call sites or is not
compiled by the default build. Ask the maintainer first. Removed items so
far (all rejected and restored 2026-11-17): the `LOGF` macro and the
`USE_QFILE`/Qt block in `src/logger.h` and `src/shared/logger.h` (kept
byte-identical; see `docs/audits/logger.h.audit.md`, BUG-H16). `LOGF`
expands unbraced: call it as a standalone statement, never as the body of
a bare `if`.

Never remove the version logic that provides backward compatibility for
`CMap` or `CLayer`: the `CMap::MapVersion` enum and the `ver >= VERSIONn`
branches in `CMap::read` (`src/map.h`, `src/map.cpp`), and the version
branches in `CLayer::readCommon` (`src/layer.h`). New fields get a *new*
version level handled alongside the old branches — never widen an existing
branch.

When a change to serialized state (savegame/recorder/actor wire records)
removes or alters a field that another member implicitly carried, mirror
the serializer/deserializer pair *and* bump `ENGINE_VERSION`
(`src/game.cpp`) plus the mirrored `EXPECTED_VERSION` in `tests/t_game.cpp`
together — see BUG-H12.

Any modification to the **data structure** of `CActor`, `CMap`, or `CLayer`
(adds, removes, reorders or retypes members) is a serialized-state change
and requires the `ENGINE_VERSION` bump described above. Behavior-only
changes to their member functions do not.

## Library internal conventions.

Interface abstract IFile::read() and IFile::write() return IFILE_OK on success (and not byte count). This applies to CFileWrap, CFileMem, and QFileWrap.

## CFrame / CFrameSet data ownership

The `CFrame` and `CFrameSet` contracts are bound to **active external
projects** — do not change their declared surface or ownership behavior
without checking those consumers. Consider this rule before touching any
frame-owning API:

- Data is **owned by the class** unless a method returns the pointer.
- `removeAll()` (and `removeAt()`) **strip ownership** and hand it back to
  the host, which then owns and must `delete` the returned frame(s).
- Methods that return a raw `CFrame *` for inspection (e.g. `operator[]`,
  `currFrame()`) do **not** transfer ownership; the set keeps and frees it.

Ownership (maintainer ruling, leak review 2026-11-18): `set(i, frame)` is
delete-on-replace and `resize()` truncation deletes the dropped tail — the set
owns every slot it holds. Behavior pinned in `tests/t_frameset.cpp`.
`resize()` growth still inserts null slots: fill them before any dereference.
`toBmp` was deprecated and removed. `docs/shared-surface.md` carries the full
status.

## Session start
- Reload `docs/spec/README.md` (and the modules it indexes) and compare against `docs/compliance/README.md` (and its modules).


## Code formatting guideline
To ensure codebase consistency and minimize unnecessary version control diffs, all code generated or modified by AI agents must strictly comply with the project's formatting guidelines.

* **Formatting Standard:** All written, refactored, or edited C++ code must strictly conform to **Microsoft-style `clang-format`** or the local project `.clang-format` configuration.
* **VS Code Integration:** Code output must match the exact formatting rules enforced by the local Visual Studio Code environment to prevent automatic format-on-save actions from creating noisy commits and making change tracking difficult.
* **Line Breakers & Indentation:** Always use standard indentation (2 or 4 spaces depending on configuration), exact brace placement rules, and specified column limits before generating code block segments.
* **Verification Loop:** Agents should internalize the styling constraints prior to outputting block syntax, matching the target repository's mechanical style.


## Keep Compliance Documentation Current
- Track the SHA256 of the specific specification file/module being described.
- On any change to a specification or implementation:
  1. Rebuild and re-run the affected test suites.
  2. Flip relevant Open Items to verified.
  3. Update the hash, tally, and checkpoint rows.
- Never bump a hash before testing. Testing proves correctness; the hash merely detects change.

## Build & test (required before claiming "verified")
- Run: `cmake -B build && cmake --build build`
- Run: `ctest --test-dir build` (or execute a single suite binary like `build/spec_check2`; sources live in `tests/unit/`).
- Always build out-of-source (`cmake -B build`). Never allow generated files to touch the source root.


## CMake layout
- Maintain separate, independently buildable targets:
  - Library: `src/*.cpp` → `libqlang.a`
  - Demo: e.g., `examples/hello.cpp` (runner: `examples/qlang_run.cpp`); `examples/` holds demos only
  - Tests: `tests/unit/*.cpp` — one executable per feature area (defined in the specification), each registered with `add_test`
  - Script suites: `tests/scripts/*.ql` — QLang regression scripts driven by `qlang_run`

## Commits
- Suggest a commit name; do not commit automatically.
- Stage by concern. Keep unrelated doc edits out of feature commits.
- Exclude or delete patch artifacts (`*.orig`, `*.rej`).
- Check github-ci workflows for deprecated typescript modules and deprecated runners.

## Cross Compiler Compatibility
Consider g++, msvc, mingw, zig, clang. Qt and emscriptem/wasm compatibility when making chanhges to existing files.
Always fix and avoid pedantic compilation warnings.

## Edit this file
- Always ask for explicit user permission before editing AGENTS.md.
