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
#include "runtime.h"
#include "game.h"
#include "shared/Frame.h"
#include "shared/FrameSet.h"
#include <cstring>
#include "shared/FileWrap.h"
#include "shared/music/mu_sdl.h"

CRuntime::CRuntime() : CGameMixin()
{
    memset(&m_app, 0, sizeof(App));
}

CRuntime::~CRuntime()
{
    SDL_DestroyTexture(m_app.texture);
    SDL_DestroyRenderer(m_app.renderer);
    SDL_DestroyWindow(m_app.window);
    SDL_Quit();
}

void CRuntime::paint()
{
    CFrame bitmap(WIDTH, HEIGHT);
    switch (m_game->mode())
    {
    case CGame::MODE_INTRO:
    case CGame::MODE_RESTART:
    case CGame::MODE_GAMEOVER:
        drawLevelIntro(bitmap);
        break;
    case CGame::MODE_LEVEL:
        drawScreen(bitmap);
        break;
    case CGame::MODE_CLICKSTART:
        drawPreScreen(bitmap);
    }

    SDL_UpdateTexture(m_app.texture, NULL, bitmap.getRGB(), WIDTH * sizeof(uint32_t));
    // SDL_RenderClear(m_app.renderer);
    SDL_RenderCopy(m_app.renderer, m_app.texture, NULL, NULL);
    SDL_RenderPresent(m_app.renderer);
}

bool CRuntime::SDLInit()
{
    int rendererFlags = SDL_RENDERER_ACCELERATED;
    int windowFlags = SDL_WINDOW_SHOWN;
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    else
    {
        m_app.window = SDL_CreateWindow(
            "CS3v2 Runtime",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 2 * WIDTH, 2 * HEIGHT, windowFlags);
        if (m_app.window == NULL)
        {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        else
        {
            atexit(cleanup);
            //            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
            m_app.renderer = SDL_CreateRenderer(m_app.window, -1, rendererFlags);
            if (m_app.renderer == nullptr)
            {
                printf("Failed to create renderer: %s\n", SDL_GetError());
                return false;
            }

            m_app.texture = SDL_CreateTexture(
                m_app.renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
            if (m_app.texture == nullptr)
            {
                printf("Failed to create texture: %s\n", SDL_GetError());
                return false;
            }
        }
    }
    return true;
}

void CRuntime::cleanup()
{
}

void CRuntime::run()
{
    mainLoop();
}

void CRuntime::doInput()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case SDLK_UP:
                m_joyState[AIM_UP] = KEY_RELEASED;
                break;
            case SDLK_DOWN:
                m_joyState[AIM_DOWN] = KEY_RELEASED;
                break;
            case SDLK_LEFT:
                m_joyState[AIM_LEFT] = KEY_RELEASED;
                break;
            case SDLK_RIGHT:
                m_joyState[AIM_RIGHT] = KEY_RELEASED;
                break;
            default:
                break;
            }
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_UP:
                m_joyState[AIM_UP] = KEY_PRESSED;
                break;
            case SDLK_DOWN:
                m_joyState[AIM_DOWN] = KEY_PRESSED;
                break;
            case SDLK_LEFT:
                m_joyState[AIM_LEFT] = KEY_PRESSED;
                break;
            case SDLK_RIGHT:
                m_joyState[AIM_RIGHT] = KEY_PRESSED;
                break;
            default:
                break;
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                SDL_SetWindowSize(m_app.window, event.window.data1, event.window.data2);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button != 0 &&
                m_game->mode() == CGame::MODE_CLICKSTART)
            {
                m_game->setMode(CGame::MODE_INTRO);
                initMusic();
            }
            break;

        case SDL_QUIT:
#ifdef __EMSCRIPTEN__
            //           emscripten_cancel_main_loop();
#endif
            exit(0);
            break;

        default:
            break;
        }
    }
}

void CRuntime::preloadAssets()
{
    CFileWrap file;

    typedef struct
    {
        const char *filename;
        CFrameSet **frameset;
    } asset_t;

    asset_t assets[] = {
        {"data/tiles.obl", &m_tiles},
        {"data/animz.obl", &m_animz},
        {"data/annie.obl", &m_annie},
    };

    for (int i = 0; i < 3; ++i)
    {
        asset_t &asset = assets[i];
        *(asset.frameset) = new CFrameSet();
        if (file.open(asset.filename, "rb"))
        {
            printf("reading %s\n", asset.filename);
            if ((*(asset.frameset))->extract(file))
            {
                printf("extracted: %d\n", (*(asset.frameset))->getSize());
            }
            file.close();
        }
    }

    const char fontName[] = "data/bitfont.bin";
    int size = 0;
    if (file.open(fontName, "rb"))
    {
        size = file.getSize();
        m_fontData = new uint8_t[size];
        file.read(m_fontData, size);
        file.close();
        printf("loaded %s: %d bytes\n", fontName, size);
    }
    else
    {
        printf("failed to open %s\n", fontName);
    }
}

void CRuntime::preRun()
{
    m_game->setMode(CGame::MODE_CLICKSTART);
}

void CRuntime::drawPreScreen(CFrame &bitmap)
{
    const char t[] = "CLICK TO START";
    int x = (WIDTH - strlen(t) * FONT_SIZE) / 2;
    int y = (HEIGHT - FONT_SIZE) / 2;
    bitmap.fill(BLACK);
    drawFont(bitmap, x, y, t, WHITE);
}

void CRuntime::initMusic()
{
    m_music = new CMusicSDL();
    const char music[] = "data/cs3idea_64.ogg";
    if (m_music && m_music->open(music))
    {
        m_music->play();
    }
}