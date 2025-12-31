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
#include "engine_hw.h"
#include <unordered_map>
#include "menu.h"
#include "color.h"
#include "chars.h"
#include "game.h"
#include "statedata.h"
#include "gamestats.h"
#include "tilesdata.h"
#include "boss.h"
#include "gamesfx.h"
#include "attr.h"
#include "tilesdefs.h"
#include "states.h"
#include "animator.h"
#include "strhelper.h"
#include "assetman.h"
#include "colormap.h"
#include "menumanager.h"
#include "layerdata.h"
#include "gameui.h"
#include "shared/FileMem.h"
#include "shared/FrameSet.h"

#define TRACE(...)            \
    if (m_game->level() == 4) \
        LOGI(__VA_ARGS__);

namespace EngineHW_Private
{
    constexpr const int FONT_SIZE = 8;
    constexpr const int SCALE = 2;
    constexpr const float SCALEF = 2;
    constexpr const int SCALE_2X = 4;

    namespace SColor
    {
        constexpr SDL_Color WHITE{255, 255, 255, 255};           // #ffffff
        constexpr SDL_Color YELLOW{255, 255, 0, 255};            // #ffff00
        constexpr SDL_Color CYAN{0, 255, 255, 255};              // #00ffff
        constexpr SDL_Color GREEN{0, 255, 0, 255};               // #00ff00
        constexpr SDL_Color PURPLE{255, 0, 255, 255};            // #ff00ff
        constexpr SDL_Color LIGHTGRAY{0xa9, 0xa9, 0xa9, 0xff};   // #a9a9a9
        constexpr SDL_Color DEEPSKYBLUE(0x00, 0xBF, 0xFF, 0xff); // #00bfff
        constexpr SDL_Color BLACK(0x00, 0x00, 0x00, 0xff);       // #000000
        constexpr SDL_Color DARKGREEN(0, 128, 0, 255);           // #008000
        constexpr SDL_Color RED(255, 0, 0, 255);                 // #ff0000
        constexpr SDL_Color ORANGE(0xf5, 0x9b, 0x14, 255);       // #f59b14
        constexpr SDL_Color DARKORANGE(0xff, 0x8c, 0x00, 255);   // #ff8c00
        constexpr SDL_Color CORAL(0xff, 0x7f, 0x50, 255);        // #ff7f50
        constexpr SDL_Color PINK(0xff, 0xc0, 0xcb, 255);         // #ffc0cb
        constexpr SDL_Color HOTPINK(0xff, 0x69, 0xb4, 255);      // #ff69b4
        constexpr SDL_Color DEEPPINK(0xff, 0x14, 0x93, 255);     // #ff1493
        constexpr SDL_Color DARKGRAY(0x44, 0x44, 0x44, 255);     // #444444

        auto toSColor = [](const uint32_t c) -> const SDL_Color
        {
            SDL_Color out;
            out.r = c & 0xFF;
            out.g = (c >> 8) & 0xFF;
            out.b = (c >> 16) & 0xFF;
            out.a = 255;
            return out;
        };
    };
};

using namespace EngineHW_Private;

EngineHW::EngineHW(SDL_Renderer *renderer, SDL_Window *window, const std::vector<std::string> &assetFiles, CAnimator *animator, MenuManager *menus, CGameMixin *mixin, const int width, const int height)
    : Engine(), m_renderer(renderer)
{
    m_window = window;
    m_width = width;
    m_height = height;
    m_game = CGame::getGame();
    m_animator = animator;
    m_menus = menus;
    m_gameMixin = mixin;
    setAssetFiles(assetFiles);
    preloadAssets();
};

EngineHW::~EngineHW()
{
    if (m_textureTitlePix)
        SDL_DestroyTexture(m_textureTitlePix);
}

bool EngineHW::initGPUDevice()
{
    //    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
    return true;
}

//////////////////////////////////////////////////////////////////////

void EngineHW::preloadAssets()
{
    if (!m_font.loadRawFont("data/bitfont.bin"))
    {
        LOGE("cannot load font");
    }
    m_font.buildAtlas(m_renderer);

    enum
    {
        ASSET_TILES,
        ASSET_ANIMZ,
        ASSET_USERS,
        ASSET_SHEET0,
        ASSET_SHEET1,
        ASSET_UISHEET,
        ASSET_TITLEPIX,
        ASSET_LAYERS0,
    };

    m_tileset_tiles.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_TILES], TILE_SIZE, TILE_SIZE);
    m_tileset_animz.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_ANIMZ], TILE_SIZE, TILE_SIZE);
    m_tileset_users.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_USERS], TILE_SIZE, TILE_SIZE);
    m_tileset_scroll.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_UISHEET], 16, 48);
    m_tileset_layers0.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_LAYERS0], TILE_SIZE, TILE_SIZE);

    LOGI("Creating texture variants");
    const uint32_t colorFilter = fazFilter(FAZ_INV_BITSHIFT);
    m_tileset_users.cacheMask(m_renderer, COLOR_FADE, [colorFilter](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color = ((color >> FAZ_INV_BITSHIFT) & colorFilter) | ALPHA;
                                  //
                              });

    m_tileset_users.cacheMask(m_renderer, COLOR_INVERTED, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color ^= 0x00ffffff;
                                  //
                              });

    m_tileset_users.cacheMask(m_renderer, COLOR_GRAYSCALE, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                  {
                                      uint8_t *c = reinterpret_cast<uint8_t *>(&color);
                                      const uint16_t avg = (c[0] + c[1] + c[2]) / 3;
                                      c[0] = c[1] = c[2] = avg;
                                  } //
                              });

    m_tileset_users.cacheMask(m_renderer, COLOR_ALL_WHITE, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color = WHITE;
                                  //
                              });

    m_tileset_sheet0.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_SHEET0], g_sheet0_data);
    m_tileset_sheet1.load(m_renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_SHEET1], g_sheet1_data);

    const std::string filepath = AssetMan::getPrefix() + "pixels/" + m_assetFiles[ASSET_TITLEPIX];
    CFrame *frame = getFrame(filepath);
    if (frame)
    {
        m_textureTitlePix = createTexture(frame);
        if (m_textureTitlePix)
            SDL_SetTextureScaleMode(m_textureTitlePix, SDL_SCALEMODE_NEAREST);
    }

    preloadHearts();
}

void EngineHW::drawMenu(CMenu &menu, const int baseX, const int baseY)
{
    m_menus->setLastMenu(&menu, baseX, baseY);
    const int scaleX = menu.scaleX();
    const int scaleY = menu.scaleY();
    const int paddingY = menu.paddingY();
    const int spacingY = scaleY * FONT_SIZE + paddingY;
    for (size_t i = 0; i < menu.size(); ++i)
    {
        const CMenuItem &item = menu.at(i);
        const std::string &text = item.str();
        const int bx = baseX == -1 ? (getWidth() - text.size() * FONT_SIZE * scaleX) / 2 : baseX;
        const bool selected = static_cast<int>(i) == menu.index();
        Color color = selected ? YELLOW : BLUE;
        if (item.isDisabled())
        {
            color = LIGHTGRAY;
        }
        int x = bx;
        const int y = baseY + i * spacingY;
        if (static_cast<int>(i) == menu.index() && !menu.isCaretDisabled())
        {
            // draw red triangle
            uint8_t tmp[2];
            tmp[0] = CHARS_TRIGHT - CHARS_CUSTOM + CHARS_CUSTOM_BASE + ' ';
            tmp[1] = '\0';
            m_font.drawText(m_renderer, (char *)tmp, SColor::RED, nullptr, 32 * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
            // drawFont(bitmap, 32, y, (char *)tmp, RED, CLEAR, scaleX, scaleY);
        }
        if (item.type() == CMenuItem::ITEM_BAR)
        {
            const int scaleX = 1;
            size_t len = 0;
            for (size_t j = 0; j < item.size(); ++j)
                len += item.option(j).size() + (size_t)(j != 0);

            x = (getWidth() - len * FONT_SIZE * scaleX) / 2;
            for (size_t j = 0; j < item.size(); ++j)
            {
                const Color color = selected && (j == (size_t)item.value()) ? YELLOW : BLUE;
                const std::string option = item.option(j);
                //                drawFont(bitmap, x, y, option.c_str(), BLACK, color, scaleX, scaleY);
                m_font.drawText(m_renderer, option.c_str(), SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
                x += (FONT_SIZE + 1) * option.size() * scaleX;
            }
            continue;
        }
        else if (item.role() == MENU_ITEM_SELECT_USER)
        {
            const uint16_t animeOffset = selected ? (m_ticks / 3) & 0x1f : 0;
            // const CFrameSet &users = *m_users;
            // CFrame &frame = *users[PLAYER_TOTAL_FRAMES * item.userData() + PLAYER_DOWN_INDEX + animeOffset];
            // drawTileFaz(bitmap, x, y, frame, 0, selected ? COLOR_NOCHANGE : COLOR_GRAYSCALE);
            const Tile *tile = m_tileset_users.getTile(PLAYER_TOTAL_FRAMES * item.userData() + PLAYER_DOWN_INDEX + animeOffset,
                                                       selected ? COLOR_NOCHANGE : COLOR_GRAYSCALE);
            drawTile(tile, x, y);
            x += 32;
        }
        // drawFont(bitmap, x, y, text.c_str(), color, CLEAR, scaleX, scaleY);
        m_font.drawText(m_renderer, text.c_str(), SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
    }
}

void EngineHW::drawFont(const int x, const int y, const char *text, Color color, const int scaleX, const int scaleY)
{
    m_font.drawText(m_renderer, text, SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
}

void EngineHW::drawScreen()
{

    // TRACE("enter drawScreen");
    const CGame &game = *m_game;

    // draw viewport
    if (m_state.m_cameraMode == CAMERA_MODE_DYNAMIC)
    {
        drawViewPortDynamic();
    }
    else if (m_state.m_cameraMode == CAMERA_MODE_STATIC)
    {
        drawViewPortStatic();
    }

    const int flash = game.statsConst().at(S_FLASH);
    if (flash)
        flashScreen();

    const int hurtStage = game.statsConst().at(S_PLAYER_HURT);
    const bool isPlayerHurt = hurtStage != CGame::HurtNone;
    //  if (isPlayerHurt)
    //      for (int i = 0; i < SCREEN_SHAKES; ++i)
    //          bitmap.shiftLEFT(false);

    // visual cues
    const visualCues_t visualcues{
        .diamondShimmer = game.goalCount() < m_visualStates.rGoalCount,
        .livesShimmer = game.lives() > m_visualStates.rLives,
    };
    m_visualStates.rGoalCount = game.goalCount();
    m_visualStates.rLives = game.lives();

    // draw game status
    drawGameStatus(visualcues);

    // draw bottom rect
    const bool isFullWidth = getWidth() >= MIN_WIDTH_FULL;

    if (m_state.m_currentEvent >= MSG0)
    {
        drawScroll();
    }
    else if (m_state.m_currentEvent == EVENT_SUGAR)
    {
        constexpr const float tileSize = TILE_SIZE;
        const SDL_Color bgColor = isFullWidth && m_state.m_currentEvent >= MSG0 ? SColor::WHITE : SColor::DARKGRAY;
        const SDL_Color borderColor = isPlayerHurt              ? SColor::PINK
                                      : visualcues.livesShimmer ? SColor::GREEN
                                                                : SColor::LIGHTGRAY;
        SDL_FRect rect{0, SCALEF * (getHeight() - tileSize), SCALEF * getWidth(), SCALEF * tileSize};
        drawRect(m_renderer, rect, bgColor, true);
        drawRect(m_renderer, rect, borderColor, false);
    }

    // draw current event text
    drawEventText();

    if (!isFullWidth || m_state.m_currentEvent < MSG0)
    {
        // draw Healthbar
        drawHealthBar(isPlayerHurt);

        // draw keys
        drawKeys();
    }

    // drawButtons
    // if (m_ui.isVisible())
    //    drawUI(bitmap, m_ui);

    // draw timeout
    drawTimeout();

    // if (m_state.m_gameMenuActive)
    if (m_menus->isMenuActive(MENUID_GAMEMENU))
    {
        fazeScreen();
        resizeGameMenu();
        drawMenu(*m_menus->get(MENUID_GAMEMENU), -1, (getHeight() - m_menus->get(MENUID_GAMEMENU)->height()) / 2);
    }

    // TRACE("leave drawScreen");
}

void EngineHW::drawGameStatus(const visualCues_t &visualcues)
{
    CGame &game = *m_game;
    char tmp[32];

    std::string_view msg;
    if (m_state.m_paused)
    {
        msg = "PRESS [F4] TO RESUME PLAYING...";
    }
    else if (m_state.m_prompt == PROMPT_ERASE_SCORES)
    {
        msg = "ERASE HIGH SCORES, CONFIRM (Y/N)?";
    }
    else if (m_state.m_prompt == PROMPT_RESTART_GAME)
    {
        msg = "RESTART GAME, CONFIRM (Y/N)?";
    }
    else if (m_state.m_prompt == PROMPT_LOAD)
    {
        msg = "LOAD PREVIOUS SAVEGAME, CONFIRM (Y/N)?";
    }
    else if (m_state.m_prompt == PROMPT_SAVE)
    {
        msg = "SAVE GAME, CONFIRM (Y/N)?";
    }
    else if (m_state.m_prompt == PROMPT_HARDCORE)
    {
        msg = "HARDCORE MODE, CONFIRM (Y/N)?";
    }
    else if (m_state.m_prompt == PROMPT_TOGGLE_MUSIC)
    {
        msg = m_state.m_musicMuted ? "PLAY MUSIC, CONFIRM (Y/N)?"
                                   : "MUTE MUSIC, CONFIRM (Y/N)?";
    }

    if (!msg.empty())
    {
        m_font.drawText(m_renderer, msg.data(), SColor::LIGHTGRAY, &SColor::BLACK, 0, Y_STATUS * SCALE, SCALE, SCALE);
    }
    else
    {
        int tx;
        int bx = 0;
        tx = snprintf(tmp, sizeof(tmp), "%.8d ", game.score());
        // drawFont(bitmap, 0, Y_STATUS, tmp, WHITE);
        m_font.drawText(m_renderer, tmp, SColor::WHITE, &SColor::BLACK, 0, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;
        tx = snprintf(tmp, sizeof(tmp), "DIAMONDS %.2d ", game.goalCount());
        // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, tmp, visualcues.diamondShimmer ? DEEPSKYBLUE : YELLOW);
        m_font.drawText(m_renderer, tmp, visualcues.diamondShimmer ? SColor::DEEPSKYBLUE : SColor::YELLOW, &SColor::BLACK, bx * FONT_SIZE * SCALE, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;
        tx = snprintf(tmp, sizeof(tmp), "LIVES %.2d ", game.lives());
        // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, tmp, visualcues.livesShimmer ? GREEN : PURPLE);
        m_font.drawText(m_renderer, tmp, visualcues.livesShimmer ? SColor::GREEN : SColor::PURPLE, &SColor::BLACK, bx * FONT_SIZE * SCALE, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;

        /*
        if (m_recorder->isRecording())
        {
            // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, "REC!", WHITE, RED);
            m_font.drawText(
                m_renderer,
                "REC!",
                SColor::RED,
                &SColor::WHITE,
                bx * FONT_SIZE * SCALE,
                Y_STATUS * SCALE, SCALE, SCALE);
        }
        else if (m_recorder->isReading())
        {
            // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, "PLAY", WHITE, DARKGREEN);
            m_font.drawText(
                m_renderer,
                "PLAY",
                SColor::WHITE,
                &SColor::DARKGREEN,
                bx * FONT_SIZE * SCALE,
                Y_STATUS * SCALE, SCALE, SCALE);
        }
                */
        if (getWidth() >= MIN_WIDTH_FULL)
            drawSugarMeter(bx);
    }
}

void EngineHW::drawSugarMeter(const int bx)
{
    constexpr const SDL_Color sugarColors[] = {
        SColor::RED,
        SColor::DEEPPINK,
        SColor::HOTPINK,
        SColor::ORANGE,
        SColor::PINK,
        SColor::BLACK,
        SColor::BLACK,
        SColor::YELLOW,
    };
    const bool updateNow = ((m_ticks >> 1) & 1);
    const int MAX_FX_COLOR = sizeof(sugarColors) / sizeof(sugarColors[0]) - 1;
    CGame &game = *m_game;
    const int sugar = game.sugar();
    if ((sugar > m_visualStates.rSugar || game.hasExtraSpeed()) &&
        !m_visualStates.sugarFx)
    {
        if (sugar > 0)
            m_visualStates.sugarCubes[sugar - 1] = MAX_FX_COLOR;
        m_visualStates.sugarFx = MAX_FX_COLOR;
    }
    else if (m_visualStates.sugarFx && updateNow)
    {
        --m_visualStates.sugarFx;
    }
    m_visualStates.rSugar = sugar;
    for (int i = 0; i < (int)CGame::MAX_SUGAR_RUSH_LEVEL; ++i)
    {
        const SDL_FRect rect{
            .x = SCALEF * bx * (int)FONT_SIZE + SCALEF * i * 5,
            .y = SCALEF * (Y_STATUS + 2),
            .w = SCALEF * 4,
            .h = SCALEF * 4};
        if (static_cast<int>(i) < sugar || game.hasExtraSpeed())
        {
            if (sugar < SUGAR_CUBES && !game.hasExtraSpeed())
            {
                const uint8_t &idx = m_visualStates.sugarCubes[i];
                drawRect(m_renderer, rect, sugarColors[idx], true);
            }
            else
            {
                drawRect(m_renderer, rect, sugarColors[m_visualStates.sugarFx], true);
            }
        }
        else
        {
            drawRect(m_renderer, rect, SColor::WHITE, false);
        }
    }

    // indicate sugar level
    const int x = bx * (int)FONT_SIZE;
    const int y = Y_STATUS + 2 + (int)FONT_SIZE;
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "Lvl %d", m_game->stats().get(S_SUGAR_LEVEL) + 1);
    // drawFont6x6(bitmap, x, y, tmp, WHITE, CLEAR);
    m_font.drawText(
        m_renderer,
        tmp,
        SColor::WHITE,
        nullptr,
        x * SCALE,
        y * SCALE);

    if (updateNow)
    {
        for (int i = 0; i < SUGAR_CUBES; ++i)
        {
            if (m_visualStates.sugarCubes[i])
                --m_visualStates.sugarCubes[i];
        }
    }
}

void EngineHW::drawRect(SDL_Renderer *renderer, const SDL_FRect &rect, const SDL_Color &color, const bool filled)
{
    // Save current draw color
    Uint8 oldR, oldG, oldB, oldA;
    SDL_GetRenderDrawColor(renderer, &oldR, &oldG, &oldB, &oldA);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled)
    {
        // filled  rectangle
        SDL_RenderFillRect(renderer, &rect);
    }
    else
    {
        // outlined rectangle
        SDL_RenderRect(renderer, &rect);
    }

    // Restore old draw color
    SDL_SetRenderDrawColor(renderer, oldR, oldG, oldB, oldA);
}

const Tile *EngineHW::getMainLayerTile(const uint8_t tileID)
{
    ColorMask colorMask = COLOR_NOCHANGE;
    const CGame &game = *m_game;
    const Tile *tile;
    if (tileID == TILES_STOP || tileID == TILES_BLANK || m_animator->isSpecialCase(tileID))
    {
        // skip blank tiles and special cases
        tile = nullptr;
    }
    else if (tileID == TILES_ANNIE2)
    {
        const int hurtStage = game.statsConst().at(S_PLAYER_HURT);
        if (hurtStage == CGame::HurtFlash)
            colorMask = COLOR_ALL_WHITE;
        else if (hurtStage == CGame::HurtInv)
            colorMask = COLOR_INVERTED;
        else if (hurtStage == CGame::HurtFaz)
            colorMask = COLOR_FADE;
        else
            colorMask = COLOR_NOCHANGE;

        if (m_game->isFrozen())
        {
            colorMask = COLOR_GRAYSCALE;
        }
        else if (m_game->hasExtraSpeed())
        {
            // colorMap = &m_colormaps.sugarRush;
            colorMask = COLOR_SUGARRUSH;
        }
        else if (m_game->isGodMode())
        {
            // colorMap = &m_colormaps.godMode;
            colorMask = COLOR_GODMODE;
        }
        else if (m_game->isRageMode())
        {
            // colorMap = &m_colormaps.rage;
            colorMask = COLOR_RAGE;
        }

        const uint8_t userID = game.getUserID();
        const uint32_t userBaseFrame = PLAYER_TOTAL_FRAMES * userID;
        const int aim = game.playerConst().getAim();
        if (!game.health())
        {
            // tile = annie[INDEX_PLAYER_DEAD * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(INDEX_PLAYER_DEAD * PLAYER_FRAMES + m_state.m_playerFrameOffset + userBaseFrame, colorMask);
        }
        else if (!game.goalCount() && game.isClosure())
        {
            // tile = annie[static_cast<uint8_t>(AIM_DOWN) * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(static_cast<uint8_t>(AIM_DOWN) * PLAYER_FRAMES + m_state.m_playerFrameOffset + userBaseFrame, colorMask);
        }
        else if (aim == AIM_DOWN && game.m_gameStats->get(S_IDLE_TIME) > IDLE_ACTIVATION)
        {
            const int idleTime = game.m_gameStats->get(S_IDLE_TIME);
            const int idleFrame = PLAYER_IDLE_BASE + ((idleTime >> 4) & 3);
            const int frame = idleTime & 0x08 ? idleFrame : static_cast<int>(PLAYER_DOWN_INDEX);
            // tile = annie[frame + userBaseFrame];
            tile = m_tileset_users.getTile(frame + userBaseFrame, colorMask);
        }
        else if (game.m_gameStats->get(S_BOAT) != 0 && game.playerConst().getPU() == TILES_SWAMP)
        {
            // tile = annie[PLAYER_BOAT_FRAME + userBaseFrame];
            tile = m_tileset_users.getTile(PLAYER_BOAT_FRAME + userBaseFrame, colorMask);
        }
        else
        {
            // tile = annie[aim * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(aim * PLAYER_FRAMES + m_state.m_playerFrameOffset + userBaseFrame, colorMask);
        }
    }
    else
    {
        const uint16_t j = m_animator->at(tileID);
        if (j == NO_ANIMZ)
        {
            // tile = tiles[tileID];
            tile = m_tileset_tiles.getTile(tileID);
        }
        else
        {
            // tile = animz[j];
            tile = m_tileset_animz.getTile(j);
        }
    }
    return tile;
}

void EngineHW::drawViewPortStatic()
{
    const CMap *map = &m_game->getMap();
    const CGame &game = *m_game;

    const int maxRows = getHeight() / TILE_SIZE;
    const int maxCols = getWidth() / TILE_SIZE;
    const int rows = std::min(maxRows, map->height());
    const int cols = std::min(maxCols, map->width());

    const int lmx = std::max(0, game.playerConst().x() - cols / 2);
    const int lmy = std::max(0, game.playerConst().y() - rows / 2);
    const int mx = std::min(lmx, map->width() > cols ? map->width() - cols : 0);
    const int my = std::min(lmy, map->height() > rows ? map->height() - rows : 0);

    const int layerCount = (int)map->layerCount();
    std::vector<overlay_t> overlays;
    for (int i = layerCount - 1; i >= 0; --i)
    {
        const CLayer *layer = map->getLayer(i);
        if (!layer)
            continue;
        for (int y = 0; y < rows; ++y)
        {
            for (int x = 0; x < cols; ++x)
            {
                const uint8_t tileID = layer->at(x + mx, y + my);
                drawViewPortInner(overlays, layer, tileID, x + mx, y + my);
            }
        }
    }

    std::vector<sprite_t> sprites;
    gatherSprites(sprites, {.mx = mx, .ox = 0, .my = my, .oy = 0});

    // ColorMask colorMask = COLOR_NOCHANGE;
    for (const auto &sprite : sprites)
    {
        // special case animations
        const int x = sprite.x - mx;
        const int y = sprite.y - my;
        const Tile *tile = calcSpecialFrame(sprite);
        drawTile(tile, x * TILE_SIZE, y * TILE_SIZE); //, colorMask, nullptr);
    }

    // draw Bosses
    drawBossses(
        mx * CBoss::BOSS_GRANULAR_FACTOR,
        my * CBoss::BOSS_GRANULAR_FACTOR,
        maxCols * CBoss::BOSS_GRANULAR_FACTOR,
        maxRows * CBoss::BOSS_GRANULAR_FACTOR);
}

const Tile *EngineHW::calcSpecialFrame(const sprite_t &sprite)
{
    if (RANGE(sprite.attr, ATTR_IDLE_MIN, ATTR_IDLE_MAX))
    {
        return m_tileset_tiles.getTile(sprite.tileID);
    }
    int saim = 0;
    if (sprite.tileID < TILES_TOTAL_COUNT)
    {
        // safeguard
        saim = sprite.aim;
        const TileDef &def = getTileDef(sprite.tileID);
        if (def.type == TYPE_DRONE)
        {
            saim &= 1;
        }
    }
    const animzInfo_t &info = m_animator->getSpecialInfo(sprite.tileID);
    return m_tileset_animz.getTile(saim * info.frames + info.base + info.offset);
}

void EngineHW::drawScroll()
{
    constexpr int SCROLL_LEFT = 0;
    constexpr int SCROLL_MID = 1;
    constexpr int SCROLL_RIGHT = 2;
    constexpr int scrollHeight = 48;
    constexpr int partWidth = 16;
    const int y = getHeight() - scrollHeight;

    drawTile(m_tileset_scroll.getTile(SCROLL_LEFT), 0, y);

    for (int x = partWidth; x < getWidth() - partWidth; x += partWidth)
        drawTile(m_tileset_scroll.getTile(SCROLL_MID), x, y);

    drawTile(m_tileset_scroll.getTile(SCROLL_RIGHT), (getWidth() - partWidth), y);
}

void EngineHW::drawEventText()
{
    if (m_state.m_currentEvent != EVENT_NONE)
    {
        const int baseY = getHeight() - 4;
        const message_t message = getEventText(baseY);
        const size_t maxLines = sizeof(message.lines) / sizeof(message.lines[0]);
        for (size_t i = 0; i < maxLines; ++i)
        {
            const auto &line = std::move(message.lines[i]);
            if (line.size() == 0)
                break;
            const int x = (getWidth() - line.size() * FONT_SIZE * message.scaleX) / 2;
            const int y = message.baseY - FONT_SIZE * message.scaleY + i * 10;
            m_font.drawText(m_renderer, line.c_str(), SColor::toSColor(message.color), nullptr, x * SCALE, y * SCALE, SCALE * message.scaleX, SCALE * message.scaleY);
        }
    }
}

void EngineHW::drawTimeout()
{
    const CMap *map = &m_game->getMap();
    const CStates &states = map->statesConst();
    const uint16_t timeout = states.getU(TIMEOUT);
    if (timeout)
    {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%.2d", timeout - 1);
        const bool lowTime = timeout <= 15;
        const int scaleX = !lowTime ? 3 : 5;
        const int scaleY = !lowTime ? 4 : 5;
        const int x = getWidth() - scaleX * FONT_SIZE * strlen(tmp) - FONT_SIZE;
        const int y = 2 * FONT_SIZE;
        const Color color = lowTime && (m_ticks >> 3) & 1 ? ORANGE : YELLOW;
        m_font.drawText(m_renderer, tmp, SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, SCALE * scaleX, SCALE * scaleY);
    }
}

void EngineHW::preloadHearts()
{
    LOGI("preloadHearts");

    CFrame *bitmap = new CFrame((FONT_SIZE + 1) * FONT_SIZE, 3 * FONT_SIZE);

    auto drawHeart = [&bitmap, this](auto bx, auto by, auto health, auto color)
    {
        //      const uint8_t *heart = getCustomChars() + (CHARS_HEART - CHARS_CUSTOM) * FONT_SIZE;
        const uint8_t *heart = m_font.rawData() + (CHARS_HEART - CHARS_CUSTOM + CHARS_CUSTOM_BASE) * FONT_SIZE;
        for (uint32_t y = 0; y < FONT_SIZE; ++y)
        {
            for (uint32_t x = 0; x < FONT_SIZE; ++x)
            {
                const uint8_t bit = heart[y] & (1 << x);
                if (bit)
                    bitmap->at(bx + x, by + y) = x < (uint32_t)health ? color : BLACK;
            }
        }
    };

    for (int i = 0; i <= FONT_SIZE; ++i)
    {
        drawHeart(i * FONT_SIZE, 0, i, RED);
        drawHeart(i * FONT_SIZE, FONT_SIZE, i, PINK);
        drawHeart(i * FONT_SIZE, FONT_SIZE * 2, i, WHITE);
    }

    m_tileset_hearts.load(m_renderer, bitmap, FONT_SIZE, FONT_SIZE);
}

void EngineHW::drawHealthBar(const bool isPlayerHurt)
{
    constexpr const int RED_HEARTS = 0;
    constexpr const int PINK_HEARTS = FONT_SIZE + 1;
    constexpr const int WHITE_HEARTS = PINK_HEARTS * 2;

    uint32_t baseID = RED_HEARTS;
    if (m_game->isGodMode())
    {
        baseID = WHITE_HEARTS;
    }
    else if (isPlayerHurt)
    {
        baseID = PINK_HEARTS;
    }

    // draw health bar
    CGame &game = *m_game;
    int step = FONT_SIZE;
    const int maxHealth = game.maxHealth() / 2 / FONT_SIZE;
    int health = game.health() / 2;
    int bx = 2;
    int by = getHeight() - 12;
    for (int i = 0; i < maxHealth; ++i)
    {
        auto hearts = health > 0 ? std::min(health, static_cast<int>(FONT_SIZE)) : 0;
        const Tile *tile = m_tileset_hearts.getTile(hearts + baseID);
        if (tile)
            drawTile(tile, bx, by);
        bx += FONT_SIZE;
        health -= step;
    }
}

void EngineHW::drawKeys()
{
    CGame &game = *m_game;
    const int y = getHeight() - TILE_SIZE;
    int x = getWidth() - TILE_SIZE;
    const CGame::userKeys_t &keys = game.keys();
    for (size_t i = 0; i < CGame::MAX_KEYS; ++i)
    {
        const uint8_t &k = keys.tiles[i];
        const uint8_t &u = keys.indicators[i];
        if (k)
        {
            // add visual sfx for key pickup
            const Tile *tile = m_tileset_tiles.getTile(k);
            SDL_Texture *texture = tile->texture;
            if (u == CGame::MAX_KEY_STATE)
            {
                // all white
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD); // multiply blend
                SDL_SetTextureColorMod(texture, 255, 255, 255);      // make RGB = 1.0
                SDL_SetTextureAlphaMod(texture, 255);                // keep alpha as is
                drawTile(tile, x, y);
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            }
            else if (u)
            {
                // fade in
                uint8_t mod = 2 << u;
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(texture, mod, mod, mod);
                // SDL_SetTextureAlphaMod(texture, mod);
                drawTile(tile, x, y);
                SDL_SetTextureColorMod(texture, 255, 255, 255);
                //   SDL_SetTextureAlphaMod(texture, 255);
            }
            else
            {
                // normal
                drawTile(tile, x, y);
            }
            x -= TILE_SIZE;
        }
    }
    if ((m_ticks >> 1) & 1)
        game.decKeyIndicators();
}

void EngineHW::drawViewPortDynamic()
{
    const CMap *map = &m_game->getMap();
    const int maxRows = getHeight() / TILE_SIZE;
    const int maxCols = getWidth() / TILE_SIZE;
    const int rows = std::min(maxRows, map->height());
    const int cols = std::min(maxCols, map->width());
    const int mx = m_state.m_cx / 2;
    const int ox = m_state.m_cx & 1;
    const int my = m_state.m_cy / 2;
    const int oy = m_state.m_cy & 1;
    constexpr const int halfOffset = TILE_SIZE / 2;

    const int layerCount = (int)map->layerCount();
    std::vector<overlay_t> overlays;
    for (int i = layerCount - 1; i >= 0; --i)
    {
        const CLayer *layer = map->getLayer(i);
        if (!layer)
            continue;
        int py = oy ? -halfOffset : 0;
        for (int y = 0; y < rows + oy; ++y)
        {
            int px = ox ? -halfOffset : 0;
            for (int x = 0; x < cols + ox; ++x)
            {
                const uint8_t tileID = layer->at(x + mx, y + my);
                drawViewPortInner(overlays, layer, tileID, px, py);
                px += TILE_SIZE;
            }
            py += TILE_SIZE;
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    // overlay special case monsters and sfx

    std::vector<sprite_t> sprites;
    gatherSprites(sprites, {.mx = mx, .ox = ox, .my = my, .oy = oy});
    for (const auto &sprite : sprites)
    {
        // special case animations
        const int x = sprite.x - mx;
        const int y = sprite.y - my;
        const Tile *tile = calcSpecialFrame(sprite);
        int px = x * TILE_SIZE;
        int py = y * TILE_SIZE;
        if (x && ox)
            px -= halfOffset;
        if (y && oy)
            py -= halfOffset;
        drawTile(tile, px, py);
    }

    for (const auto &overlay : overlays)
        drawTile(overlay.tile, overlay.x, overlay.y);

    // draw Bosses
    drawBossses(m_state.m_cx, m_state.m_cy, maxCols * CBoss::BOSS_GRANULAR_FACTOR, maxRows * CBoss::BOSS_GRANULAR_FACTOR);
}

void EngineHW::drawBossses(const int mx, const int my, const int sx, const int sy)
{
    auto between = [](int a1, int a2, int b1, int b2)
    {
        return a1 < b2 && a2 > b1;
    };

    auto betweenRect = [&between](const rect_t &bRect, const rect_t &sRect)
    {
        return between(bRect.x, bRect.x + bRect.width, sRect.x, sRect.x + sRect.width) &&
               between(bRect.y, bRect.y + bRect.height, sRect.y, sRect.y + sRect.height);
    };

    auto printRect = [](const rect_t &rect, const std::string_view &name)
    {
        LOGI("%s (%d, %d) w:%d h: %d", name.data(), rect.x, rect.y, rect.width, rect.height);
    };

    (void)printRect;

    constexpr int MAX_HP_GAUGE = 64;
    constexpr int GRID_SIZE = 8;
    constexpr int HP_BAR_HEIGHT = 8;
    constexpr int HP_BAR_SPACING = 2;

    // bosses are drawn on a 8x8 grid overlayed on top of the regular 16x16 grid
    for (const auto &boss : m_game->bosses())
    {
        // don't draw completed bosses
        if (boss.isDone())
            continue;

        TileSet *tileset = nullptr;
        if (boss.data()->sheet == 0)
        {
            tileset = &m_tileset_sheet0;
        }
        else if (boss.data()->sheet == 1)
        {
            tileset = &m_tileset_sheet1;
        }
        else
        {
            LOGE("invalid sheet: %d", boss.data()->sheet);
            continue;
        }

        const int num = boss.currentFrame();
        const Tile *tile = tileset->getTile(num);
        if (!tile)
            continue;
        const hitbox_t &hitbox = boss.hitbox();

        // Logical coordonates comverted to screen positions
        // (using GRID_SIZE)

        // Boss Rect
        const rect_t bRect{
            GRID_SIZE * (boss.x() - hitbox.x),
            GRID_SIZE * (boss.y() - hitbox.y),
            (int)tile->rect.w,
            (int)tile->rect.h};

        // Screen Rect
        const rect_t sRect{
            .x = GRID_SIZE * mx,
            .y = GRID_SIZE * my,
            .width = GRID_SIZE * sx,
            .height = GRID_SIZE * sy,
        };

        if (betweenRect(bRect, sRect))
        {
            const int x = bRect.x - sRect.x;
            const int y = bRect.y - sRect.y;
            // draw boss
            drawTile(tile, x, y);
        }

        // skip drawing the healthbar and name
        if (!boss.data()->show_details)
            continue;

        // Hp Rect
        const float hpRatio = (float)boss.maxHp() / MAX_HP_GAUGE;
        const rect_t hRect{
            .x = bRect.x,
            .y = bRect.y - HP_BAR_HEIGHT - HP_BAR_SPACING,
            .width = MAX_HP_GAUGE, // boss.maxHp(),
            .height = HP_BAR_HEIGHT,
        };
        if (betweenRect(hRect, sRect))
        {
            const float x = hRect.x - sRect.x;
            const float y = hRect.y - sRect.y;
            SDL_FRect rect{x * SCALEF, y * SCALEF, hRect.width * SCALEF, hRect.height * SCALEF};
            SDL_FRect rectH{x * SCALEF, y * SCALEF, static_cast<int>(boss.hp() / hpRatio) * SCALEF, hRect.height * SCALEF};
            drawRect(m_renderer, rect, SColor::BLACK, true);
            drawRect(m_renderer, rectH, SColor::toSColor(boss.data()->color_hp), true);
            drawRect(m_renderer, rect, SColor::WHITE, false);
        }

        // draw Boss Name
        const int x = hRect.x - sRect.x;
        const int y = hRect.y - sRect.y - FONT_SIZE;
        m_font.drawText(m_renderer, boss.name(), SColor::toSColor(boss.data()->color_name), nullptr, x * SCALE, y * SCALE, 1, 1);
    }
}

void EngineHW::fazeScreen()
{
    // save default
    SDL_BlendMode oldMode = 0;
    SDL_GetRenderDrawBlendMode(m_renderer, &oldMode);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_Color color{0, 0, 0, 0xe0};
    SDL_FRect rect{0, 0, getWidth() * SCALEF, getHeight() * SCALEF};
    drawRect(m_renderer, rect, color, true);

    // restore
    SDL_SetRenderDrawBlendMode(m_renderer, oldMode);
}

void EngineHW::flashScreen()
{
    // save default
    SDL_BlendMode oldMode;
    SDL_GetRenderDrawBlendMode(m_renderer, &oldMode);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);

    SDL_FRect rect{0, 0, getWidth() * SCALEF, getHeight() * SCALEF};
    SDL_RenderFillRect(m_renderer, &rect);

    // restore
    SDL_SetRenderDrawBlendMode(m_renderer, oldMode);
}

void EngineHW::updateColorMaps(const ColorMaps &colormaps)
{
    // update colodMasks
    m_tileset_users.cacheMask(m_renderer, COLOR_SUGARRUSH, [&colormaps](uint32_t &color)
                              {
                                  const auto &colorMap = colormaps.sugarRush;
                                  if (auto it = colorMap.find(color); it != colorMap.end())
                                      color = it->second; //
                              });

    m_tileset_users.cacheMask(m_renderer, COLOR_GODMODE, [&colormaps](uint32_t &color)
                              {
                                  const auto &colorMap = colormaps.godMode;
                                  if (auto it = colorMap.find(color); it != colorMap.end())
                                      color = it->second; //
                              });

    m_tileset_users.cacheMask(m_renderer, COLOR_RAGE, [&colormaps](uint32_t &color)
                              {
                                  const auto &colorMap = colormaps.rage;
                                  if (auto it = colorMap.find(color); it != colorMap.end())
                                      color = it->second; //
                              });
}

void EngineHW::drawRect(const rect_t &rect, const Color &color, const bool fill)
{
    const SDL_FRect drect{SCALEF * rect.x, SCALEF * rect.y, SCALEF * rect.width, SCALEF * rect.height};
    drawRect(m_renderer, drect, SColor::toSColor(color), fill);
}

SDL_Texture *EngineHW::createTexture(CFrame *frame)
{
    SDL_PropertiesID texProps = SDL_CreateProperties();
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame->width());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame->height());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
    SDL_Texture *texture = SDL_CreateTextureWithProperties(m_renderer, texProps);
    SDL_DestroyProperties(texProps);

    if (!texture)
        return nullptr;

    int pitch = frame->width() * sizeof(uint32_t); // 4 bytes per pixel
    if (!SDL_UpdateTexture(texture, nullptr, frame->getRGB().data(), pitch))
    {
        std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << "\n";
    }
    return texture;
}

CFrame *EngineHW::getFrame(const std::string &filepath)
{
    CFrame *frame = nullptr;
    auto set = std::make_unique<CFrameSet>();
    data_t data = AssetMan::read(filepath);
    if (!data.empty())
    {
        LOGI("reading %s", filepath.c_str());
        CFileMem mem;
        mem.replace(data.data(), data.size());
        if (set->extract(mem))
        {
            LOGI("extracted: %ld", set->getSize());
        }
        if (set->size())
            frame = set->removeAt(0);
    }
    else
    {
        LOGE("can't read: %s", filepath.c_str());
    }
    return frame;
}

int EngineHW::drawTitlePix(int offsetY)
{
    const int x = 0;
    const int y = offsetY;
    SDL_FRect dst = {
        (float)x * SCALEF,
        (float)y * SCALEF,
        m_textureTitlePix->w * SCALEF,
        m_textureTitlePix->h * SCALEF,
    };
    SDL_RenderTexture(m_renderer, m_textureTitlePix, nullptr, &dst);
    return (int)m_textureTitlePix->h;
}