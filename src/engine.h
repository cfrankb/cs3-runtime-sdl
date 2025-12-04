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
#include "shared/interfaces/ISound.h"
#include "color.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include "animator.h"
#include "colormap.h"
#include "summary.h"

class CMenu;
class CGame;
class CAnimator;
class MenuManager;
class CGameMixin;

struct EngineState
{
    bool m_paused;
    int m_prompt;
    int m_musicMuted;
    int m_currentEvent;
    int m_playerFrameOffset;
    int m_cameraMode;
    int m_cx;
    int m_cy;
    int m_countdown;
};

class Engine
{
public:
    Engine() {};
    ~Engine() {};

    void resize(int width, int height)
    {
        m_width = width;
        m_height = height;
    }

    void updateEngineState(const EngineState &state)
    {
        m_state = state;
    }

    // virtual
    virtual void drawScreen() = 0;
    virtual void updateColorMaps(const ColorMaps &colormaps) = 0;
    virtual void drawMenu(CMenu &menu, const int baseX, const int baseY) = 0;

    // dispatch
    void drawPreScreen();
    void drawLevelIntro();
    void drawTest();
    void drawSkillMenu();
    void drawLevelSummary();
    void drawUserMenu();
    void drawScores();
    void drawHelpScreen();
    void drawOptions();

    // helper
    void initLevelSummary();

    // overloaded
    virtual void drawFont(const int x, const int y, const char *text, const Color color = WHITE, const int scaleX = 1, const int scaleY = 1) = 0;

    void setTicks(const uint64_t ticks)
    {
        m_ticks = ticks;
    }

protected:
    enum
    {
        FONT_SIZE = 8,

        MENU_ITEM_NEW_GAME = 0x100,
        MENU_ITEM_LOAD_GAME,
        MENU_ITEM_SAVE_GAME,
        MENU_ITEM_SKILL,
        MENU_ITEM_LEVEL,
        MENU_ITEM_HISCORES,
        MENU_ITEM_MUSIC,
        MENU_ITEM_MUSIC_VOLUME,
        MENU_ITEM_SND_VOLUME,
        MENU_ITEM_OPTIONS,
        MENU_ITEM_X_AXIS_SENTIVITY,
        MENU_ITEM_Y_AXIS_SENTIVITY,
        MENU_ITEM_RESOLUTION,
        MENU_ITEM_FULLSCREEN,
        MENU_ITEM_RETURN_MAIN,
        MENU_ITEM_CAMERA,
        MENU_ITEM_RETURN_TO_GAME,
        MENU_ITEM_SELECT_USER,
        MENU_ITEM_MAINMENU_BAR,
        MENU_ITEM_SKILLGROUP,
        MENU_ITEM_QUIT,
        MENUBAR_OPTIONS = 0,
        MENUBAR_CREDITS,
        MENUBAR_HISCORES,
        DEFAULT_OPTION_COOLDOWN = 3,
        MAX_OPTION_COOLDOWN = 6,
        MUSIC_VOLUME_STEPS = 1 + (ISound::MAX_VOLUME / 10),
        MUSIC_VOLUME_MAX = ISound::MAX_VOLUME,
        SCALE2X = 2,
        PIXEL_SCALE = 2,
        VK_SPACE = ' ',
        VK_ENTER = '\n',
        VK_BACKSPACE = 8,
    };

    enum : uint32_t
    {
        TICK_RATE = 24, // 1s
        NO_ANIMZ = 255,
        KEY_PRESSED = 1,
        KEY_RELEASED = 0,
        BUTTON_PRESSED = 1,
        BUTTON_RELEASED = 0,
        INTRO_DELAY = 3 * TICK_RATE,
        HISCORE_DELAY = 5 * TICK_RATE,
        EVENT_COUNTDOWN_DELAY = TICK_RATE,
        MSG_COUNTDOWN_DELAY = 6 * TICK_RATE,
        TRAP_MSG_COUNTDOWN_DELAY = 2 * TICK_RATE,
        TILE_SIZE = 16,
        COUNTDOWN_INTRO = 1,
        COUNTDOWN_RESTART = 2,
        GAME_MENU_COOLDOWN = 10,
        MAX_SCORES = 18,
        KEY_REPETE_DELAY = 5,
        KEY_NO_REPETE = 1,
        MAX_NAME_LENGTH = 16,
        SAVENAME_PTR_OFFSET = 8,
        CARET_SPEED = 8,
        INTERLINES = 2,
        Y_STATUS = 2,
        PLAYER_FRAMES = 8,
        PLAYER_STD_FRAMES = 7,
        PLAYER_DOWN_INDEX = 8,
        PLAYER_TOTAL_FRAMES = 45,
        PLAYER_BOAT_FRAME = 44,
        PLAYER_IDLE_BASE = 0x28,
        ANIMZ_INSECT1_FRAMES = 8,
        INSECT1_MAX_OFFSET = 7,
        CAMERA_MODE_STATIC = 0,
        CAMERA_MODE_DYNAMIC = 1,
        FAZ_INV_BITSHIFT = 1,
        INDEX_PLAYER_DEAD = 4,
        HEALTHBAR_CLASSIC = 0,
        HEALTHBAR_HEARTHS = 1,
    };

    enum : int32_t
    {
        MAX_IDLE_CYCLES = 0x100,
        IDLE_ACTIVATION = 0x40,
        MIN_WIDTH_FULL = 320,
        SUGAR_CUBES = 5,
        SCREEN_SHAKES = 4,
    };

    enum Prompt
    {
        PROMPT_NONE,
        PROMPT_ERASE_SCORES,
        PROMPT_RESTART_GAME,
        PROMPT_LOAD,
        PROMPT_SAVE,
        PROMPT_RESTART_LEVEL,
        PROMPT_HARDCORE,
        PROMPT_TOGGLE_MUSIC,
    };

    enum
    {
        INVALID = -1,
    };

    struct visualStates_t
    {
        int rGoalCount = 0;
        int rLives = 0;
        int rSugar = 0;
        int sugarFx = 0;
        uint8_t sugarCubes[SUGAR_CUBES];
    };

    struct cameraContext_t
    {
        int mx;
        int ox;
        int my;
        int oy;
    };

    struct sprite_t
    {
        int16_t x;
        int16_t y;
        uint16_t tileID;
        uint8_t aim;
        uint8_t attr;
    };

    struct visualCues_t
    {
        bool diamondShimmer;
        bool livesShimmer;
    };

    struct message_t
    {
        int scaleX;
        int scaleY;
        int baseY;
        Color color;
        std::string lines[3];
    };

    struct hiscore_t
    {
        int32_t score;
        int32_t level;
        char name[MAX_NAME_LENGTH];
    };

    uint64_t m_ticks;
    CGame *m_game;
    visualStates_t m_visualStates;
    EngineState m_state;

    int m_width;
    int m_height;
    MenuManager *m_menus = nullptr;
    CAnimator *m_animator = nullptr;
    CGameMixin *m_gameMixin;
    std::vector<std::string> m_assetFiles;
    Summary m_summary;

    inline int getWidth()
    {
        return m_width;
    }

    inline int getHeight()
    {
        return m_height;
    }

    void setAssetFiles(const std::vector<std::string> &assetFiles)
    {
        m_assetFiles = assetFiles;
    }

    // shared
    void gatherSprites(std::vector<sprite_t> &sprites, const cameraContext_t &context);
    void resizeGameMenu();
    message_t getEventText(const int baseY);
};