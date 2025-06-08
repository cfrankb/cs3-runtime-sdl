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
#include "shared/interfaces/ISound.h"
#include "shared/implementers/mu_sdl.h"
#include "shared/implementers/sn_sdl.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
const char HISCORE_FILE[] = "/offline/hiscores-cs3.dat";
const char SAVEGAME_FILE[] = "/offline/savegame-cs3.dat";
#else
const char HISCORE_FILE[] = "hiscores-cs3.dat";
const char SAVEGAME_FILE[] = "savegame-cs3.dat";
#endif

CRuntime::CRuntime() : CGameMixin()
{
#ifdef __EMSCRIPTEN__
    EM_ASM(
        // Make a directory other than '/'
        FS.mkdir('/offline');
        // Then mount with IDBFS type
        FS.mount(IDBFS, {autoPersist : true}, '/offline');

        // Then sync
        FS.syncfs(true, function(err) {
            console.log(FS.readdir('/offline'));
            err ? console.log(err) : null; }));
#endif
    m_music = nullptr;
    memset(&m_app, 0, sizeof(App));
}

CRuntime::~CRuntime()
{
    SDL_DestroyTexture(m_app.texture);
    SDL_DestroyRenderer(m_app.renderer);
    SDL_DestroyWindow(m_app.window);
    SDL_Quit();

    if (m_music)
    {
        delete m_music;
    }
}

void CRuntime::paint()
{
    static CFrame bitmap(WIDTH, HEIGHT);
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
    case CGame::MODE_HISCORES:
        drawScores(bitmap);
        break;
    case CGame::MODE_CLICKSTART:
        drawPreScreen(bitmap);
        break;
    case CGame::MODE_HELP:
        drawHelpScreen(bitmap);
        break;
    case CGame::MODE_TITLE:
        drawTitleScreen(bitmap);
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
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    else
    {
        m_app.window = SDL_CreateWindow(
            "CS3v2 Runtime",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 2 * WIDTH, 2 * HEIGHT, windowFlags);
        if (m_app.window == NULL)
        {
            fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        else
        {
            atexit(cleanup);
            //            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
            m_app.renderer = SDL_CreateRenderer(m_app.window, -1, rendererFlags);
            if (m_app.renderer == nullptr)
            {
                fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
                return false;
            }

            m_app.texture = SDL_CreateTexture(
                m_app.renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
            if (m_app.texture == nullptr)
            {
                fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
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
        uint8_t keyState = KEY_RELEASED;
        switch (event.type)
        {
        case SDL_KEYDOWN:
            keyState = KEY_PRESSED;
        case SDL_KEYUP:
            keyReflector(event.key.keysym.sym, keyState);

            switch (event.key.keysym.sym)
            {
            case SDLK_UP:
                m_joyState[AIM_UP] = keyState;
                continue;
            case SDLK_DOWN:
                m_joyState[AIM_DOWN] = keyState;
                continue;
            case SDLK_LEFT:
                m_joyState[AIM_LEFT] = keyState;
                continue;
            case SDLK_RIGHT:
                m_joyState[AIM_RIGHT] = keyState;
                continue;
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
                m_game->setMode(CGame::MODE_TITLE);
#ifdef __EMSCRIPTEN__
                EM_ASM(
                    enableButtons(););
#endif
                initMusic();
                initSounds();
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
    CFrameSet **frameSets[] = {
        &m_tiles,
        &m_animz,
        &m_annie,
    };
    for (int i = 0; i < m_assetFiles.size(); ++i)
    {
        std::string filename = m_prefix + m_assetFiles[i];
        *frameSets[i] = new CFrameSet();
        if (file.open(filename.c_str(), "rb"))
        {
            printf("reading %s\n", filename.c_str());
            if ((*frameSets[i])->extract(file))
            {
                printf("extracted: %d\n", ((*frameSets[i])->getSize()));
            }
            file.close();
        }
        else
        {
            fprintf(stderr, "can't read: %s\n", filename.c_str());
        }
    }

    std::string fontName = m_prefix + "bitfont.bin";
    if (file.open(fontName.c_str(), "rb"))
    {
        int size = file.getSize();
        m_fontData = new uint8_t[size];
        file.read(m_fontData, size);
        file.close();
        printf("loaded %s: %d bytes\n", fontName.c_str(), size);
    }
    else
    {
        fprintf(stderr, "failed to open %s\n", fontName.c_str());
    }

    // extracting title
    m_title = new CFrameSet();
    if (file.open("data/cs3title.png", "rb"))
    {
        m_title->extract(file);
        file.close();
    }
}

void CRuntime::preRun()
{
    m_game->setMode(CGame::MODE_CLICKSTART);
}

void CRuntime::initMusic()
{
    m_music = new CMusicSDL();
    openMusicForLevel(m_game->level());
}

void CRuntime::keyReflector(SDL_Keycode key, uint8_t keyState)
{
    auto range = [](auto keyCode, auto start, auto end)
    {
        return keyCode >= start && keyCode <= end;
    };

    uint16_t result;
    if (range(key, SDLK_0, SDLK_9))
    {
        result = key - SDLK_0 + Key_0;
    }
    else if (range(key, SDLK_a, SDLK_z))
    {
        result = key - SDLK_a + Key_A;
    }
    else if (range(key, SDLK_F1, SDLK_F12))
    {
        result = key - SDLK_F1 + Key_F1;
    }
    else
    {
        switch (key)
        {
        case SDLK_SPACE:
            result = Key_Space;
            break;
        case SDLK_BACKSPACE:
            result = Key_BackSpace;
            break;
        case SDLK_RETURN:
            result = Key_Enter;
            break;
        default:
            return;
        }
    }
    m_keyStates[result] = keyState;
}

bool CRuntime::loadScores()
{
    printf("reading %s\n", HISCORE_FILE);
    CFileWrap file;
    if (file.open(HISCORE_FILE, "rb"))
    {
        if (file.getSize() == sizeof(m_hiscores))
        {
            file.read(m_hiscores, sizeof(m_hiscores));
            file.close();
        }
        else
        {
            fprintf(stderr, "size mismatch. resetting to default.\n");
            clearScores();
        }
        return true;
    }
    fprintf(stderr, "can't read %s\n", HISCORE_FILE);
    return false;
}

bool CRuntime::saveScores()
{
    CFileWrap file;
    if (file.open(HISCORE_FILE, "wb"))
    {
        file.write(m_hiscores, sizeof(m_hiscores));
        file.close();
#ifdef __EMSCRIPTEN__
        EM_ASM(
            FS.syncfs(function(err) {
                // Error
                err ? console.log(err) : null;
            }));
#endif
        return true;
    }
    fprintf(stderr, "can't write %s\n", HISCORE_FILE);
    return false;
}

void CRuntime::enableMusic()
{
    m_musicEnabled = true;
}

void CRuntime::stopMusic()
{
    m_musicEnabled = false;
    if (m_music)
    {
        m_music->stop();
    }
}

void CRuntime::startMusic()
{
    m_musicEnabled = true;
    if (m_music)
    {
        m_music->play();
    }
    else
    {
        initMusic();
    }
}

void CRuntime::save()
{
    if (m_game->mode() != CGame::MODE_LEVEL)
    {
        fprintf(stderr, "cannot save while not playing\n");
        return;
    }

    printf("writing: %s\n", SAVEGAME_FILE);
    std::string name{"Testing123"};
    FILE *tfile = fopen(SAVEGAME_FILE, "wb");
    if (tfile)
    {
        write(tfile, name);
        fclose(tfile);
#ifdef __EMSCRIPTEN__
        EM_ASM(
            FS.syncfs(function(err) {
                // Error
                err ? console.log(err) : null;
            }));
#endif
    }
    else
    {
        fprintf(stderr, "can't write:%s\n", SAVEGAME_FILE);
    }
}

void CRuntime::load()
{
    m_game->setMode(CGame::MODE_IDLE);
    std::string name;
    printf("reading: %s\n", SAVEGAME_FILE);
    FILE *sfile = fopen(SAVEGAME_FILE, "rb");
    if (sfile)
    {
        if (!read(sfile, name))
        {
            fprintf(stderr, "incompatible file\n");
        }
        fclose(sfile);
    }
    else
    {
        fprintf(stderr, "can't read:%s\n", SAVEGAME_FILE);
    }
    m_game->setMode(CGame::MODE_LEVEL);
    openMusicForLevel(m_game->level());
}

void CRuntime::initSounds()
{
    auto m_sound = new CSndSDL();
    CFileWrap file;
    for (int i = 0; i < m_soundFiles.size(); ++i)
    {
        const auto soundName = m_prefix + std::string("sounds/") + m_soundFiles[i];
        if (file.open(soundName.c_str(), "rb"))
        {
            const int size = file.getSize();
            auto sound = new uint8_t[size];
            file.read(sound, size);
            file.close();
            printf("loaded %s: %d bytes\n", soundName.c_str(), size);
            m_sound->add(sound, size, i + 1);
        }
        else
        {
            fprintf(stderr, "failed to open %s\n", soundName.c_str());
        }
    }
    m_game->attach(m_sound);
}

void CRuntime::openMusicForLevel(int i)
{
    const std::string music = m_prefix + std::string("music/") + m_musicFiles[i % m_musicFiles.size()];
    if (m_music && m_musicEnabled && m_music->open(music.c_str()))
    {
        m_music->play();
    }
}

bool CRuntime::parseConfig(const char *filename)
{
    CFileWrap file;
    if (!file.open(filename))
    {
        return false;
    }
    const int size = file.getSize();
    char *tmp = new char[size + 1];
    tmp[size] = '\0';
    file.read(tmp, size);
    file.close();

    std::string section;
    char *p = tmp;
    int line = 1;
    while (p && *p)
    {
        char *en = strstr(p, "\n");
        if (en)
        {
            *en = 0;
        }
        char *er = strstr(p, "\r");
        if (er)
        {
            *er = 0;
        }
        char *e = er > en ? er : en;
        char *c = strstr(p, "#");
        if (c)
        {
            *c = 0;
        }
        while (*p == ' ' || *p == '\t')
        {
            ++p;
        }
        int i = strlen(p) - 1;
        while (i >= 0 && (p[i] == ' ' || p[i] == '\t'))
        {
            p[i] = '\0';
            --i;
        }
        if (p[0] == '[')
        {
            ++p;
            char *pe = strstr(p, "]");
            if (pe)
                *pe = 0;
            if (!pe)
            {
                fprintf(stderr, "missing section terminator on line %d\n", line);
            }
            section = p;
        }
        else if (p[0])
        {
            if (section == "musics")
            {
                m_musicFiles.push_back(p);
            }
            else if (section == "sounds")
            {
                m_soundFiles.push_back(p);
            }
            else if (section == "assets")
            {
                m_assetFiles.push_back(p);
            }
        }
        p = e ? e + 1 : nullptr;
        ++line;
    }
    delete[] tmp;
    return true;
}

void CRuntime::setPrefix(const char *prefix)
{
    m_prefix = prefix;
}

void CRuntime::drawTitleScreen(CFrame &bitmap)
{
    bitmap.clear();
    auto &title = *(*m_title)[0];
    const auto offsetY = (HEIGHT - title.hei()) / 2;
    for (int y = 0; y < title.hei(); ++y)
    {
        for (int x = 0; x < title.len(); ++x)
        {
            bitmap.at(x, y + offsetY) = title.at(x, y);
        }
    }

    if ((m_ticks / 20) & 1)
    {
        const Rect rect = {
            .x = 0,
            .y = 180 - 38,
            .width = 320,
            .height = 16 + 5,
        };
        drawRect(bitmap, rect, BLACK, true);
    }
}