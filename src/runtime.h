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
#include <vector>
#include <unordered_map>

class ISound;
class CFrameSet;
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
    void setPrefix(const char *prefix);
    void setWorkspace(const char *workspace);
    void initOptions();

protected:
    static void cleanup();
    void preloadAssets() override;
    void initMusic();
    void initSounds();
    void keyReflector(SDL_Keycode key, uint8_t keyState);
    bool loadScores() override;
    bool saveScores() override;
    void openMusicForLevel(int i) override;
    void setupTitleScreen() override;
    void takeScreenshot() override;
    void toggleFullscreen() override;
    void sanityTest() override;

    typedef struct
    {
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_Texture *texture;
        bool isFullscreen = false;
        int windowedX;
        int windowedY;
        int windowedWidth = WIDTH * 2;
        int windowedHeigth = HEIGHT * 2;
    } App;

    enum
    {
        FONT_SIZE = 8,
        SCROLLER_BUF_SIZE = WIDTH / FONT_SIZE,
    };

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
    char m_scroll[SCROLLER_BUF_SIZE + 1];
    int m_scrollPtr;

private:
    void drawTitleScreen(CFrame &bitmap);
    void splitString2(const std::string &str, StringVector &list);
    void addTrailSlash(std::string &path);
    bool isTrue(std::string value);
};