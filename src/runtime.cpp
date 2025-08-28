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
#include <ctime>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include "runtime.h"
#include "game.h"
#include "shared/Frame.h"
#include "shared/FrameSet.h"
#include "shared/FileWrap.h"
#include "shared/implementers/mu_sdl.h"
#include "shared/implementers/sn_sdl.h"
#include "chars.h"
#include "skills.h"
#include "menu.h"
#include "strhelper.h"
#include "sounds.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
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
    m_startLevel = 0;
    m_mainMenu = new CMenu(MENUID_MAINMENU);
    m_gameMenu = new CMenu(MENUID_GAMEMENU);
    m_optionMenu = new CMenu(MENUID_OPTIONMENU);
    m_title = nullptr;
}

CRuntime::~CRuntime()
{
    if (m_app.window)
    {
        printf("detroying SDL2 objects\n");
        SDL_DestroyTexture(m_app.texture);
        SDL_DestroyRenderer(m_app.renderer);
        SDL_DestroyWindow(m_app.window);
        SDL_Quit();
    }

    if (m_music)
    {
        delete m_music;
    }

    if (m_title)
    {
        delete m_title;
    }

    if (m_scroll)
    {
        delete[] m_scroll;
    }

    if (m_credits)
    {
        delete[] m_credits;
    }

    if (m_mainMenu)
    {
        delete m_mainMenu;
    }

    if (m_gameMenu)
    {
        delete m_gameMenu;
    }

    if (m_optionMenu)
    {
        delete m_optionMenu;
    }
}

void CRuntime::paint()
{
    if (!m_bitmap)
    {
        m_bitmap = new CFrame(WIDTH, HEIGHT);
    }

    CFrame &bitmap = *m_bitmap;
    bitmap.fill(BLACK);
    switch (m_game->mode())
    {
    case CGame::MODE_LEVEL_INTRO:
    case CGame::MODE_RESTART:
    case CGame::MODE_GAMEOVER:
    case CGame::MODE_TIMEOUT:
        drawLevelIntro(bitmap);
        break;
    case CGame::MODE_PLAY:
        drawScreen(bitmap);
        if (m_gameMenuActive)
        {
            fazeScreen(bitmap, 2);
            resizeGameMenu();
            drawMenu(bitmap, *m_gameMenu, -1, (_HEIGHT - m_gameMenu->height()) / 2);
        }
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
        break;
    case CGame::MODE_OPTIONS:
        drawOptions(bitmap);
    case CGame::MODE_IDLE:
        break;
    };

    SDL_UpdateTexture(m_app.texture, NULL, bitmap.getRGB(), WIDTH * sizeof(uint32_t));
    int width;
    int height;
    SDL_GetWindowSize(m_app.window, &width, &height);
    SDL_Rect rectDest{0, 0, width, height};
    SDL_RenderCopy(m_app.renderer, m_app.texture, NULL, &rectDest);
    SDL_RenderPresent(m_app.renderer);
}

bool CRuntime::initSDL()
{
    printf("SDL Init() texture: %dx%d\n", WIDTH, HEIGHT);
    int rendererFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    int windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    else
    {
        const std::string title = m_config.count("title") ? m_config["title"] : "CS3v2 Runtime";
        m_app.window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 2 * WIDTH, 2 * HEIGHT, windowFlags);
        if (m_app.window == NULL)
        {
            fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        else
        {
            atexit(cleanup);
            //     SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
            m_app.renderer = SDL_CreateRenderer(m_app.window, -1, rendererFlags);
            if (m_app.renderer == nullptr)
            {
                fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
                return false;
            }

            m_app.texture = SDL_CreateTexture(
                m_app.renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
            if (m_app.texture == nullptr)
            {
                fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
                return false;
            }

            if (SDL_RenderSetVSync(m_app.renderer, 1) != 0)
            {
                fprintf(stderr, "SDL_RenderSetVSync Failed: %s\n", SDL_GetError());
                return false;
            }
        }
    }

    const std::string iconFile = m_prefix + m_config["icon"];
    CFrameSet frames;
    CFileWrap file;
    if (file.open(iconFile.c_str(), "rb") && frames.extract(file))
    {
        CFrame &frame = *frames[0];
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
            frame.getRGB(), frame.len(), frame.hei(), 32, frame.len() * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        SDL_SetWindowIcon(m_app.window, surface);
        SDL_FreeSurface(surface);
        file.close();
    }

#ifndef __EMSCRIPTEN__
    createResolutionList();
#endif
    m_resolution = findResolutionIndex();
    return true;
}

void CRuntime::cleanup()
{
    printf("cleanup()\n");
}

void CRuntime::run()
{
    mainLoop();
}

void CRuntime::doInput()
{
    int xAxisSensitivity = 8000 / 10 * m_xAxisSensitivity;
    int yAxisSensitivity = 8000 / 10 * m_yAxisSensitivity;
    SDL_GameController *controller;
    SDL_Event event;
    int joystick_index;
    while (SDL_PollEvent(&event))
    {
        uint8_t keyState = KEY_RELEASED;
        uint8_t buttonState = BUTTON_RELEASED;
        switch (event.type)
        {
        case SDL_KEYDOWN:
            keyState = KEY_PRESSED;
            [[fallthrough]];
        case SDL_KEYUP:
            keyReflector(event.key.keysym.sym, keyState);
            switch (event.key.keysym.sym)
            {
            case SDLK_w:
            case SDLK_UP:
                m_joyState[AIM_UP] = keyState;
                continue;
            case SDLK_s:
            case SDLK_DOWN:
                m_joyState[AIM_DOWN] = keyState;
                continue;
            case SDLK_a:
            case SDLK_LEFT:
                m_joyState[AIM_LEFT] = keyState;
                continue;
            case SDLK_d:
            case SDLK_RIGHT:
                m_joyState[AIM_RIGHT] = keyState;
                continue;
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED && !m_app.isFullscreen)
            {
                printf("resized\n");
                SDL_SetWindowSize(m_app.window, event.window.data1, event.window.data2);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button != 0 &&
                m_game->mode() == CGame::MODE_CLICKSTART)
            {
                setupTitleScreen();
#ifdef __EMSCRIPTEN__
                EM_ASM(
                    enableButtons(););
#endif
                initMusic();
                initSounds();
                initControllers();
            }
            break;

        case SDL_QUIT:
#ifdef __EMSCRIPTEN__
//           emscripten_cancel_main_loop();
#else
            if (m_app.isFullscreen)
            {
                toggleFullscreen();
            }
#endif
            printf("SQL_QUIT\n");
            m_isRunning = false;
            break;

        case SDL_CONTROLLERDEVICEADDED:
            joystick_index = event.cdevice.which;
            if (SDL_IsGameController(joystick_index))
            {
                controller = SDL_GameControllerOpen(joystick_index);
                if (controller)
                {
                    m_gameControllers.push_back(controller);
                    printf("Controller ADDED: %s (Index:%d)\n",
                           SDL_GameControllerName(controller), joystick_index);
                }
                else
                {
                    fprintf(stderr, "Failed to open new controller! SDL_Error: %s\n", SDL_GetError());
                }
            }
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            controller = SDL_GameControllerFromInstanceID(event.cdevice.which);
            if (controller)
            {
                std::string controllerName = SDL_GameControllerName(controller);
                printf("Controller REMOVED: %s\n", controllerName.c_str());
                for (auto it = m_gameControllers.begin(); it != m_gameControllers.end(); ++it)
                {
                    if (*it == controller)
                    {
                        SDL_GameControllerClose(*it);
                        m_gameControllers.erase(it);
                        break;
                    }
                }
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            buttonState = BUTTON_PRESSED;
            [[fallthrough]];

        case SDL_CONTROLLERBUTTONUP:
            controller = SDL_GameControllerFromInstanceID(event.cbutton.which);
            switch (event.cbutton.button)
            {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                m_joyState[AIM_UP] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                m_joyState[AIM_DOWN] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                m_joyState[AIM_LEFT] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                m_joyState[AIM_RIGHT] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_A:
                m_buttonState[BUTTON_A] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_B:
                m_buttonState[BUTTON_B] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_X:
                m_buttonState[BUTTON_X] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_Y:
                m_buttonState[BUTTON_Y] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_START:
                m_buttonState[BUTTON_START] = buttonState;
                continue;
            case SDL_CONTROLLER_BUTTON_BACK:
                m_buttonState[BUTTON_BACK] = buttonState;
                continue;
            default:
                printf("Controller: %s - Button %s: %s\n",
                       SDL_GameControllerName(controller),
                       buttonState ? "DOWN" : "UP",
                       SDL_GameControllerGetStringForButton((SDL_GameControllerButton)event.cbutton.button));
            }
            break;

        case SDL_CONTROLLERAXISMOTION:
            controller = SDL_GameControllerFromInstanceID(event.caxis.which);
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
            {
                if (event.caxis.value < -xAxisSensitivity)
                {
                    m_joyState[AIM_LEFT] = KEY_PRESSED;
                }
                else if (event.caxis.value > xAxisSensitivity)
                {
                    m_joyState[AIM_RIGHT] = KEY_PRESSED;
                }
                else
                {
                    m_joyState[AIM_LEFT] = KEY_RELEASED;
                    m_joyState[AIM_RIGHT] = KEY_RELEASED;
                }
            }
            else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY ||
                     event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
            {
                if (event.caxis.value < -yAxisSensitivity)
                {
                    m_joyState[AIM_UP] = KEY_PRESSED;
                }
                else if (event.caxis.value > yAxisSensitivity)
                {
                    m_joyState[AIM_DOWN] = KEY_PRESSED;
                }
                else
                {
                    m_joyState[AIM_UP] = KEY_RELEASED;
                    m_joyState[AIM_DOWN] = KEY_RELEASED;
                }
            }
            // Axis value ranges from -32768 to 32767
            // Apply a deadzone to ignore small movements
            else if (event.caxis.value < -8000 || event.caxis.value > 8000)
            { // Example deadzone
                printf("Controller: %s - Axis MOTION: %s -- Value: %d\n", SDL_GameControllerName(controller),
                       SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)event.caxis.axis),
                       event.caxis.value);
            }
            break;
        default:
            break;
        }
    }
}

bool CRuntime::fetchFile(const std::string &path, char **dest, const bool terminator)
{
    CFileWrap file;
    printf("loading: %s\n", path.c_str());
    if (file.open(path.c_str(), "rb"))
    {
        const int size = file.getSize();
        char *data = new char[terminator ? size + 1 : size];
        if (terminator)
        {
            data[size] = '\0';
        }
        file.read(data, size);
        file.close();
        printf("-> loaded %s: %d bytes\n", path.c_str(), size);
        *dest = data;
        return true;
    }
    else
    {
        fprintf(stderr, "failed to open %s\n", path.c_str());
        return false;
    }
}

void CRuntime::preloadAssets()
{
    CFileWrap file;
    CFrameSet **frameSets[] = {
        &m_tiles,
        &m_animz,
        &m_annie,
        &m_title,
    };
    for (size_t i = 0; i < m_assetFiles.size(); ++i)
    {
        const std::string filename = m_prefix + m_assetFiles[i];
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

    const std::string fontName = m_prefix + m_config["font"];
    fetchFile(fontName, reinterpret_cast<char **>(&m_fontData), false);

    const std::string creditsFile = m_prefix + m_config["credits"];
    if (fetchFile(creditsFile, &m_credits, true))
    {
        cleanUpCredits();
    }

    char *tmp;
    const std::string hintsFile = m_prefix + m_config["hints"];
    if (fetchFile(hintsFile, &tmp, true))
    {
        m_game->parseHints(tmp);
        delete[] tmp;
    }

    const std::string helpFile = m_prefix + m_config["help"];
    if (fetchFile(helpFile, &tmp, true))
    {
        parseHelp(tmp);
        delete[] tmp;
    }
}

void CRuntime::parseHelp(char *text)
{
    const char marker[] = {'_', '_', 'E', 'M', ' '};
    m_helptext.clear();
    char *p = text;
    while (p && *p)
    {
        char *n = strstr(p, "\n");
        if (n)
            *n = 0;
        char *r = strstr(p, "\r");
        if (r)
            *r = 0;
        char *e = std::max(r, n);
        if (memcmp(p, marker, sizeof(marker)) == 0)
        {
#ifndef __EMSCRIPTEN__
            m_helptext.push_back(p + sizeof(marker));
#endif
        }
        else
        {
            m_helptext.push_back(p);
        }
        // next line
        p = e ? e + 1 : nullptr;
    }
}

void CRuntime::cleanUpCredits()
{
    const size_t len = strlen(m_credits);
    size_t j = 0;
    for (size_t i = 0; i < len; ++i)
    {
        auto c = m_credits[i];
        if (c == '\n' ||
            c == '\r')
            continue;
        else if (c == '\t')
        {
            c = ' ';
        }
        if (i != j)
        {
            m_credits[j] = c;
        }
        ++j;
    }
    m_credits[j] = '\0';
}

void CRuntime::preRun()
{
    if (isTrue(m_config["clickstart"]))
    {
        m_game->setMode(CGame::MODE_CLICKSTART);
    }
    else
    {
        setupTitleScreen();
        initMusic();
        initSounds();
        initControllers();
    }
}

void CRuntime::initMusic()
{
    m_music = new CMusicSDL();
    openMusicForLevel(m_game->level());
    printf("volume: %d\n", m_music->getVolume());
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
        case SDLK_PERIOD:
            result = Key_Period;
            break;
        case SDLK_ESCAPE:
            result = Key_Escape;
            break;
        default:
            return;
        }
    }
    m_keyStates[result] = keyState;
}

bool CRuntime::loadScores()
{
#ifdef __EMSCRIPTEN__
    std::string path = HISCORE_FILE;
#else
    std::string path = m_workspace + HISCORE_FILE;
#endif
    printf("reading %s\n", path.c_str());
    CFileWrap file;
    if (file.open(path.c_str(), "rb"))
    {
        if (file.getSize() == sizeof(m_hiscores))
        {
            file.read(m_hiscores, sizeof(m_hiscores));
            file.close();
        }
        else
        {
            fprintf(stderr, "hiscore size mismatch. resetting to default.\n");
            clearScores();
        }
        return true;
    }
    fprintf(stderr, "can't read %s\n", path.c_str());
    return false;
}

bool CRuntime::saveScores()
{
#ifdef __EMSCRIPTEN__
    std::string path = HISCORE_FILE;
#else
    std::string path = m_workspace + HISCORE_FILE;
#endif

    CFileWrap file;
    if (file.open(path.c_str(), "wb"))
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
    fprintf(stderr, "can't write %s\n", path.c_str());
    return false;
}

void CRuntime::enableMusic(bool state)
{
    m_musicEnabled = state;
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
#ifdef __EMSCRIPTEN__
    std::string path = SAVEGAME_FILE;
#else
    std::string path = m_workspace + SAVEGAME_FILE;
#endif

    if (m_game->mode() != CGame::MODE_PLAY)
    {
        fprintf(stderr, "cannot save while not playing\n");
        return;
    }
    printf("writing: %s\n", path.c_str());
    std::string name{"Testing123"};
    FILE *tfile = fopen(path.c_str(), "wb");
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
        fprintf(stderr, "can't write:%s\n", path.c_str());
    }
}

void CRuntime::load()
{
#ifdef __EMSCRIPTEN__
    std::string path = SAVEGAME_FILE;
#else
    std::string path = m_workspace + SAVEGAME_FILE;
#endif
    m_game->setMode(CGame::MODE_IDLE);
    std::string name;
    printf("reading: %s\n", path.c_str());
    FILE *sfile = fopen(path.c_str(), "rb");
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
        fprintf(stderr, "can't read:%s\n", path.c_str());
    }
    m_game->setMode(CGame::MODE_PLAY);
    openMusicForLevel(m_game->level());
    centerCamera();
}

void CRuntime::initSounds()
{
    m_sound = new CSndSDL();
    CFileWrap file;
    for (size_t i = 0; i < m_soundFiles.size(); ++i)
    {
        const auto soundName = m_prefix + std::string("sounds/") + m_soundFiles[i];
        if (file.open(soundName.c_str(), "rb"))
        {
            const int size = file.getSize();
            auto sound = new uint8_t[size];
            file.read(sound, size);
            file.close();
            if (m_verbose)
                printf("loaded %s: %d bytes\n", soundName.c_str(), size);
            m_sound->add(sound, size, i + 1);
            delete[] sound;
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
    const std::string music = m_prefix + std::string("musics/") + m_musicFiles[i % m_musicFiles.size()];
    if (m_music && m_musicEnabled && m_music->open(music.c_str()))
    {
        m_music->play();
    }
}

/**
 * @brief Extract configuration from filename
 *
 * @param filename
 * @return true
 * @return false
 */
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
        char *next = processLine(p);
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
            else if (section == "config")
            {
                StringVector list;
                splitString2(std::string(p), list);
                if (list.size() == 2)
                {
                    m_config[list[0]] = list[1];
                }
                else
                {
                    fprintf(stderr, "string %s on line %d split into %zu parts\n", p, line, list.size());
                }
            }
            else
            {
                fprintf(stderr, "item for unknown section %s on line %d\n", section.c_str(), line);
            }
        }
        p = next;
        ++line;
    }
    delete[] tmp;
    return true;
}

/**
 * @brief Ser configuration option
 *
 * @param key
 * @param val
 */
void CRuntime::setConfig(const char *key, const char *val)
{
    m_config[key] = val;
}

/**
 * @brief Set path to game data files
 *
 * @param prefix
 */
void CRuntime::setPrefix(const char *prefix)
{
    m_prefix = prefix;
    addTrailSlash(m_prefix);
}

/**
 * @brief Set working directory
 *
 * @param workspace
 */
void CRuntime::setWorkspace(const char *workspace)
{
    m_workspace = workspace;
    addTrailSlash(m_workspace);
}

std::string &CRuntime::addTrailSlash(std::string &path)
{
    if (path.size() == 0)
    {
        path = "./";
    }
    else if (path.back() != '/' &&
             path.back() != '\\')
    {
        path += "/";
    }
    return path;
}

/**
 * @brief Draw Menu at coordinates baseX, baseY
 *
 * @param bitmap
 * @param menu
 * @param baseX
 * @param baseY
 */
void CRuntime::drawMenu(CFrame &bitmap, CMenu &menu, const int baseX, const int baseY)
{
    const int scaleX = menu.scaleX();
    const int scaleY = menu.scaleY();
    const int paddingY = menu.paddingY();
    const int spacingY = scaleY * FONT_SIZE + paddingY;
    for (size_t i = 0; i < menu.size(); ++i)
    {
        const CMenuItem &item = menu.at(i);
        const std::string &text = item.str();
        const int x = baseX == -1 ? (WIDTH - text.size() * FONT_SIZE * scaleX) / 2 : baseX;
        Color color = static_cast<int>(i) == menu.index() ? YELLOW : BLUE;
        if (item.isDisabled())
        {
            color = LIGHTGRAY;
        }
        drawFont(bitmap, x, baseY + i * spacingY, text.c_str(), color, CLEAR, scaleX, scaleY);
        if (static_cast<int>(i) == menu.index())
        {
            char tmp[2];
            tmp[0] = CHARS_TRIGHT;
            tmp[1] = '\0';
            drawFont(bitmap, 32, baseY + i * spacingY, tmp, RED, CLEAR, scaleX, scaleY);
        }
    }
}

/**
 * @brief Draw titlescreen
 *
 * @param bitmap
 */
void CRuntime::drawTitleScreen(CFrame &bitmap)
{
    bitmap.clear();
    if (m_title->getSize() == 0)
        return;

    auto &title = *(*m_title)[0];
    const int offsetY = 12;
    drawTitlePix(bitmap, offsetY);

    const int baseY = 2 * offsetY + title.hei();
    const Rect rect{
        .x = 8,
        .y = baseY,
        .width = WIDTH - 16,
        .height = HEIGHT - baseY - 24};
    drawRect(bitmap, rect, DARKRED, false);

    CMenu &menu = *m_mainMenu;
    const int menuBaseY = 100 - 8;
    drawMenu(bitmap, menu, -1, menuBaseY);
    drawScroller(bitmap);
}

/**
 * @brief Draw Titlescreen Pixmap
 *
 * @param bitmap
 * @param offsetY
 */
void CRuntime::drawTitlePix(CFrame &bitmap, const int offsetY)
{
    auto &title = *(*m_title)[0];
    int baseX = (bitmap.len() - title.len()) / 2;
    for (int y = 0; y < title.hei(); ++y)
    {
        for (int x = 0; x < title.len(); ++x)
        {
            const int xx = baseX + x;
            if (xx < bitmap.len() && xx >= 0)
                bitmap.at(baseX + x, y + offsetY) = title.at(x, y);
        }
    }
}

/**
 * @brief Draw scrolling text on Intro Screen
 *
 * @param bitmap
 */
void CRuntime::drawScroller(CFrame &bitmap)
{
    drawFont(bitmap, 0, HEIGHT - FONT_SIZE * 2, m_scroll, YELLOW);
    if (m_ticks & 1 && m_credits != nullptr)
    {
        for (size_t i = 0; i < scrollerBufSize() - 1; ++i)
            m_scroll[i] = m_scroll[i + 1];
        if (!m_credits[m_scrollPtr])
        {
            m_scrollPtr = 0;
        }
        else
        {
            m_scroll[scrollerBufSize() - 1] = m_credits[m_scrollPtr];
            ++m_scrollPtr;
        }
    }
}

void CRuntime::setupTitleScreen()
{
    size_t len = scrollerBufSize();
    memset(m_scroll, ' ', len);
    m_scroll[len] = 0;
    m_scrollPtr = 0;
    clearJoyStates();
    clearKeyStates();
    clearButtonStates();
    m_optionCooldown = MAX_OPTION_COOLDOWN;
    m_skill = m_game->skill();
    CMenu &menu = *m_mainMenu;
    menu.clear();
    if (_WIDTH > 450)
    {
        menu.setScaleX(4);
    }
    else if (_WIDTH > 350)
    {
        menu.setScaleX(3);
    }
    else
    {
        menu.setScaleX(2);
    }

    if (_HEIGHT >= 450)
    {
        menu.setScaleY(6);
    }
    else if (_HEIGHT >= 350)
    {
        menu.setScaleY(5);
    }
    else if (_HEIGHT >= 300)
    {
        menu.setScaleY(3);
    }
    else
    {
        menu.setScaleY(2);
    }
    menu.addItem(CMenuItem("NEW GAME", MENU_ITEM_NEW_GAME));
    menu.addItem(CMenuItem("LOAD GAME", MENU_ITEM_LOAD_GAME))
        .disable(!fileExists(getSavePath()));
    menu.addItem(CMenuItem("%s MODE", {"EASY", "NORMAL", "HARD"}, &m_skill))
        .setRole(MENU_ITEM_SKILL);
    menu.addItem(CMenuItem("LEVEL %.2d", 0, m_game->size() - 1, &m_startLevel))
        .setRole(MENU_ITEM_LEVEL);
#ifdef __EMSCRIPTEN__
    menu.addItem(CMenuItem("HIGH SCORES", MENU_ITEM_HISCORES));
#else
    menu.addItem(CMenuItem("OPTIONS", MENU_ITEM_OPTIONS));
#endif
    m_game->setMode(CGame::MODE_TITLE);
}

/**
 * @brief Take a screenshot of the game canvas and save it to a file
 *
 */
void CRuntime::takeScreenshot()
{
    CFrame bitmap(WIDTH, HEIGHT);
    bitmap.fill(BLACK);
    drawScreen(bitmap);
    auto rgba = bitmap.getRGB();
    for (int i = 0; i < bitmap.len() * bitmap.hei(); ++i)
    {
        if (rgba[i] == CLEAR)
            rgba[i] = BLACK;
    }
    bitmap.enlarge();
    uint8_t *png;
    int size;
    bitmap.toPng(png, size);
    CFileWrap file;
    char filename[64];
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm *localTime = std::localtime(&currentTime);
    sprintf(filename, "screenshot%.4d%.2d%.2d-%.2d%.2d%.2d.png",
            localTime->tm_year + 1900,
            localTime->tm_mon + 1,
            localTime->tm_mday,
            localTime->tm_hour,
            localTime->tm_min,
            localTime->tm_sec);
    std::string path = m_workspace + filename;
    if (file.open(path.c_str(), "wb"))
    {
        file.write(png, size);
        file.close();
        printf("screenshot save: %s\n", path.c_str());
    }
    else
    {
        fprintf(stderr, "can't write %s\n", path.c_str());
    }
    delete[] png;
}

/**
 * @brief Evaluate a string to determine if it contains a True value
 *
 * @param value
 * @return true
 * @return false
 */
bool CRuntime::isTrue(const std::string &value) const
{
    auto iequals = [](const std::string &a,
                      const std::string &b)
    {
        return std::equal(
            a.begin(), a.end(),
            b.begin(), b.end(),
            [](char a, char b)
            {
                return tolower(a) == tolower(b);
            });
    };
    return (iequals(value, "true")) ||
           (value == "1");
}

/**
 * @brief Initialize options from config
 *
 */
void CRuntime::initOptions()
{
    if (isTrue(m_config["hiscore_enabled"]))
    {
        enableHiScore();
    }
    if (isTrue(m_config["music_enabled"]))
    {
        enableMusic(true);
    }
    if (isTrue(m_config["fullscreen"]))
    {
        printf("is full screen?\n");
        toggleFullscreen();
    }
    if (m_config["viewport"] == "static")
    {
        m_cameraMode = CAMERA_MODE_STATIC;
        printf("using viewport static\n");
    }
    else if (m_config["viewport"] == "dynamic")
    {
        m_cameraMode = CAMERA_MODE_DYNAMIC;
        printf("using viewport dynamic\n");
    }
    else
    {
        printf("using viewport default\n");
    }

    if (m_config["healthbar"] == "hearts")
    {
        m_healthBar = HEALTHBAR_HEARTHS;
    }
    else
    {
        m_healthBar = HEALTHBAR_CLASSIC;
    }
}

/**
 * @brief  Function to toggle fullscreen mode
 *
 */

void CRuntime::toggleFullscreen()
{
#ifdef __EMSCRIPTEN__
    if (m_config["webfullscreen"] == "true")
    {
        if (SDL_SetWindowFullscreen(
                m_app.window,
                !m_app.isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
        {
            fprintf(stderr, "Fullscreen toggle error: %s\n", SDL_GetError());
        }
    }
    else
    {
        if (m_app.isFullscreen)
        {
            EmscriptenFullscreenStrategy strategy = {
                .scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_DEFAULT,
                .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE,
                .filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
                .canvasResizedCallback = NULL,
                .canvasResizedCallbackUserData = NULL};
            emscripten_enter_soft_fullscreen("#canvas", &strategy);
        }
        else
        {
            emscripten_exit_soft_fullscreen();
        }
    }
#else
    if (m_app.isFullscreen)
    {
        // Currently in fullscreen, switch to windowed
        printf("Switching to windowed mode.\n");
        SDL_SetWindowFullscreen(m_app.window, 0); // 0 means windowed mode

        // Restore original window size and position
        SDL_SetWindowSize(m_app.window, m_app.windowedWidth, m_app.windowedHeigth);
        SDL_SetWindowPosition(m_app.window, m_app.windowedX, m_app.windowedX);
    }
    else
    {
        // Currently in windowed, switch to fullscreen
        printf("Switching to fullscreen desktop mode.\n");

        // Save current windowed position and size before going fullscreen
        SDL_GetWindowPosition(m_app.window, &m_app.windowedX, &m_app.windowedX);
        SDL_GetWindowSize(m_app.window, &m_app.windowedWidth, &m_app.windowedHeigth);

        SDL_DisplayMode dm;
        SDL_GetCurrentDisplayMode(0, &dm);

        // Use SDL_WINDOW_FULLSCREEN_DESKTOP for borderless fullscreen
        // or SDL_WINDOW_FULLSCREEN for exclusive fullscreen
        SDL_SetWindowSize(m_app.window, dm.w, dm.h);
        SDL_SetWindowFullscreen(m_app.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
#endif
    int x, y, w, h;
    SDL_GetWindowPosition(m_app.window, &x, &y);
    SDL_GetWindowSize(m_app.window, &w, &h);
    printf("x:%d, y:%d, w:%d, h:%d\n", x, y, w, h);

    m_app.isFullscreen = !m_app.isFullscreen;
    m_fullscreen = (int)m_app.isFullscreen;
}

void CRuntime::sanityTest()
{
}

void CRuntime::resizeScroller()
{
    if (m_scroll)
        delete[] m_scroll;
    size_t len = scrollerBufSize();
    m_scroll = new char[len + 1];
    memset(m_scroll, ' ', len);
    m_scroll[len] = 0;
}

/**
 * @brief handle events for the title screen
 *
 */
void CRuntime::manageTitleScreen()
{
    manageMenu(*m_mainMenu);
}

/**
 * @brief handles the game menu
 *
 */

void CRuntime::manageGameMenu()
{
    manageMenu(*m_gameMenu);
}

/**
 * @brief handles the option screen
 *
 */

void CRuntime::manageOptionScreen()
{
    manageMenu(*m_optionMenu);
}

void CRuntime::manageMenu(CMenu &menu)
{
    CGame &game = *m_game;
    CMenuItem &item = menu.current();
    int oldValue = item.role() ? item.value() : 0;
    if (m_optionCooldown)
    {
        --m_optionCooldown;
    }
    else if (m_joyState[AIM_UP])
    {
        if (menu.up())
            m_sound->play(SOUND_SPRING05);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_joyState[AIM_DOWN])
    {
        if (menu.down())
            m_sound->play(SOUND_SPRING05);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_joyState[AIM_LEFT])
    {
        if (item.left())
            m_sound->play(SOUND_0009);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_joyState[AIM_RIGHT])
    {
        if (item.right())
            m_sound->play(SOUND_0009);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_keyStates[Key_Space] ||
             m_keyStates[Key_Enter] ||
             m_buttonState[BUTTON_A])
    {
        m_sound->play(SOUND_POUF);
        if (item.role() == MENU_ITEM_NEW_GAME ||
            item.role() == MENU_ITEM_SKILL ||
            item.role() == MENU_ITEM_LEVEL)
        {
            if (menu.id() == MENUID_GAMEMENU)
            {
                setupTitleScreen();
                return;
            }

            if (game.level() != m_startLevel)
            {
                game.setLevel(m_startLevel);
                openMusicForLevel(m_startLevel);
            }
            m_gameMenuActive = false;
            game.loadLevel(CGame::MODE_LEVEL_INTRO);
            centerCamera();
            game.setSkill(m_skill);
            game.resetStats();
            startCountdown(COUNTDOWN_INTRO);
        }
        else if (item.role() == MENU_ITEM_HISCORES)
        {
            if (!m_scoresLoaded)
            {
                m_scoresLoaded = loadScores();
            }
            m_countdown = HISCORE_DELAY;
            game.setMode(CGame::MODE_HISCORES);
        }
        else if (item.role() == MENU_ITEM_LOAD_GAME &&
                 !item.isDisabled())
        {
            load();
            m_gameMenuActive = false;
        }
        else if (item.role() == MENU_ITEM_SAVE_GAME)
        {
            save();
            m_gameMenuActive = false;
        }
        else if (item.role() == MENU_ITEM_OPTIONS)
        {
            if (menu.id() == MENUID_MAINMENU)
                initOptionMenu().addItem(CMenuItem("RETURN TO MAIN MENU", MENU_ITEM_RETURN_MAIN));
            else if (menu.id() == MENUID_GAMEMENU)
                initOptionMenu().addItem(CMenuItem("RETURN TO GAME", MENU_ITEM_RETURN_TO_GAME));
            m_game->setMode(CGame::MODE_OPTIONS);
            m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
        }
        else if (item.role() == MENU_ITEM_RETURN_MAIN)
        {
            setupTitleScreen();
            m_game->setMode(CGame::MODE_TITLE);
            m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
        }
        else if (item.role() == MENU_ITEM_RETURN_TO_GAME)
        {
            m_gameMenuActive = false;
            m_game->setMode(CGame::MODE_PLAY);
        }
    }

    if (item.role() == MENU_ITEM_MUSIC &&
        oldValue != item.value())
    {
        item.value() ? stopMusic() : startMusic();
    }
    else if (item.role() == MENU_ITEM_MUSIC_VOLUME &&
             oldValue != item.value())
    {
        int volume = std::min(item.value() * MUSIC_VOLUME_STEPS,
                              static_cast<int>(MUSIC_VOLUME_MAX));
        m_music->setVolume(volume);
    }
    else if (item.role() == MENU_ITEM_SND_VOLUME &&
             oldValue != item.value())
    {
        int volume = std::min(item.value() * MUSIC_VOLUME_STEPS,
                              static_cast<int>(MUSIC_VOLUME_MAX));
        m_sound->setVolume(volume);
    }
    else if (item.role() == MENU_ITEM_RESOLUTION &&
             oldValue != item.value())
    {
        Rez &rez = m_resolutions[item.value()];
        resize(rez.w, rez.h);
    }
    else if (item.role() == MENU_ITEM_FULLSCREEN &&
             oldValue != item.value())
    {
        toggleFullscreen();
    }
}

void CRuntime::setStartLevel(int level)
{
    m_startLevel = level;
}

bool CRuntime::fileExists(const std::string &name) const
{
    return (access(name.c_str(), F_OK) != -1);
}

const std::string CRuntime::getSavePath() const
{
#ifdef __EMSCRIPTEN__
    return SAVEGAME_FILE;
#else
    return m_workspace + SAVEGAME_FILE;
#endif
}

void CRuntime::resizeGameMenu()
{
    CMenu &menu = *m_gameMenu;
    if (WIDTH > MIN_WIDTH_FULL)
    {
        menu.setScaleX(2);
    }
    else
    {
        menu.setScaleX(1);
    }
}

void CRuntime::toggleGameMenu()
{
    m_gameMenuActive = !m_gameMenuActive;
    CMenu &menu = *m_gameMenu;
    menu.clear();
    menu.setScaleX(2);
    menu.setScaleY(2);
    menu.addItem(CMenuItem("NEW GAME", MENU_ITEM_NEW_GAME));
    menu.addItem(CMenuItem("LOAD GAME", MENU_ITEM_LOAD_GAME))
        .disable(!fileExists(getSavePath()));
    menu.addItem(CMenuItem("SAVE GAME", MENU_ITEM_SAVE_GAME));
#ifdef __EMSCRIPTEN__
    menu.addItem(CMenuItem("MUSIC VOLUME: %d%%", 0, 10, &m_musicVolume, 0, 10))
        .setRole(MENU_ITEM_MUSIC_VOLUME);
    menu.addItem(CMenuItem("SND VOLUME: %d%%", 0, 10, &m_sndVolume, 0, 10))
        .setRole(MENU_ITEM_SND_VOLUME);
#else
    menu.addItem(CMenuItem("OPTIONS", MENU_ITEM_OPTIONS));
#endif
    menu.addItem(CMenuItem("RETURN TO GAME", MENU_ITEM_RETURN_TO_GAME));
}

bool CRuntime::initControllers()
{
    // Initialize SDL's video and game controller subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Load game controller mappings from a file (optional, but good practice)
    // SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    // You can download this file from https://github.com/libsdl-org/sdl/blob/main/src/joystick/SDL_gamecontroller.c
    // or from https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt
    // and place it next to your executable.
    // SDL_GameControllerAddMappingsFromFile("data/gamecontrollerdb.txt");

    // Check for connected controllers initially
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            SDL_GameController *controller = SDL_GameControllerOpen(i);
            if (controller)
            {
                m_gameControllers.push_back(controller);
                printf("Opened Game Controller: %d: %s\n", i, SDL_GameControllerName(controller));
            }
            else
            {
                fprintf(stderr, "Could not open game controller:%d! SDL_Error: %s\n", i, SDL_GetError());
            }
        }
    }
    if (m_gameControllers.empty())
    {
        fprintf(stderr, "No game controllers found. Connect one now!\n");
    }
    return true;
}

bool CRuntime::isRunning() const
{
    return m_isRunning;
}

void CRuntime::init(CMapArch *maparch, int index)
{
    CGameMixin::init(maparch, index);
    resizeScroller();
}

size_t CRuntime::scrollerBufSize()
{
    return WIDTH / FONT_SIZE;
}

void CRuntime::drawOptions(CFrame &bitmap)
{
    CMenu &menu = *m_optionMenu;
    const int menuBaseY = 32;
    drawMenu(bitmap, menu, 48, menuBaseY);
}

CMenu &CRuntime::initOptionMenu()
{
    CMenu &menu = *m_optionMenu;
    menu.setScaleY(2);
    menu.setScaleX(1);
    menu.clear();
    // menu.addItem(CMenuItem("%s MUSIC", {"PLAY", "MUTE"}, &m_musicMuted))
    //     .setRole(MENU_ITEM_MUSIC);
    menu.addItem(CMenuItem("MUSIC VOLUME: %d%%", 0, 10, &m_musicVolume, 0, 10))
        .setRole(MENU_ITEM_MUSIC_VOLUME);
    menu.addItem(CMenuItem("SND VOLUME: %d%%", 0, 10, &m_sndVolume, 0, 10))
        .setRole(MENU_ITEM_SND_VOLUME);
    menu.addItem(CMenuItem("VIEWPORT: %s", {"STATIC", "DYNAMIC"}, &m_cameraMode))
        .setRole(MENU_ITEM_CAMERA);
#ifndef __EMSCRIPTEN__
    char tmp[16];
    std::vector<std::string> resolutions;
    for (const auto &rez : m_resolutions)
    {
        sprintf(tmp, "%dx%d", rez.w, rez.h);
        resolutions.push_back(tmp);
    }
    menu.addItem(CMenuItem("SCREEN: %s", resolutions, &m_resolution))
        .setRole(MENU_ITEM_RESOLUTION);
    menu.addItem(CMenuItem("DISPLAY: %s", {"WINDOWED", "FULLSCREEN"}, &m_fullscreen))
        .setRole(MENU_ITEM_FULLSCREEN);
#endif
    if (m_gameControllers.size() != 0)
    {
        menu.addItem(CMenuItem("X-AXIS SENSITIVITY: %d%%", 0, 10, &m_xAxisSensitivity, 0, 10))
            .setRole(MENU_ITEM_X_AXIS_SENTIVITY);
        menu.addItem(CMenuItem("Y-AXIS SENSITIVITY: %d%%", 0, 10, &m_yAxisSensitivity, 0, 10))
            .setRole(MENU_ITEM_Y_AXIS_SENTIVITY);
    }
    return menu;
}

/**
 * @brief Create a list of available resolutions
 *
 */
void CRuntime::createResolutionList()
{
    const int displayIndex = 0;
    int numModes = SDL_GetNumDisplayModes(displayIndex);
    if (numModes < 1)
    {
        fprintf(stderr, "SDL_GetNumDisplayModes failed: %s\n", SDL_GetError());
        return;
    }
    m_resolutions.clear();
    int pw = -1;
    int ph = -1;
    for (int i = 0; i < numModes; ++i)
    {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(displayIndex, i, &mode) != 0)
        {
            fprintf(stderr, "SDL_GetDisplayMode failed: %s\n", SDL_GetError());
            continue;
        }
        // ignore anything larger than 720p
        if (mode.w > 1280 || mode.h > 800)
            continue;
        if (mode.w != pw || mode.h != ph)
            m_resolutions.push_back({mode.w, mode.h});
        pw = mode.w;
        ph = mode.h;
    }
}

/**
 * @brief Find resolution index in list of available resolutions
 *
 * @return int
 */
int CRuntime::findResolutionIndex()
{
    const int w = _WIDTH * 2;
    const int h = _HEIGHT * 2;
    int i = 0;
    for (const auto &rez : m_resolutions)
    {
        if (rez.h == h && rez.w == w)
        {
            printf("found resolution %dx%d: %d\n", w, h, i);
            return i;
        }
        ++i;
    }
    printf("new resolution %dx%d: %d\n", w, h, i);
    m_resolutions.push_back({w, h});
    return i;
}

/**
 * @brief resize canvas. the provided values will be divided by 2
 *
 * @param w window width
 * @param h window height
 */
void CRuntime::resize(int w, int h)
{
    if (m_verbose)
        printf("switch to: %dx%d\n", w, h);
    setWidth(w / 2);
    setHeight(h / 2);
    SDL_SetWindowSize(m_app.window, w, h);
    auto oldTexture = m_app.texture;
    m_app.texture = SDL_CreateTexture(
        m_app.renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (m_app.texture == nullptr)
    {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        return;
    }
    SDL_DestroyTexture(oldTexture);
    if (m_bitmap)
    {
        delete m_bitmap;
    }
    m_bitmap = new CFrame(WIDTH, HEIGHT);
    resizeScroller();
    m_app.windowedWidth = w;
    m_app.windowedHeigth = h;
}

/**
 * @brief list all available resolution
 *
 * @param displayIndex
 */
void CRuntime::listResolutions(int displayIndex)
{
    int numModes = SDL_GetNumDisplayModes(displayIndex);
    if (numModes < 1)
    {
        fprintf(stderr, "SDL_GetNumDisplayModes failed:%s\n", SDL_GetError());
        return;
    }

    printf("Available display modes:\n");
    for (int i = 0; i < numModes; ++i)
    {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(displayIndex, i, &mode) != 0)
        {
            fprintf(stderr, "SDL_GetDisplayMode failed: %s\n", SDL_GetError());
            continue;
        }
        printf("Mode %d: %dx%d @ %dHz\n", i, mode.w, mode.h, mode.refresh_rate);
    }
}

/**
 * @brief Enable or disable verbose
 *
 * @param enable
 */
void CRuntime::setVerbose(bool enable)
{
    m_verbose = enable;
}

/**
 * @brief Notify the runtime that ESC was pressed. this is only relevant for the web version
 *
 */
void CRuntime::notifyExitFullScreen()
{
    m_app.isFullscreen = false;
    m_fullscreen = m_app.isFullscreen;
}