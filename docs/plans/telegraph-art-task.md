# Telegraph art task (plan fx-boss-skills §5, deferred at step 6)

Boss skill casts are implemented (§7) but strike without a visual warning:
`bossSkillDef_t::telegraphFx` is `-1` for every row because no ground-marker
art exists on the fx sheet (`sfx_private.h` / `data/pixels/sfx.idx`).

To enable, per telegraphed skill (currently `LIGHTNING_STRIKE`, `METEOR`):

1. Author a small ground-marker strip (target ~32x16, a few frames) into the
   fx sheet assets, register a new `FX_*_BASEID`/`FRAMES` pair in
   `src/sfx_private.h` and sheet metadata (`data/pixels/sfx.idx`).
2. Add a `g_fxData` row in `src/gamesfx.cpp`: `FX_REPEATABLE`, short `ttl`
   (the marker should outlive nothing — it warns, never damages; no
   `data/fx` JSON), `align = FX_ALIGN_CENTER`.
3. Set `telegraphFx` (and a `telegraphTtl`) on the registry row in
   `src/bossdata.cpp`. The cast trigger already spawns the marker alongside
   the damaging fx when `telegraphFx >= 0`.

No code changes needed beyond the tables; the §7 cast path and §4 collision
are wired for it (telegraph fx without hitboxes never damages).

Placeholder meanwhile (2026-11-21): while a skill-owned fx runs its lead-in
frames, both renderers tint the origin tile 25% red (`fxDraw_t::telegraph`
from `CGame::getFxToDraw`; `CGameMixin::tintTelegraphTile` + the telegraph
branch in `EngineHW::drawFx`). When the real marker is enabled per row,
remove that red-tint pass (or gate it on `telegraphFx < 0`) so the two
hints don't stack.
