/*
    cs3-runtime-sdl
    Copyright (C) 2025  Francois Blanchette

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <cstdint>
#include <vector>
#include <string_view>
#include <unordered_map>
#include "SDL3/SDL.h"
#include "logger.h"
#include "colormasks.h"
#include "shared/Frame.h"
#include "sheetdata.h"

class CFrame;

struct Tile
{
    SDL_Texture *texture;
    SDL_FRect rect;
    float sw;
    float sh;
};

struct bgra_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

class TileSet
{
public:
    TileSet();
    ~TileSet();

    bool load(SDL_Renderer *renderer, const std::string_view &filename, const int sx, const int sy);
    bool load(SDL_Renderer *renderer, CFrame *frame, const int sx, const int sy);
    bool load(SDL_Renderer *renderer, const std::string_view &filename, std::vector<tileinfo_t>);

    inline const Tile *getTile(size_t id, const ColorMask colorMask = ColorMask::COLOR_NOCHANGE)
    {
        if (colorMask == COLOR_NOCHANGE)
        {
            return (id < m_tiles.size()) ? &m_tiles[id] : nullptr;
        }
        else
        {
            if (m_cache.count(colorMask) == 0)
                return nullptr;
            auto &cache = m_cache[colorMask];
            return (id < cache.size()) ? &cache[id] : nullptr;
        }
    }

    size_t tileCount() const { return m_tiles.size(); }
    SDL_Texture *texture() const { return m_texture; }

    template <typename PixelCallback>
    inline bool cacheMask(SDL_Renderer *renderer, const ColorMask colorMask, const PixelCallback pixelCallBack)
    {
        if (m_textures.count(colorMask) != 0)
        {
            // delete previous cache
            SDL_DestroyTexture(m_textures[colorMask]);
            m_textures.erase(colorMask);
        }

        CFrame frame{*m_frame};
        for (auto &pixel : frame.getRGB())
        {
            pixelCallBack(pixel);
        }

        auto &cache = m_cache[colorMask];
        SDL_Texture *texture = createTexture(renderer, &frame);
        if (!texture)
            return false;

        m_textures[colorMask] = texture;
        return saveTiles(texture, cache, &frame);
    }

private:
    SDL_Texture *createTexture(SDL_Renderer *renderer, CFrame *frame);
    bool saveTiles(SDL_Texture *texture, std::vector<Tile> &tiles, CFrame *frame);

private:
    int m_scale;
    int m_sx;
    int m_sy;
    SDL_Texture *m_texture;
    CFrame *m_frame;
    std::vector<Tile> m_tiles;
    std::unordered_map<ColorMask, std::vector<Tile>> m_cache;
    std::unordered_map<ColorMask, SDL_Texture *> m_textures;
};
