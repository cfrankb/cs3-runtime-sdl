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
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include "SDL3/SDL_gamepad.h"
#include <vector>
#include <unordered_map>
#include "shared/interfaces/ISound.h"
#include "tileset_tex.h"

class ISound;
class CFrameSet;
class CMenu;
class CGameUI;
typedef std::vector<std::string> StringVector;

struct Rez
{
    int w;
    int h;
};

class CRuntime : public CGameMixin
{
public:
    CRuntime();
    ~CRuntime() override;

    void paint();
    void run();
    bool initSDL();
    void doInput();
    void preRun();
    void enableMusic(bool state);
    void stopMusic() override;
    void startMusic() override;
    void save() override;
    void load() override;
    bool parseConfig(uint8_t *buf);
    void setConfig(const char *key, const char *val);
    void setWorkspace(const char *workspace);
    void initOptions();
    void setStartLevel(int level);
    bool isRunning() const;
    void init(CMapArch *maparch, int index) override;
    void setVerbose(bool enable);
    void notifyExitFullScreen();
    bool checkMusicFiles();
    void loadColorMaps(const int userID);
    bool createSDLWindow();
    Rez getScreenSize();
    Rez getWindowSize();
    rect_t getSafeAreaWindow();
    rect_t windowRect2textureRect(const rect_t &wRect);
    void debugSDL();
    bool saveToFile(const std::string filepath, const std::string name);
    bool loadFromFile(const std::string filepath, std::string &name);
    bool isValidSavegame(const std::string &filepath);

private:
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

    struct Summary
    {
        int ppFruits;
        int ppBonuses;
        int ppSecrets;
        int timeTaken;
    };

    struct posF_t
    {
        float x;
        float y;
    };

    struct pos_t
    {
        int x;
        int y;
    };

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
    void sanityTest() override {};
    void toggleFullscreen() override;
    void manageTitleScreen() override;
    void toggleGameMenu() override;
    void manageGameMenu() override;
    void manageUserMenu() override;
    void manageSkillMenu() override;
    void manageLevelSummary() override;
    void initLevelSummary() override;
    void changeMoodMusic(CGame::GameMode mode) override;

    std::unique_ptr<IMusic> m_music;
    std::shared_ptr<ISound> m_sound;
    std::unique_ptr<CMenu> m_mainMenu;
    std::unique_ptr<CMenu> m_gameMenu;
    std::unique_ptr<CMenu> m_optionMenu;
    std::unique_ptr<CMenu> m_userMenu;
    std::unique_ptr<CMenu> m_skillMenu;
    std::unique_ptr<CFrameSet> m_titlePix;
    bool m_musicEnabled = false;
    App m_app;
    std::unordered_map<std::string, std::string> m_config;
    std::vector<std::string> m_musicFiles;
    std::vector<std::string> m_soundFiles;
    std::vector<std::string> m_assetFiles;
    std::vector<std::string> m_userNames;
    std::string m_workspace = "";
    std::string m_credits;
    char *m_scroll = nullptr;
    uint32_t m_scrollPtr = 0;
    int m_startLevel;
    int m_skill;
    int m_resolution = 0;
    int m_fullscreen = 0;
    int m_musicVolume = 10;
    int m_sndVolume = 10;
    int m_xAxisSensitivity = 10;
    int m_yAxisSensitivity = 10;
    bool m_verbose = false;
    bool m_trace = false;
    bool m_isRunning = true;
    CFrame *m_bitmap = nullptr;
    Summary m_summary;
    int m_lastMenuBaseY = 0;
    int m_lastMenuBaseX = 0;
    CMenu *m_lastMenu = nullptr;
    uint8_t m_mouseButtons[3];
    CGameUI *m_virtualKeyboard;
    std::string m_input;
    bool m_hasFocus;

    // Vector to hold pointers to opened game controllers
    std::vector<SDL_Gamepad *> m_gameControllers;

    std::vector<Rez> m_resolutions = {
        {640, 480},
        {800, 600},
        {1024, 768},
        {1280, 720}};

    enum
    {
        FONT_SIZE = 8,
        MENUID_MAINMENU = 0x10,
        MENUID_GAMEMENU = 0x11,
        MENUID_OPTIONMENU,
        MENUID_USERS,
        MENUID_SKILLS,
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

    void drawTitleScreen(CFrame &bitmap);
    bool isTrue(const std::string &value) const;
    void resizeScroller();
    void drawScroller(CFrame &bitmap);
    void drawTitlePix(CFrame &bitmap, const int offsetY);
    void drawOptions(CFrame &bitmap);
    size_t scrollerBufSize();
    bool fileExists(const std::string &filename) const;
    const std::string getSavePath() const;
    void drawMenu(CFrame &bitmap, CMenu &menu, const int baseX, const int baseY);
    void manageMenu(CMenu &menu);
    bool fetchFile(const std::string &path, char **dest, const bool terminator);
    void parseHelp(char *text);
    void manageOptionScreen() override;
    CMenu &initOptionMenu();
    CMenu &initUserMenu();
    int findResolutionIndex();
    void resize(int w, int h);
    void listResolutions(int displayIndex = 0);
    void createResolutionList();
    void resizeGameMenu();
    void openMusic(const std::string &filename);
    void drawUserMenu(CFrame &bitmap);
    void drawSkillMenu(CFrame &bitmap);
    std::string getMusicPath(const std::string &filename);
    void enterGame();
    void drawLevelSummary(CFrame &bitmap);
    int menuItemAt(int x, int y);
    void followPointer(int x, int y);
    void initSkillMenu();
    bool isMenuActive();
    void clearVJoyStates();
    void clearMouseButtons();
    void handleMouse(int x, int y);
    void handleFingerDown(float x, float y);
    void onOrientationChange();
    pos_t windowPos2texturePos(posF_t sf);
    void onGamePadEvent(const SDL_Event &event);
    void onMouseEvent(const SDL_Event &event);
    void onSDLQuit();
    void initVirtualKeyboard();
    void drawVirtualKeyboard(CFrame &bitmap, const std::string &title, std::string &buffer);
    void handleVKEY(int x, int y);
    bool loadAppIcon();
    void addGamePadOptions(CMenu &menu);
    void drawTest(CFrame &bitmap);

    void drawScreen();
    void drawGameStatus(const visualCues_t &visualcues);
    void drawSugarMeter(const int bx);
    void drawRect(SDL_Renderer *renderer, const SDL_FRect &rect, const SDL_Color &color, const bool filled);

    void drawPreScreen();
    void drawLevelIntro();
    SDL_Texture *createTexture(SDL_Renderer *renderer, CFrame *frame);
    void drawTile(const Tile *tile, const int x, const int y, const ColorMask &colorMask, const std::unordered_map<uint32_t, uint32_t> *colorMap);
    void drawViewPortStatic();
    Tile *tile2Frame(const uint8_t tileID, ColorMask &colorMask, std::unordered_map<uint32_t, uint32_t> *&colorMap);
    Tile *calcSpecialFrame(const sprite_t &sprite);

    SDL_Texture *m_textureTitlePix;
    TileSet m_tileset_tiles;
    TileSet m_tileset_animz;
    TileSet m_tileset_users;

#ifdef __EMSCRIPTEN__
    void mountFS();
    void readGamePadJs();
#endif
};