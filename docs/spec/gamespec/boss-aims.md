# Boss frame layouts (`aims`, `mirror`)

Authoritative implementation: `src/boss.cpp` (`CBoss::currentFrame`,
`isMirrored`, `testHitbox1`), `src/bossdata.cpp` (ini parse +
validation). Pinned by `tests/unit/t_boss.cpp::test_boss_aims3` and
`tests/unit/t_bossdata.cpp::test_bossdata_hostile`.

## `aims` (per boss, `bosses.ini`)

Number of direction blocks stored per animation in the boss sheet.
A sheet block for an animation with `count` frames per direction
occupies `count * aims_effective` physical frames; frame accounting is
`frame += count * aims` at load.

| aims | directions | `currentFrame()` slot |
|------|------------|------------------------|
| 1    | shared     | slot 0 always          |
| 2    | UP, DOWN   | `aim % 2` (LEFT→0, RIGHT→1) |
| 3    | UP, DOWN, one horizontal | slot table below |
| 4    | UP, DOWN, LEFT, RIGHT (`JoyAim` order) | `aim % 4` |

Legacy pin: for aims 1/2/4 the modulo path (including `AIM_NONE` →
`255 % aims`) is unchanged historical behavior.

## `aims 3` + required `mirror left|right`

One horizontal direction's art is a horizontal mirror of the other's;
the sheet stores only UP, DOWN and ONE horizontal block (25% storage
saving). `mirror` names the **derived** direction:

- `mirror left` → sheet holds UP, DOWN, RIGHT; LEFT is drawn flipped.
- `mirror right` → sheet holds UP, DOWN, LEFT; RIGHT is drawn flipped.
- `aims 3` without a valid `mirror` → `LOGE` + degrade to `aims 1`.
- `mirror` with `aims != 3` → `LOGW` + ignored.

Slot table (identical for both mirror values):

| aim | slot |
|-----|------|
| UP | 0 |
| DOWN | 1 |
| LEFT | 2 (shared horizontal block) |
| RIGHT | 2 (shared horizontal block) |
| `AIM_NONE` (default/on-disk) | 0 (normalised to UP) |

`CBoss::isMirrored()` is true exactly when `aims == 3` and the current
aim equals `data->mirror`.

## Rendering of a mirrored aim

Both renderers draw the slot-2 frame with a horizontal flip and the
sprite rect anchored as `bRect.x = GRID * (x + hb.x) - frameW`
(normal: `GRID * (x - hb.x)`). HW: `SDL_FlipMode` param on
`CGameEngine::drawTile`; SW: trailing `bool flipX` on the `rect_t`
`CGameMixin::drawTile` (mirrors about the full tile width, so clipped
blits stay correct). `CFrame::flipH()` must never be used — it mutates
the shared sheet frame.

`CBoss::warnAsymAnchor()` emits a one-shot `LOGW` per boss type when
the main hitbox anchor is not centred in the frame (`hb.x !=
frameW_half - hb.x - hb.width`); warn only, no rejection.

## Hitboxes of a mirrored aim

- Main hitbox: aim-independent, placed at `(m_x, m_y)` — unchanged.
- Per-frame secondary boxes (`boss.cpp::testHitbox1`): authored in
  cell-relative half-tiles for the **provided** direction; when
  `isMirrored()` each box reflects in boss space:
  `hb.x = m_x - (c.x - hbMain.x) - c.width` (normal:
  `m_x + c.x - hbMain.x`). This is the exact mirror of the renderer
  flip transform; no per-box JSON change exists or is needed.
- Hitbox JSON contract: `frame.cols` must match the sheet strip grid;
  boxes appear only in the UP / DOWN / provided-horizontal blocks. A
  box whose derived `raw_id` falls outside the sequence's
  `[0, count * aims)` block (phantom direction, misaligned `cols`)
  would land in the next boss's frame IDs — the loader (`handleSeq` ->
  `readHitboxFile`) must drop it and report `LOGE`; the surviving
  entries stay keyed to their own sequence's block.

## Storage note

`aims`/`mirror` are ini-derived runtime data inside `bossData_t`;
neither is serialized (save/load persists actor state only), so no
`ENGINE_VERSION` bump is involved.
