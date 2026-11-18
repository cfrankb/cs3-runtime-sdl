# Tile table limits: `TILES_TOTAL_COUNT` vs `TOTAL_TILE_COUNT`

The two count constants have confusingly similar names and belong to
different tables. Never swap them.

| Constant | Header | Value | Table it bounds | Applies to |
|---|---|---|---|---|
| `TILES_TOTAL_COUNT` | `src/tilesdata.h` | `0xa5` (165) | `tileDefs[]` via `getTileDef()` | the **main layer** (`baseID() == 0`, `LAYER_MAIN`) — the **primary tileset** ("Tiles") |
| `TOTAL_TILE_COUNT` | `src/layerdata.h` | `256 * 3 = 768` | `g_layerdata[]` via `getLayerTileDef()` and the animator tables (`CAnimator::m_tileLayer`) | every **other layer** (`baseID() != 0`; `LAYER_FLOOR`/`LAYER_WALLS`/`LAYER_DECOR` — "LayerTiles") |

Routing rule: `layer->baseID() == 0` means the main layer using the
primary tileset; its cells index `tileDefs`. Cells of any layer with a
non-zero `baseID` index the layerdata/animator tables.
Runtime choke point: `src/game.cpp` (`baseID() == 0` branch feeding
`getTileDef`, else `getLayerTile`/`getLayerTileDef`).

An equivalent identifier is the layer's type (`src/layer.h`):

```cpp
enum LayerType : uint8_t
{
    LAYER_MAIN,   // uses Tiles (tileDefs / TILES_TOTAL_COUNT)
    LAYER_FLOOR,  // uses LayerTiles (g_layerdata / TOTAL_TILE_COUNT)
    LAYER_WALLS,  // uses LayerTiles
    LAYER_DECOR,  // uses LayerTiles
};
```

`LAYER_MAIN` uses **Tiles**; every other type uses **LayerTiles**.
The ingest sanitiser discriminates on `baseID()` rather than
`layerType()` because that is what the runtime consumers branch on.

## Enforcement

Maintainer ruling 2026-11-18: `CGame::sanitiseMapTiles()`
(`src/game.cpp`) applies exactly this split at the engine's map ingest
points (`loadLevel`, savegame `read`), replacing out-of-table cells with
blank/no-sprite and logging one warning with the total count.
`CMap`/`CLayer` are deliberately untouched. Pinned by
`t_game::testMapTileSanitiser`.

```cpp
const uint16_t limit = layer->baseID() == 0 ? TILES_TOTAL_COUNT  // tilesdata.h
                                            : TOTAL_TILE_COUNT;  // layerdata.h
```
