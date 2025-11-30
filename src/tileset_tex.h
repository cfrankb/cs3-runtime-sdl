
#pragma once
#include <cstdint>
#include "SDL3/SDL.h"
#include <string_view>
#include <vector>
#include "logger.h"

class CFrame;

struct Tile
{
    SDL_Texture *texture;
    SDL_FRect rect;
};

class TileSet
{

public:
    TileSet();
    ~TileSet();

    bool load(SDL_Renderer *renderer, const std::string_view &filename, const int sx, const int sy);
    bool load(SDL_Renderer *renderer, CFrame *frame, const int sx, const int sy);
    inline Tile *getTile(const uint16_t tileID)
    {
        if (tileID >= m_tiles.size())
        {
            LOGE("tileID: %u larger than max: %lu", tileID, m_tiles.size());
            return nullptr;
        }
        return &m_tiles[tileID];
    }

protected:
    SDL_Texture *createTexture(SDL_Renderer *renderer, CFrame *frame);
    std::vector<Tile> m_tiles;
    SDL_Texture *m_texture;
};