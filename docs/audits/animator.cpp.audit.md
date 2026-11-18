# animator.cpp audit

Entry created 2026-11-18 during hostile-save-data burn-down (follow-on to
the map-ingest ruling; see `docs/spec/gamespec/tile-table-limits.md` and
the `CGame::sanitiseMapTiles` closure in `actor.cpp.audit.md`).

## FIXED / STAMPED

- **Hostile savegame animator tables** — `CAnimator::read` deserialized
  `m_tileMainLayer` (256 entries) and `m_tileLayer` (768 entries) raw.
  `m_tileLayer` entries become refIDs via `getLayerTile()` and reach
  `getLayerTileDef()` (backed by `g_layerdata[TOTAL_TILE_COUNT]`)
  unguarded at `game.cpp` scan-pos/granularity paths and
  `gamemixin.cpp` sprite routing; only `engine_hw.h` carried its own
  `< TOTAL_TILE_COUNT` guard. Fixed at ingest (same policy as
  `CGame::sanitiseMapTiles`, maintainer ruling 2026-11-18): entries
  `>= TOTAL_TILE_COUNT` are cleared to 0 ("not animated", the
  `reloadTileData()` invariant) with one `LOGW` carrying the total count.
  `m_tileMainLayer` needs no gate: its consumers (`TileSet::getTile`,
  `CFrameSet::operator[]`) are bounds-checked and return `nullptr`.
  No wire-format change; `ENGINE_VERSION` untouched.
  Pinned by `t_animator::test_animator_read_clamp`.

## Notes (verified, no action)

- `CAnimator::animate()` already defends its own `m_tileLayer` scan with
  a `>= TOTAL_TILE_COUNT` `LOGW` + `break`; with the load clamp it is
  belt-and-braces. `break` (vs `continue`) means one bad entry stops the
  rest of the tick's animation advance — unreachable from save data now.
- Test-hygiene finding 2026-11-18: `CFileMem` gates `read()`/`write()` on
  literal 'r'/'w' characters in the mode string; `"wb+"` (and `"w+b"`) are
  write-only. `test_animator_read_fail` previously "passed" through that
  gate instead of exercising truncation. Modes in `t_animator.cpp` fixed
  to the suite's round-trip idiom `"rw+"`.
