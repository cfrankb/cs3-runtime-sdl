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
    void enableMusic();
    virtual void stopMusic() override;
    virtual void startMusic() override;
    virtual void save() override;
    virtual void load() override;
    bool parseConfig(const char *filename);
    void setPrefix(const char *prefix);

protected:
    static void cleanup();
    virtual void preloadAssets() override;
    void initMusic();
    void initSounds();
    void keyReflector(SDL_Keycode key, uint8_t keyState);
    virtual bool loadScores() override;
    virtual bool saveScores() override;
    virtual void openMusicForLevel(int i) override;
    void drawTitleScreen(CFrame &bitmap);
    virtual void setupTitleScreen() override;
    virtual void takeScreenshot() override;

    typedef struct
    {
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_Texture *texture;
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
    CFrameSet *m_title;
    char m_scroll[SCROLLER_BUF_SIZE + 1];
    int m_scrollPtr;
};