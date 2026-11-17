# Shared surface — sister-project consumers

> **STATUS 2026-11-18 (maintainer):** the external LGCK editor is
> **deprecated and no longer maintained**; the "verify against the editor
> before removing" caveat in the audits is moot for editor-only consumers.
> However, the `CFrame` and `CFrameSet` contracts remain **bound to other
> active projects** — their freezes stay in force, and their declared
> surface and behavior must not change without checking those consumers.
>
> **Ownership rule (`CFrame`/`CFrameSet`):** data is owned by the class
> unless a pointer is returned to the caller; `removeAll()`/`removeAt()`
> strip ownership and hand it back to the host, which must delete.
> `tests/t_frameset.cpp` pins this behavior.

Many files in this repo are not ours alone: `bin/utils/sync.sh` delivers
them to the external LGCK editor checkout
(`$TARGET_EDITOR=/disks/salmon/cfrankb/toolkit/qt/cs3-map-edit`, a
different machine). The authoritative inventory is `sync.sh` itself:

- `-g` runtime code: most of `src/` (actor, game, gamemixin, map, states,
  …) plus `src/shared/` (Frame, FrameSet, FileWrap, FileMem, IFile,
  dot.h, ss_limits, PngMagic). (`DotArray.*` and `CRC.h` staging in
  `external/lgck-editor/` deleted 2026-11-18 — deprecated by maintainer.)
- `-a`: `src/tiles*.{h,cpp}`, `src/animzdata*.h`. `-s`: `statedata.*`.
- In-repo second consumer: `android-project/app/jni/CMakeLists.txt`
  (lists `src/shared/*.cpp` explicitly).

Rules this implies:

1. **Never delete a synced file** to "remove dead code" — move it to
   `external/lgck-editor/` and keep `sync.sh` delivering it (precedent:
   `docs/bugs/resolved/dotarray-move-2026-11-18.md`).
2. **Frozen contracts** (maintainer directives, 2026-11-18): `IFile.h`,
   `CFrame` (`Frame.h`), `FrameSet.*`, `helper.cpp` dead surface. In-file
   DO-NOT-TOUCH notes carry the details.
3. Comment/whitespace edits are safe; anything changing a synced header's
   include graph or declared surface must be mirrored in `sync.sh` and
   flagged as an editor-side follow-up, since the editor build cannot be
   compiled or grepped from this machine.
