# DotArray move — CDotArray out of the runtime (2026-11-18)

Maintainer decisions recorded this round (docs/audits/DotArray.cpp.audit.md
dead-cluster finding, per-file sections of IFile.h / helper.cpp /
Frame.cpp / FrameSet.cpp audits):

- **helper.cpp** — dead surface KEPT; DO-NOT-TOUCH note at top of file.
- **IFile.h** — interface contract FROZEN; DO-NOT-TOUCH note at top.
- **Frame.h (CFrame)** — contract FROZEN; DO-NOT-TOUCH note at top.
- **FrameSet.*** — untouched per maintainer; dead surface stays.
- **DotArray** — MOVED.

## What moved

`src/shared/DotArray.{h,cpp}` and `tests/t_dotarray.{h,cpp}` →
`external/lgck-editor/` (staging dir served to the editor by
`bin/utils/sync.sh -g`). Staged `DotArray.h` now takes the `Dot` POD from
`src/shared/dot.h` instead of defining it (no redefinition once sync.sh
delivers dot.h; editor must drop any stale Dot copy from its old header).

Runtime keeps only what `CFrame` needs: new `src/shared/dot.h` with the
byte-identical `Dot` struct. Removed `DotArray.cpp` from
`src/shared/CMakeLists.txt` and `android-project/app/jni/CMakeLists.txt`
(it shipped dead in the Android binary), removed `t_dotarray` from
`tests/CMakeLists.txt` + `tmain.cpp`, dropped the unused
`class CDotArray;` forward decl from Frame.h.

## Verification

`cmake -B build && cmake --build build` clean; `ctest --test-dir build`
1/1 passed (2026-11-18). No serialized-state change → no ENGINE_VERSION
bump. Compliance checklist row/hashes updated for dot.h, Frame.h/cpp,
IFile.h, helper.cpp; src-coverage DotArray row marked stale.

## Editor follow-up (cannot be done from this machine)

On the box holding `/disks/salmon/cfrankb/toolkit/qt/cs3-map-edit`:
run `sync.sh -g`, ensure its build compiles `DotArray.cpp` from
src/shared with dot.h present, and delete any editor-local duplicate
`Dot` definition before its next compile.
