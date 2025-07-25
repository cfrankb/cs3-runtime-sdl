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
#include "gamemixin.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <unordered_map>

class ISound;
class CFrameSet;
class CMenu;
typedef std::vector<std::string> StringVector;

class CRuntime : public CGameMixin
{
public:
    CRuntime();
    ~CRuntime() override;

    void paint();
    void run();
    bool SDLInit();
    void doInput();
    void preRun();
    void enableMusic(bool state);
    void stopMusic() override;
    void startMusic() override;
    void save() override;
    void load() override;
    bool parseConfig(const char *filename);
    void setConfig(const char *key, const char *val);
    void setPrefix(const char *prefix);
    void setWorkspace(const char *workspace);
    void initOptions();
    void setStartLevel(int level);

    typedef struct
    {
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_Texture *texture;
        bool isFullscreen;
        int windowedX;
        int windowedY;
        int windowedWidth;
        int windowedHeigth;
    } App;

protected:
    static void cleanup();
    void preloadAssets() override;
    bool initControllers();
    void initMusic();
    void initSounds();
    void keyReflector(SDL_Keycode key, uint8_t keyState);
    bool loadScores() override;
    bool saveScores() override;
    void openMusicForLevel(int i) override;
    void setupTitleScreen() override;
    void takeScreenshot() override;
    void sanityTest() override;
    void toggleFullscreen() override;
    void manageTitleScreen() override;
    void toggleGameMenu() override;
    void manageGameMenu() override;

    IMusic *m_music = nullptr;
    ISound *m_sound = nullptr;
    bool m_musicEnabled = false;
    App m_app;
    std::unordered_map<std::string, std::string> m_config;
    std::vector<std::string> m_musicFiles;
    std::vector<std::string> m_soundFiles;
    std::vector<std::string> m_assetFiles;
    std::string m_prefix = "data/";
    std::string m_workspace = "";
    CFrameSet *m_title;
    char *m_credits = nullptr;
    char *m_scroll = nullptr;
    uint32_t m_scrollPtr;
    int m_startLevel;
    int m_skill;
    int m_volume = 10;
    CMenu *m_mainMenu = nullptr;
    CMenu *m_gameMenu = nullptr;
    enum
    {
        FONT_SIZE = 8,
        MENUID_MAINMENU = 0x10,
        MENUID_GAMEMENU = 0x11,
        MENU_ITEM_NEW_GAME = 0x100,
        MENU_ITEM_LOAD_GAME,
        MENU_ITEM_SAVE_GAME,
        MENU_ITEM_SKILL,
        MENU_ITEM_LEVEL,
        MENU_ITEM_HISCORES,
        MENU_ITEM_MUSIC,
        MENU_ITEM_MUSIC_VOLUME,
        MENU_ITEM_X_AXIS_SENTIVITY,
        MENU_ITEM_Y_AXIS_SENTIVITY,
        DEFAULT_OPTION_COOLDOWN = 3,
        MAX_OPTION_COOLDOWN = 6,
        MUSIC_VOLUME_STEPS = 1 + (MIX_MAX_VOLUME / 10),
        MUSIC_VOLUME_MAX = MIX_MAX_VOLUME,
        SCALE2X = 2,
    };

private:
    void drawTitleScreen(CFrame &bitmap);
    void splitString2(const std::string &str, StringVector &list);
    void addTrailSlash(std::string &path);
    bool isTrue(const std::string &value) const;
    void resizeScroller();
    void cleanUpCredits();
    void drawScroller(CFrame &bitmap);
    void drawTitlePix(CFrame &bitmap, const int offsetY);
    size_t scrollerBufSize() { return WIDTH / FONT_SIZE; };
    bool fileExists(const std::string &filename) const;
    const std::string getSavePath() const;
    void drawMenu(CFrame &bitmap, CMenu &menu, const int baseY);
    void manageMenu(CMenu &menu);
    bool fetchFile(const std::string &path, char **dest, const bool terminator);
};