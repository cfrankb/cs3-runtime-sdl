#pragma once

#include <cstdint>

struct layerdata_t
{
    uint8_t nextTile;
    uint8_t animeSpeed;
    uint8_t tileType;
    uint8_t weight;
    const char *tag;
};

enum LayerTileType
{
    Background,
    Foreground,
    Solid,
    Deadly,
    Water,
};

extern layerdata_t g_layerdata[256];

constexpr inline const layerdata_t &getLayerTileDef(int i)
{
    return g_layerdata[i];
}
