/*
    cs3-runtime-sdl
    Copyright (C) 2025 Francois Blanchette

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

#include "engine.h"
#include <SDL3/SDL.h>
#include "tileset_tex.h"
#include "font8x8.h"
#include "colormap.h"
#include "layerdata.h"
#include "layer.h"

class CMenu;
class CFrame;
class Tile;
class CAnimator;
class CGameMixin;

class EngineHW : public Engine
{

public:
    EngineHW(SDL_Renderer *renderer, SDL_Window *, const std::vector<std::string> &assetFiles, CAnimator *, MenuManager *, CGameMixin *, const int width, const int height);
    ~EngineHW();

    inline int getWidth()
    {
        return m_width;
    }

    inline int getHeight()
    {
        return m_height;
    }

    bool initGPUDevice();

    // override
    void drawScreen() override;
    void updateColorMaps(const ColorMaps &colormaps) override;
    void drawFont(const int x, const int y, const char *text, const Color color = WHITE, const int scaleX = 1, const int scaleY = 1) override;
    void drawMenu(CMenu &menu, const int baseX, const int baseY) override;
    void drawRect(const rect_t &rect, const Color &color, const bool fill) override;

protected:
    void drawGameStatus(const visualCues_t &visualcues);
    void drawSugarMeter(const int bx);
    void drawRect(SDL_Renderer *renderer, const SDL_FRect &rect, const SDL_Color &color, const bool filled);

    void drawViewPortStatic();
    void drawViewPortDynamic();
    void drawBossses(const int mx, const int my, const int sx, const int sy);
    void drawScroll();
    void drawEventText();
    void drawTimeout();
    void preloadHearts();
    void drawHealthBar(const bool isPlayerHurt);
    void fazeScreen();
    void flashScreen();
    void drawKeys();
    const Tile *getMainLayerTile(const uint8_t tileID);
    const Tile *calcSpecialFrame(const sprite_t &sprite);
    SDL_Texture *createTexture(CFrame *frame);
    CFrame *getFrame(const std::string &filename);

    // overloaded
    void preloadAssets();
    int drawTitlePix(int offsetY) override;

    inline void drawTile(const Tile *tile, const int x, const int y)
    {
        constexpr const float SCALE = 2;
        SDL_FRect dst = {
            (float)x * SCALE,
            (float)y * SCALE,
            tile->sw, //  tile->rect.w * SCALE,
            tile->sh, // tile->rect.h * SCALE
        };
        SDL_RenderTexture(m_renderer, tile->texture, &tile->rect, &dst);
    }

    struct overlay_t
    {
        const Tile *tile;
        const int x;
        const int y;
    };

    inline void drawViewPortInner(std::vector<overlay_t> &overlays, const CLayer *layer, const uint8_t &tileID, const int px, const int py)
    {
        if (tileID == 0)
            return;

        const Tile *tile = nullptr;
        if (layer->baseID() == 0)
        {
            tile = getMainLayerTile(tileID);
        }
        else if (tileID)
        {
            const uint8_t refID = m_animator->getLayerTile(tileID);
            tile = m_tileset_layers0.getTile(refID);
            const layerdata_t &data = getLayerTileDef(refID);
            if (data.tileType == LayerTileType::Foreground)
                overlays.emplace_back(overlay_t{tile, px, py});
        }
        if (tile)
        {
            drawTile(tile, px, py);
        }
    }

    constexpr inline uint32_t fazFilter(const int bitShift) const
    {
        return (0xff >> bitShift) << 16 |
               (0xff >> bitShift) << 8 |
               0xff >> bitShift;
    }

    SDL_Renderer *m_renderer;
    SDL_Window *m_window;
    SDL_Texture *m_textureTitlePix;
    TileSet m_tileset_tiles;
    TileSet m_tileset_animz;
    TileSet m_tileset_users;
    TileSet m_tileset_scroll;
    TileSet m_tileset_hearts;
    TileSet m_tileset_sheet0;
    TileSet m_tileset_sheet1;
    TileSet m_tileset_layers0;
    BitmapFont8x8 m_font;
};