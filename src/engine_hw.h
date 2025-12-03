#pragma once

#include "engine.h"
#include <SDL3/SDL.h>
#include "tileset_tex.h"
#include "font8x8.h"
#include "colormap.h"

class CMenu;
class CFrame;
class Tile;
class CAnimator;
class CGameMixin;

class EngineHW : public Engine
{

public:
    EngineHW(SDL_Renderer *renderer, const std::vector<std::string> &assetFiles, CAnimator *, MenuManager *, CGameMixin *, const int width, const int height);
    ~EngineHW() {};

    inline int getWidth()
    {
        return m_width;
    }

    inline int getHeight()
    {
        return m_height;
    }

    // override
    void drawScreen() override;
    void updateColorMaps(const ColorMaps &colormaps) override;
    void drawFont(const int x, const int y, const char *text, const Color color = WHITE, const int scaleX = 1, const int scaleY = 1) override;
    void drawMenu(CMenu &menu, const int baseX, const int baseY) override;

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
    const Tile *tile2Frame(const uint8_t tileID);
    const Tile *calcSpecialFrame(const sprite_t &sprite);

    // overloaded
    void preloadAssets();

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

    constexpr inline uint32_t fazFilter(const int bitShift) const
    {
        return (0xff >> bitShift) << 16 |
               (0xff >> bitShift) << 8 |
               0xff >> bitShift;
    }

    SDL_Renderer *m_renderer;
    TileSet m_tileset_tiles;
    TileSet m_tileset_animz;
    TileSet m_tileset_users;
    TileSet m_tileset_scroll;
    TileSet m_tileset_hearts;
    TileSet m_tileset_sheet0;
    TileSet m_tileset_sheet1;
    BitmapFont8x8 m_font;
};