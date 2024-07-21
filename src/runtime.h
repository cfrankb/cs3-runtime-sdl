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

class CRuntime : public CGameMixin
{
public:
    CRuntime();
    virtual ~CRuntime();

    void paint();
    void run();
    bool SDLInit();
    void doInput();
    void preRun();
    void enableMusic();
    void stopMusic();
    void startMusic();
    virtual void save() override;
    virtual void load() override;

protected:
    static void cleanup();
    virtual void preloadAssets() override;
    void initMusic();
    void keyReflector(SDL_Keycode key, uint8_t keyState);
    virtual bool loadScores() override;
    virtual bool saveScores() override;

    typedef struct
    {
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_Texture *texture;
    } App;

    IMusic *m_music = nullptr;
    bool m_musicEnabled = false;
    App m_app;
};