/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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

#ifdef USE_QFILE
#include <QTimer>
#endif
#include <cstdint>
#include <cstdio>
#include <string>

#define WIDTH getWidth()
#define HEIGHT getHeight()

class CFrameSet;
class CGame;
class CFrame;
class CMapArch;
class CAnimator;
class IMusic;
class CRecorder;

class CGameMixin
{
public:
    explicit CGameMixin();
    virtual ~CGameMixin();
    void init(CMapArch *maparch, int index);
    inline bool within(int val, int min, int max);
    void enableHiScore();
    void setSkill(uint8_t skill);

#ifdef USE_QFILE
protected slots:
#endif
    virtual void mainLoop();
    void changeZoom();
    virtual void save() = 0;
    virtual void load() = 0;

protected:
    enum : uint32_t
    {
        TICK_RATE = 24,
        NO_ANIMZ = 255,
        KEY_PRESSED = 1,
        KEY_RELEASED = 0,
        BUTTON_PRESSED = 1,
        BUTTON_RELEASED = 0,
        INTRO_DELAY = TICK_RATE * 3,
        HISCORE_DELAY = 5 * TICK_RATE,
        CLEAR = 0,
        ALPHA = 0xff000000,
        WHITE = 0x00ffffff | ALPHA,
        YELLOW = 0x0000ffff | ALPHA,
        PURPLE = 0x00ff00ff | ALPHA,
        BLACK = 0x00000000 | ALPHA,
        GREEN = 0x0000ff00 | ALPHA,
        DARKGREEN = 0x00008000 | ALPHA,
        LIME = 0x0000ffbf | ALPHA,
        BLUE = 0x00ff0000 | ALPHA,
        CYAN = 0x00ffff00 | ALPHA,
        RED = 0x000000ff | ALPHA,
        DARKRED = 0x00000080 | ALPHA,
        DARKBLUE = 0x00440000 | ALPHA,
        DARKGRAY = 0x00444444 | ALPHA,
        LIGHTGRAY = 0x00A9A9A9 | ALPHA,
        _WIDTH = 320,
        _HEIGHT = 240,
        TILE_SIZE = 16,
        COUNTDOWN_INTRO = 1,
        COUNTDOWN_RESTART = 2,
        GAME_MENU_COOLDOWN = 10,
        FONT_SIZE = 8,
        MAX_SCORES = 18,
        KEY_REPETE_DELAY = 5,
        KEY_NO_REPETE = 1,
        MAX_NAME_LENGTH = 16,
        SAVENAME_PTR_OFFSET = 8,
        CARET_SPEED = 8,
        INTERLINES = 2,
        Y_STATUS = 2,
    };

    enum KeyCode : uint8_t
    {
        Key_A,
        Key_N = Key_A + 13,
        Key_Y = Key_A + 24,
        Key_Z = Key_A + 25,
        Key_Space,
        Key_0,
        Key_9 = Key_0 + 9,
        Key_Period,
        Key_BackSpace,
        Key_Enter,
        Key_Escape,
        Key_F1,
        Key_F2,
        Key_F3,
        Key_F4,
        Key_F5,
        Key_F6,
        Key_F7,
        Key_F8,
        Key_F9,
        Key_F10,
        Key_F11,
        Key_F12,
        Key_Count
    };

    enum
    {
        PROMPT_NONE,
        PROMPT_ERASE_SCORES,
        PROMPT_RESTART_GAME,
        PROMPT_LOAD,
        PROMPT_SAVE,
        PROMPT_RESTART_LEVEL,
        PROMPT_HARDCORE,
        PROMPT_TOGGLE_MUSIC,
        INVALID = -1,
        BUTTON_A = 0,
        BUTTON_B,
        BUTTON_X,
        BUTTON_Y,
        BUTTON_START,
        BUTTON_BACK,
        Button_Count,
        JOY_AIMS = 4,
    };
    uint8_t m_keyStates[Key_Count];
    uint8_t m_keyRepeters[Key_Count];

    using Rect = struct
    {
        int x;
        int y;
        int width;
        int height;
    };

    using hiscore_t = struct
    {
        int score;
        int level;
        char name[MAX_NAME_LENGTH];
    };

    hiscore_t m_hiscores[MAX_SCORES];
    uint8_t m_joyState[JOY_AIMS];
    uint8_t m_buttonState[Button_Count];
    uint32_t m_ticks = 0;
    CAnimator *m_animator;
    CFrameSet *m_tiles = nullptr;
    CFrameSet *m_animz = nullptr;
    CFrameSet *m_annie = nullptr;
    uint8_t *m_fontData = nullptr;
    CGame *m_game = nullptr;
    CMapArch *m_maparch = nullptr;
    CRecorder *m_recorder = nullptr;
    int m_playerFrameOffset = 0;
    int m_healthRef = 0;
    int m_countdown = 0;
    int m_scoreRank = INVALID;
    bool m_recordScore = false;
    bool m_zoom = false;
    bool m_assetPreloaded = false;
    bool m_scoresLoaded = false;
    bool m_hiscoreEnabled = false;
    bool m_paused = false;
    bool m_musicMuted = false;
    int m_prompt = PROMPT_NONE;
    int m_optionCooldown = 0;
    bool m_gameMenuActive = false;
    int m_gameMenuCooldown = 0;

    void drawPreScreen(CFrame &bitmap);
    void drawScreen(CFrame &bitmap);
    void fazeScreen(CFrame &bitmap, const int shift);
    void drawViewPort(CFrame &bitmap, const int mx, const int my, const int cols, const int rows);
    void drawLevelIntro(CFrame &bitmap);
    void drawPreview(CFrame &bitmap);
    void drawFont(CFrame &frame, int x, int y, const char *text, const uint32_t color = WHITE, const uint32_t bgcolor = BLACK, const int scaleX = 1, const int scaleY = 1);
    void drawRect(CFrame &frame, const Rect &rect, const uint32_t color = GREEN, bool fill = true);
    inline void drawKeys(CFrame &bitmap);
    inline void drawTile(CFrame &frame, const int x, const int y, CFrame &tile, bool alpha);
    void nextLevel();
    void restartLevel();
    void restartGame();
    void startCountdown(int f = 1);
    int rankScore();
    void drawScores(CFrame &bitmap);
    bool inputPlayerName();
    void clearScores();
    void clearKeyStates();
    void clearJoyStates();
    void clearButtonStates();
    void manageGamePlay();
    void handleFunctionKeys();
    bool handlePrompts();
    inline int getWidth()
    {
        return _WIDTH;
    }
    inline int getHeight()
    {
        return _HEIGHT;
    }
    virtual void preloadAssets() = 0;
    virtual void sanityTest() = 0;
    virtual void drawHelpScreen(CFrame &bitmap);
    virtual bool loadScores() = 0;
    virtual bool saveScores() = 0;
    virtual bool read(FILE *sfile, std::string &name);
    virtual bool write(FILE *tfile, const std::string &name);
    virtual void stopMusic() = 0;
    virtual void startMusic() = 0;
    virtual void setZoom(bool zoom);
    virtual void openMusicForLevel(int i) = 0;
    virtual void setupTitleScreen() = 0;
    virtual void takeScreenshot() = 0;
    virtual void toggleFullscreen() = 0;
    virtual void manageTitleScreen() = 0;
    virtual void toggleGameMenu() = 0;
    virtual void manageGameMenu() = 0;

private:
    void stopRecorder();
    void recordGame();
    void playbackGame();
};
