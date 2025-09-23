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
#define LOG_TAG "runtime"
#include <ctime>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_render.h"
#include "runtime.h"
#include "game.h"
#include "shared/Frame.h"
#include "shared/FrameSet.h"
#include "shared/FileWrap.h"
#include "shared/FileMem.h"
#include "shared/implementers/mu_sdl.h"
#include "shared/implementers/sn_sdl.h"
#include "chars.h"
#include "skills.h"
#include "menu.h"
#include "strhelper.h"
#include "sounds.h"
#include "assetman.h"
#include "logger.h"
#include "attr.h"

#define _f(x) static_cast<float>(x)

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
    m_music = nullptr;
    m_app = App();
    m_startLevel = 0;
    m_mainMenu = new CMenu(MENUID_MAINMENU);
    m_gameMenu = new CMenu(MENUID_GAMEMENU);
    m_optionMenu = new CMenu(MENUID_OPTIONMENU);
    m_userMenu = new CMenu(MENUID_USERS);
    m_skillMenu = new CMenu(MENUID_SKILLS);
    m_title = nullptr;
    m_skill = 0;
    m_verbose = false;
    m_summary = Summary{0, 0, 0, 0};
    m_virtualKeyboard = new CGameUI();
    initVirtualKeyboard();

#ifdef __EMSCRIPTEN__
    mountFS();
#endif
}

CRuntime::~CRuntime()
{
    if (m_app.window)
    {
        LOGI("detroying SDL3 objects\n");
        SDL_DestroyTexture(m_app.texture);
        SDL_DestroyRenderer(m_app.renderer);
        SDL_DestroyWindow(m_app.window);
    }
    delete m_music;
    delete m_title;
    delete[] m_scroll;
    delete[] m_credits;
    delete m_mainMenu;
    delete m_gameMenu;
    delete m_optionMenu;
    delete m_userMenu;
    delete m_skillMenu;
    delete m_virtualKeyboard;
}

/**
 * @brief Composite the screen and send it to the renderer
 *
 */
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
        break;
    case CGame::MODE_IDLE:
        break;
    case CGame::MODE_USERSELECT:
        drawUserMenu(bitmap);
        break;
    case CGame::MODE_LEVEL_SUMMARY:
        drawLevelSummary(bitmap);
        break;
    case CGame::MODE_SKLLSELECT:
        drawSkillMenu(bitmap);
        break;
    case CGame::MODE_NEW_INPUTNAME:
        drawVirtualKeyboard(bitmap, "ENTER YOUR NAME", m_input);
    };

    SDL_UpdateTexture(m_app.texture, nullptr, bitmap.getRGB(), WIDTH * sizeof(uint32_t));
#if defined(__ANDROID__)
    Rect safeArea = getSafeAreaWindow();
    SDL_FRect rectDest{.x = _f(safeArea.x), .y = _f(safeArea.y), .w = _f(safeArea.width), .h = _f(safeArea.height)};
#else
    int width;
    int height;
    SDL_GetWindowSize(m_app.window, &width, &height);
    SDL_FRect rectDest{0, 0, static_cast<float>(width), static_cast<float>(height)};
#endif
    SDL_RenderTexture(m_app.renderer, m_app.texture, nullptr, &rectDest);
    SDL_RenderPresent(m_app.renderer);
}

/**
 * @brief Initialize the SDL3 library. Create window, texture and surface.
 *
 * @return true
 * @return false
 */
bool CRuntime::initSDL()
{
    LOGI("SDL Init()\n");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOGE("SDL could not initialize video! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

bool CRuntime::createSDLWindow()
{
    const std::string title = m_config.count("title") ? m_config["title"] : "CS3v2 Runtime";
    SDL_WindowFlags windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                  SDL_WINDOW_INPUT_FOCUS |
                                  SDL_WINDOW_MOUSE_CAPTURE;
#if defined(__ANDROID__) || defined(EMSCRIPTEN)
    windowFlags |= SDL_WINDOW_BORDERLESS;
#endif
    atexit(cleanup);
    const int width = PIXEL_SCALE * WIDTH;
    const int height = PIXEL_SCALE * HEIGHT;
    m_app.window = SDL_CreateWindow(title.c_str(), width, height, windowFlags);
    if (m_app.window == nullptr)
    {
        LOGE("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    else
    {
        LOGI("SDL Window created: %d x %d\n", width, height);
        m_app.renderer = SDL_CreateRenderer(m_app.window, nullptr);
        if (m_app.renderer == nullptr)
        {
            LOGE("Failed to create renderer: %s\n", SDL_GetError());
            return false;
        }
        LOGI("SDL Rendered created\n");

        // create streaming texture used to composite the screen
        m_app.texture = SDL_CreateTexture(
            m_app.renderer,
            SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        if (m_app.texture == nullptr)
        {
            LOGE("Failed to create texture: %s\n", SDL_GetError());
            return false;
        }
        LOGI("texture created: %dx%d\n", WIDTH, HEIGHT);

        // this prevents the pixelart from looking fuzy
        if (!SDL_SetTextureScaleMode(m_app.texture, SDL_SCALEMODE_NEAREST))
        {
            LOGE("SDL_SetTextureScaleMode  failed: %s\n", SDL_GetError());
            return false;
        }
    }

    if (!loadAppIcon())
    {
        LOGE("Failed to load appIcon\n");
    }

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    createResolutionList();
#endif
    m_resolution = findResolutionIndex();
    return true;
}

void CRuntime::debugSDL()
{
    int w = 0;
    int h = 0;
    if (!SDL_GetCurrentRenderOutputSize(m_app.renderer, &w, &h))
    {
        LOGE("SDL_GetCurrentRenderOutputSize failed: %s\n", SDL_GetError());
    }
    LOGI("SDL_GetCurrentRenderOutputSize() %d x %d\n", w, h);

    const int displayIndex = SDL_GetDisplayForWindow(m_app.window);
    SDL_DisplayOrientation currentOrientation = SDL_GetCurrentDisplayOrientation(displayIndex);
    const Rez rezScreen = getScreenSize();
    LOGI("SCREENSIZE: %d x %d; Orientation: %d\n", rezScreen.w, rezScreen.h, currentOrientation);

    Rect safeAreaWmd = getSafeAreaWindow();
    LOGI("safeAreaWnd: x: %d, y: %d, w: %d, h: %d\n", safeAreaWmd.x, safeAreaWmd.y, safeAreaWmd.width, safeAreaWmd.height);
    Rect safeAreaTexture = windowRect2textureRect(safeAreaWmd);
    LOGI("safeAreaTexture: x: %d, y: %d, w: %d, h: %d\n", safeAreaTexture.x, safeAreaTexture.y, safeAreaTexture.width, safeAreaTexture.height);

    int pixelW = 0, pixelH = 0;
    if (!SDL_GetWindowSizeInPixels(m_app.window, &pixelW, &pixelH))
    {
        LOGE("SDL_GetWindowSizeInPixels failed: %s\n", SDL_GetError());
    }
    LOGI("SDL_GetWindowSizeInPixels() w: %d h:%d\n", pixelW, pixelH);
}

void CRuntime::cleanup()
{
    LOGI("cleanup()\n");
}

void CRuntime::run()
{
    mainLoop();
}

bool CRuntime::isMenuActive()
{
    const int mode = m_game->mode();
    return mode == CGame::MODE_TITLE ||
           mode == CGame::MODE_USERSELECT ||
           mode == CGame::MODE_SKLLSELECT ||
           mode == CGame::MODE_OPTIONS ||
           m_gameMenuActive;
}

/**
 * @brief Read input devices for user inputs
 *
 */
void CRuntime::doInput()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
        SDL_ConvertEventToRenderCoordinates(m_app.renderer, &event);
        uint8_t keyState = KEY_RELEASED;
        switch (event.type)
        {
        case SDL_EVENT_KEY_DOWN:
            keyState = KEY_PRESSED;
            [[fallthrough]];
        case SDL_EVENT_KEY_UP:
            keyReflector(event.key.key, keyState);
            switch (event.key.key)
            {
            case SDLK_W:
            case SDLK_UP:
                m_joyState[AIM_UP] = keyState;
                continue;
            case SDLK_S:
            case SDLK_DOWN:
                m_joyState[AIM_DOWN] = keyState;
                continue;
            case SDLK_A:
            case SDLK_LEFT:
                m_joyState[AIM_LEFT] = keyState;
                continue;
            case SDLK_D:
            case SDLK_RIGHT:
                m_joyState[AIM_RIGHT] = keyState;
                continue;
            }
            break;

        case SDL_EVENT_JOYSTICK_ADDED:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_ADDED:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_REMOVED:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_REMAPPED:
            [[fallthrough]];
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
            onGamePadEvent(event);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            [[fallthrough]];
        case SDL_EVENT_FINGER_DOWN:
            [[fallthrough]];
        case SDL_EVENT_FINGER_UP:
            [[fallthrough]];
        case SDL_EVENT_FINGER_MOTION:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_MOTION:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_WHEEL:
            onMouseEvent(event);
            break;

        case SDL_EVENT_QUIT:
            onSDLQuit();
            break;

        case SDL_EVENT_WINDOW_SHOWN:
            LOGI("SDL_EVENT_WINDOW_SHOWN\n");
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            LOGI("SDL_EVENT_WINDOW_HIDDEN\n");
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            LOGI("SDL_EVENT_WINDOW_RESIZED\n");
            if (!m_app.isFullscreen)
            {
                LOGI("resized\n");
                SDL_SetWindowSize(m_app.window, event.window.data1, event.window.data2);
            }
            // updateScreenInfo();
            onOrientationChange();
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            LOGI("SDL_EVENT_WINDOW_MINIMIZED\n");
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            LOGI("SDL_EVENT_WINDOW_RESTORED\n");
            // updateScreenInfo();
            break;

        case SDL_EVENT_DISPLAY_ORIENTATION:
            LOGI("SDL_EVENT_DISPLAY_ORIENTATION\n");
            // updateScreenInfo();
            onOrientationChange();
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            LOGI("SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED\n");
            break;

        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            // LOGI("SDL_EVENT_WINDOW_MOUSE_ENTER\n");
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            memset(m_mouseButtons, BUTTON_RELEASED, sizeof(m_mouseButtons));
            // LOGI("SDL_EVENT_WINDOW_MOUSE_LEAVE\n");
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            LOGI("SDL_EVENT_WINDOW_FOCUS_GAINED\n");
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            LOGI("SDL_EVENT_WINDOW_FOCUS_LOST\n");
            break;

        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            LOGI("SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED\n");
            break;

        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
            LOGI("SDL_EVENT_WINDOW_SAFE_AREA_CHANGED\n");
            /*
            SDL_Rect safe;
            if (SDL_GetWindowSafeArea(window, &safe))
            {
                SDL_Log("Safe area changed: x=%d y=%d w=%d h=%d",
                        safe.x, safe.y, safe.w, safe.h);
            }
            */
            break;

        case SDL_EVENT_CLIPBOARD_UPDATE:
            LOGI("SDL_EVENT_CLIPBOARD_UPDATE\n");
            break;

        case SDL_EVENT_AUDIO_DEVICE_ADDED:
            LOGI("SDL_EVENT_AUDIO_DEVICE_ADDED\n");
            break;

        default:
            if (m_trace)
                LOGI("UNHANDLED EVENT: %d\n", event.type);
        }
    }
#ifdef __EMSCRIPTEN__
    readGamePadJs();
#endif
}

void CRuntime::onMouseEvent(const SDL_Event &event)
{
    const int mode = m_game->mode();
#if defined(__ANDROID__)
    bool isAndroid = true;
#else
    bool isAndroid = false;
#endif
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        m_mouseButtons[event.button.button] = BUTTON_RELEASED;
        pos_t pos = windowPos2texturePos(posF_t{event.button.x, event.button.y});
        if (m_trace)
            LOGI("SDL_EVENT_MOUSE_BUTTON_UP button: %d x=%f y=%f c=%u\n",
                 event.button.button, event.button.x, event.button.y, event.button.clicks);
        if (event.button.button == SDL_BUTTON_LEFT && isMenuActive())
        {
            m_buttonState[BUTTON_A] = BUTTON_RELEASED;
        }
        else if (mode == CGame::MODE_PLAY)
        {
            handleMouse(pos.x, pos.y);
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        m_mouseButtons[event.button.button] = BUTTON_PRESSED;
        pos_t pos = windowPos2texturePos(posF_t{event.button.x, event.button.y});
        if (m_trace)
        {
            LOGI("SDL_EVENT_MOUSE_BUTTON_DOWN button: %d x=%f y=%f c=%u\n",
                 event.button.button, event.button.x, event.button.y, event.button.clicks);
            LOGI("==>>> (%d, %d) SDL_EVENT_MOUSE_BUTTON_DOWN BTN%d[%d] from {%f, %f}\n",
                 pos.x, pos.y, event.button.button, event.button.clicks, event.button.x, event.button.y);
        }
        if (event.button.button != 0 && mode == CGame::MODE_CLICKSTART)
        {
            enterGame();
        }
        else if (event.button.button == SDL_BUTTON_LEFT &&
                 (menuItemAt(pos.x, pos.y) != INVALID))
        {
            // simulate button press
            m_buttonState[BUTTON_A] = BUTTON_PRESSED;
            manageMenu(*m_lastMenu);
        }
        else if (mode == CGame::MODE_PLAY)
        {
            handleMouse(pos.x, pos.y);
        }
        else if (event.button.button == SDL_BUTTON_LEFT && mode == CGame::MODE_NEW_INPUTNAME)
        {
            handleVKEY(pos.x, pos.y);
        }
    }
    else if (event.type == SDL_EVENT_FINGER_DOWN)
    {
        if (m_trace)
            LOGI("SDL_EVENT_FINGER_DOWN x=%f y=%f\n",
                 event.tfinger.x, event.tfinger.y);
        handleFingerDown(event.tfinger.x, event.tfinger.y);
    }
    else if (event.type == SDL_EVENT_FINGER_UP)
    {
        if (m_trace)
            LOGI("SDL_EVENT_FINGER_UP x=%f y=%f\n",
                 event.tfinger.x, event.tfinger.y);
    }
    else if (event.type == SDL_EVENT_FINGER_MOTION)
    {
        LOGI("SDL_EVENT_FINGER_MOTION\n");
    }
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        if (m_trace)
            LOGI("SDL_EVENT_MOUSE_MOTION x=%f y=%f\n",
                 event.motion.x, event.motion.y);
        pos_t pos = windowPos2texturePos(posF_t{event.motion.x, event.motion.y});
        if (isMenuActive())
        {
            followPointer(pos.x, pos.y);
        }
        else if (event.button.button == SDL_BUTTON_LEFT && mode == CGame::MODE_PLAY)
        {
            handleMouse(pos.x, pos.y);
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        // LOGI("SDL_EVENT_MOUSE_WHEEL x=%f y=%f\n",
        //      event.wheel.x, event.wheel.y);
        if (static_cast<int>(event.wheel.y) == 1)
        {
            m_joyState[AIM_LEFT] = KEY_PRESSED;
        }
        else if (static_cast<int>(event.wheel.y) == -1)
        {
            m_joyState[AIM_RIGHT] = KEY_PRESSED;
        }
    }
    else
    {
        LOGW("unhandled mouseEvent: %d\n", event.type);
    }
}

void CRuntime::onGamePadEvent(const SDL_Event &event)
{
    int xAxisSensitivity = 8000 / 10 * m_xAxisSensitivity;
    int yAxisSensitivity = 8000 / 10 * m_yAxisSensitivity;
    if (event.type == SDL_EVENT_JOYSTICK_ADDED)
    {
        LOGI("SDL_EVENT_JOYSTICK_ADDED\n");
    }
    else if (event.type == SDL_EVENT_GAMEPAD_REMAPPED)
    {
        LOGI("SDL_EVENT_GAMEPAD_REMAPPED\n");
    }
    else if (event.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        int joystick_index = event.gdevice.which;
        if (SDL_IsGamepad(joystick_index))
        {
            SDL_Gamepad *controller = SDL_OpenGamepad(joystick_index);
            if (controller)
            {
                m_gameControllers.push_back(controller);
                LOGI("Controller ADDED: %s (Index:%d)\n",
                     SDL_GetGamepadNameForID(event.cdevice.which), joystick_index);
            }
            else
            {
                LOGE("Failed to open new controller! SDL_Error: %s\n", SDL_GetError());
            }
        }
        if (m_gameControllers.size() != 0 &&
            !m_optionMenu->containsRole(MENU_ITEM_X_AXIS_SENTIVITY))
        {
            addGamePadOptions(*m_optionMenu);
        }
    }
    else if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        SDL_Gamepad *controller = SDL_GetGamepadFromID(event.gdevice.which);
        if (controller)
        {
            std::string controllerName = SDL_GetGamepadNameForID(event.gdevice.which);
            LOGI("Controller REMOVED: %s\n", controllerName.c_str());
            for (auto it = m_gameControllers.begin(); it != m_gameControllers.end(); ++it)
            {
                if (*it == controller)
                {
                    SDL_CloseGamepad(*it);
                    m_gameControllers.erase(it);
                    break;
                }
            }
        }
        if (m_gameControllers.size() == 0)
        {
            m_optionMenu->removeRole(MENU_ITEM_X_AXIS_SENTIVITY)
                .removeRole(MENU_ITEM_Y_AXIS_SENTIVITY);
        }
    }
    else if (RANGE(event.type, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_EVENT_GAMEPAD_BUTTON_UP))
    {
#ifndef __EMSCRIPTEN__
        uint8_t buttonState = BUTTON_RELEASED;
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            buttonState = BUTTON_PRESSED;

        SDL_Gamepad *controller = SDL_GetGamepadFromID(event.gdevice.which);
        switch (event.gbutton.button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            m_joyState[AIM_UP] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            m_joyState[AIM_DOWN] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            m_joyState[AIM_LEFT] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            m_joyState[AIM_RIGHT] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            m_buttonState[BUTTON_A] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            m_buttonState[BUTTON_B] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_WEST:
            m_buttonState[BUTTON_X] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_NORTH:
            m_buttonState[BUTTON_Y] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_START:
            m_buttonState[BUTTON_START] = buttonState;
            break;
        case SDL_GAMEPAD_BUTTON_BACK:
            m_buttonState[BUTTON_BACK] = buttonState;
            break;
        default:
            LOGI("Controller: %s - Button %s: %s [%d]\n",
                 SDL_GetGamepadName(controller),
                 buttonState ? "DOWN" : "UP",
                 SDL_GetGamepadStringForButton((SDL_GamepadButton)event.button.button),
                 event.button.button);
        }
#endif
    }
    else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        // case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        SDL_Gamepad *controller = SDL_GetGamepadFromID(event.gaxis.which);
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX ||
            event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX)
        {
            if (event.gaxis.value < -xAxisSensitivity)
            {
                m_joyState[AIM_LEFT] = KEY_PRESSED;
            }
            else if (event.gaxis.value > xAxisSensitivity)
            {
                m_joyState[AIM_RIGHT] = KEY_PRESSED;
            }
            else
            {
                m_joyState[AIM_LEFT] = KEY_RELEASED;
                m_joyState[AIM_RIGHT] = KEY_RELEASED;
            }
        }
        else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY ||
                 event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY)
        {
            if (event.gaxis.value < -yAxisSensitivity)
            {
                m_joyState[AIM_UP] = KEY_PRESSED;
            }
            else if (event.gaxis.value > yAxisSensitivity)
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
        else if (event.gaxis.value < -8000 || event.gaxis.value > 8000)
        { // Example deadzone
            LOGI("Controller: %s - Axis MOTION: %s -- Value: %d\n", SDL_GetGamepadName(controller),
                 SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event.gaxis.axis),
                 event.gaxis.value);
        }
    }
    else if (event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
    {
        LOGI("SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN\n");
    }
    else
    {
        LOGW("unhandled gamepad event: %d\n", event.type);
    }
}

void CRuntime::handleFingerDown(float x, float y)
{
    // float touchX = event.tfinger.x; // normalized [0.0, 1.0]
    // float touchY = event.tfinger.y;
    // 2. Convert Touch to Pixel Coordinates
    // int pixelX = static_cast<int>(touchX * windowWidth);
    // int pixelY = static_cast<int>(touchY * windowHeight);
    // 3. Convert to Surface Coordinates
    // If you're rendering to an SDL_Surface (e.g. software rendering): cpp
    // int surfaceX = pixelX * surface->w / windowWidth;
    // int surfaceY = pixelY * surface->h / windowHeight;
}

void CRuntime::handleVKEY(int x, int y)
{
    int btn = whichButton(*m_virtualKeyboard, x, y);
    if (btn != INVALID)
    {
        if (btn == VK_BACKSPACE)
        {
            if (m_input.length() != 0)
                m_input.pop_back();
        }
        else if (btn == VK_ENTER)
        {
            m_recordScore = false;
            const int j = m_scoreRank;
            if (j >= 0)
            {
                strncpy(m_hiscores[j].name, m_input.c_str(), sizeof(m_hiscores[j].name));
                saveScores();
            }
            m_game->setMode(CGame::MODE_HISCORES);
            m_countdown = HISCORE_DELAY;
            m_input.clear();
        }
        else if (m_input.length() < MAX_NAME_LENGTH - 1)
        {
            m_input += (char)btn;
        }
    }
}

void CRuntime::handleMouse(int x, int y)
{
    if (!m_ui.isVisible())
        return;
    // SDL_BUTTON_LEFT
    // SDL_BUTTON_RIGHT
    // SDL_BUTTON_MIDDLE
    clearVJoyStates();
    if (m_mouseButtons[SDL_BUTTON_LEFT] == BUTTON_PRESSED)
    {
        int btn = whichButton(m_ui, x, y);
        if (btn != INVALID)
        {
            m_vjoyState[btn] = BUTTON_PRESSED;
        }
    }
}

/**
 * @brief Read data into memory
 *
 * @param path filepath
 * @param dest memory buffer holding data
 * @param terminator add a null char terminator
 * @return true
 * @return false
 */
bool CRuntime::fetchFile(const std::string &path, char **dest, const bool terminator)
{
    CFileWrap file;
    if (m_verbose)
        LOGI("loading: %s\n", path.c_str());
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
        LOGI("-> loaded %s: %d bytes\n", path.c_str(), size);
        *dest = data;
        return true;
    }
    else
    {
        LOGE("failed to open %s\n", path.c_str());
        return false;
    }
}

void CRuntime::preloadAssets()
{
    CFrameSet **frameSets[] = {
        &m_tiles,
        &m_animz,
        &m_users,
        &m_title,
    };
    CFileMem mem;
    for (size_t i = 0; i < m_assetFiles.size(); ++i)
    {
        const std::string filename = AssetMan::getPrefix() + "pixels/" + m_assetFiles[i];
        *frameSets[i] = new CFrameSet();
        data_t data;
        if (AssetMan::read(filename, data))
        {
            LOGI("reading %s\n", filename.c_str());
            mem.replace(reinterpret_cast<char *>(data.ptr), data.size);
            if ((*frameSets[i])->extract(mem))
            {
                LOGI("extracted: %d\n", ((*frameSets[i])->getSize()));
            }
        }
        else
        {
            LOGE("can't read: %s\n", filename.c_str());
        }
    }

    data_t data;
    const std::string fontName = AssetMan::getPrefix() + m_config["font"];
    if (!AssetMan::read(fontName, data))
        return;
    m_fontData = data.ptr;

    const std::string creditsFile = AssetMan::getPrefix() + m_config["credits"];
    if (AssetMan::read(creditsFile, data, true))
    {
        m_credits = reinterpret_cast<char *>(data.ptr);
        cleanUpCredits();
    }

    const std::string hintsFile = AssetMan::getPrefix() + m_config["hints"];
    if (AssetMan::read(hintsFile, data, true))
    {
        m_game->parseHints(reinterpret_cast<char *>(data.ptr));
        AssetMan::free(data);
    }

    const std::string helpFile = AssetMan::getPrefix() + m_config["help"];
    if (AssetMan::read(helpFile, data, true))
    {
        parseHelp(reinterpret_cast<char *>(data.ptr));
        AssetMan::free(data);
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
        enterGame();
    }
}

void CRuntime::initMusic()
{
    m_music = new CMusicSDL();
    m_music->setVolume(ISound::MAX_VOLUME);
    LOGI("volume: %d\n", m_music->getVolume());
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
    else if (range(key, SDLK_A, SDLK_Z))
    {
        result = key - SDLK_A + Key_A;
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
    LOGI("reading %s\n", path.c_str());
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
            LOGE("hiscore size mismatch. resetting to default.\n");
            clearScores();
        }
        return true;
    }
    LOGE("can't read %s\n", path.c_str());
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
    LOGE("can't write %s\n", path.c_str());
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
        LOGE("cannot save while not playing\n");
        return;
    }
    LOGI("writing: %s\n", path.c_str());
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
        LOGE("can't write:%s\n", path.c_str());
    }
}

void CRuntime::load()
{
#ifdef __EMSCRIPTEN__
    std::string path = SAVEGAME_FILE;
#else
    std::string path = m_workspace + SAVEGAME_FILE;
#endif
    CGame &game = *m_game;
    game.setMode(CGame::MODE_IDLE);
    std::string name;
    LOGI("reading: %s\n", path.c_str());
    FILE *sfile = fopen(path.c_str(), "rb");
    if (sfile)
    {
        if (!read(sfile, name))
        {
            LOGE("incompatible file\n");
        }
        fclose(sfile);
    }
    else
    {
        LOGE("can't read:%s\n", path.c_str());
    }
    game.setMode(CGame::MODE_PLAY);
    openMusicForLevel(m_game->level());
    centerCamera();
    int userID = game.getUserID();
    loadColorMaps(userID);
}

void CRuntime::initSounds()
{
    LOGI("initSound\n");
    m_sound = new CSndSDL();
    CFileWrap file;
    for (size_t i = 0; i < m_soundFiles.size(); ++i)
    {
        const auto soundName = AssetMan::getPrefix() + std::string("sounds/") + m_soundFiles[i];
        bool result = m_sound->add(soundName.c_str(), i + 1);
        if (m_verbose)
            LOGI("%s %s\n", result ? "loaded" : "failed to load", soundName.c_str());
    }
    m_game->attach(m_sound);
}

void CRuntime::openMusicForLevel(int i)
{
    const std::string filename = m_musicFiles[i % m_musicFiles.size()];
    openMusic(filename);
}

void CRuntime::openMusic(const std::string &filename)
{
    LOGI("open music: %s\n", filename.c_str());
    const std::string music = getMusicPath(filename);
    if (m_music && m_musicEnabled && m_music->open(music.c_str()))
    {
        m_music->play();
    }
}

/**
 * @brief parse configuration from buffer
 *
 * @param filename
 * @return true
 * @return false
 */
bool CRuntime::parseConfig(uint8_t *buf)
{
    std::string section;
    char *p = reinterpret_cast<char *>(buf);
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
                LOGE("missing section terminator on line %d\n", line);
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
            else if (section == "users")
            {
                m_userNames.push_back(p);
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
                    LOGE("string %s on line %d split into %zu parts\n", p, line, list.size());
                }
            }
            else
            {
                LOGE("item for unknown section %s on line %d\n", section.c_str(), line);
            }
        }
        p = next;
        ++line;
    }
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
 * @brief Set working directory
 *
 * @param workspace
 */
void CRuntime::setWorkspace(const char *workspace)
{
    m_workspace = workspace;
    // TODO: check this later
    AssetMan::addTrailSlash(m_workspace);
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
    m_lastMenu = &menu;
    m_lastMenuBaseY = baseY;
    m_lastMenuBaseX = baseX;

    const int scaleX = menu.scaleX();
    const int scaleY = menu.scaleY();
    const int paddingY = menu.paddingY();
    const int spacingY = scaleY * FONT_SIZE + paddingY;
    for (size_t i = 0; i < menu.size(); ++i)
    {
        const CMenuItem &item = menu.at(i);
        const std::string &text = item.str();
        const int bx = baseX == -1 ? (WIDTH - text.size() * FONT_SIZE * scaleX) / 2 : baseX;
        const bool selected = static_cast<int>(i) == menu.index();
        Color color = selected ? YELLOW : BLUE;
        if (item.isDisabled())
        {
            color = LIGHTGRAY;
        }
        int x = bx;
        const int y = baseY + i * spacingY;
        if (static_cast<int>(i) == menu.index() && !menu.isCaretDisabled())
        {
            // draw red triangle
            char tmp[2];
            tmp[0] = CHARS_TRIGHT;
            tmp[1] = '\0';
            drawFont(bitmap, 32, y, tmp, RED, CLEAR, scaleX, scaleY);
        }
        if (item.type() == CMenuItem::ITEM_BAR)
        {
            const int scaleX = 1;
            size_t len = 0;
            for (size_t j = 0; j < item.size(); ++j)
                len += item.option(j).size() + (size_t)(j != 0);

            x = (WIDTH - len * FONT_SIZE * scaleX) / 2;
            for (size_t j = 0; j < item.size(); ++j)
            {
                const Color color = selected && (j == (size_t)item.value()) ? YELLOW : BLUE;
                const std::string option = item.option(j);
                drawFont(bitmap, x, y, option.c_str(), BLACK, color, scaleX, scaleY);
                x += (FONT_SIZE + 1) * option.size() * scaleX;
            }
            continue;
        }
        else if (item.role() == MENU_ITEM_SELECT_USER)
        {
            const uint16_t animeOffset = selected ? (m_ticks / 3) & 0x1f : 0;
            const CFrameSet &users = *m_users;
            CFrame &frame = *users[PLAYER_TOTAL_FRAMES * item.userData() + PLAYER_DOWN_INDEX + animeOffset];
            drawTileFaz(bitmap, x, y, frame, 0, selected ? COLOR_NOCHANGE : COLOR_GRAYSCALE);
            x += 32;
        }
        drawFont(bitmap, x, y, text.c_str(), color, CLEAR, scaleX, scaleY);
    }
}

/**
 * @brief Draw titlescreen
 *
 * @param bitmap
 */
void CRuntime::drawTitleScreen(CFrame &bitmap)
{
    bitmap.fill(BLACK);
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

/**
 * @brief Setup the title screen. Create Menu. Clear key/button states. etc
 *
 */
void CRuntime::setupTitleScreen()
{
    m_game->resetSugar();
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
    menu.addItem(CMenuItem("LEVEL %.2d", 0, m_game->size() - 1, &m_startLevel))
        .setRole(MENU_ITEM_LEVEL);

#if !defined(__ANDROID__)
    menu.addItem(CMenuItem("OPTIONS", MENU_ITEM_OPTIONS));
    menu.addItem(CMenuItem("HIGH SCORES", MENU_ITEM_HISCORES));
#else
    menu.addItem(CMenuItem("HIGH SCORES", MENU_ITEM_HISCORES));
    menu.addItem(CMenuItem("QUIT", MENU_ITEM_QUIT));
#endif
    m_game->setMode(CGame::MODE_TITLE);

    if (m_config["theme"] != "")
        openMusic(m_config["theme"]);
}

/**
 * @brief Take a screenshot of the game canvas and save it to a file
 *
 */
void CRuntime::takeScreenshot()
{
    CFrame bitmap(WIDTH, HEIGHT);
    if (m_bitmap)
        bitmap = *m_bitmap;
    else
        bitmap.fill(BLACK);
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
    snprintf(filename, sizeof(filename), "screenshot%.4d%.2d%.2d-%.2d%.2d%.2d.png",
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
        LOGI("screenshot saved: %s\n", path.c_str());
    }
    else
    {
        LOGE("can't write png: %s\n", path.c_str());
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
        LOGI("is full screen?\n");
        toggleFullscreen();
    }

    if (m_config["viewport"] == "static")
    {
        m_cameraMode = CAMERA_MODE_STATIC;
        LOGI("using viewport static\n");
    }
    else if (m_config["viewport"] == "dynamic")
    {
        m_cameraMode = CAMERA_MODE_DYNAMIC;
        LOGI("using viewport dynamic\n");
    }
    else
    {
        LOGI("using viewport default\n");
    }

    if (m_config["healthbar"] == "hearts")
    {
        m_healthBar = HEALTHBAR_HEARTHS;
    }
    else
    {
        m_healthBar = HEALTHBAR_CLASSIC;
    }

    if (isTrue(m_config["level_summary"]))
    {
        m_summaryEnabled = true;
    }

    if (isTrue(m_config["hardcore"]))
    {
        m_game->setDefaultLives(1);
    }

    m_trace = isTrue(m_config["trace"]);
}

/**
 * @brief  Function to toggle fullscreen mode on/off
 *
 */
void CRuntime::toggleFullscreen()
{
#ifdef __EMSCRIPTEN__
    if (m_config["webfullscreen"] == "true")
    {
        if (!SDL_SetWindowFullscreen(m_app.window, m_app.isFullscreen))
        {
            LOGE("Fullscreen toggle error: %s\n", SDL_GetError());
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
                .canvasResizedCallback = nullptr,
                .canvasResizedCallbackUserData = nullptr};
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
        LOGI("Switching to windowed mode.\n");
        if (!SDL_SetWindowFullscreen(m_app.window, false)) // false means windowed mode
        {
            LOGE("Switching to windowed mode failed: %s\n", SDL_GetError());
        }

        // Restore original window size and position
        if (!SDL_SetWindowSize(m_app.window, m_app.windowedWidth, m_app.windowedHeigth))
        {
            LOGE("SDL_SetWindowSize failed: %s\n", SDL_GetError());
        }

        if (!SDL_SetWindowPosition(m_app.window, m_app.windowedX, m_app.windowedX))
        {
            LOGE("SDL_SetWindowPosition failed: %s\n", SDL_GetError());
        }
    }
    else
    {
        // Currently in windowed, switch to fullscreen
        LOGI("Switching to fullscreen desktop mode.\n");

        // Save current windowed position and size before going fullscreen
        SDL_GetWindowPosition(m_app.window, &m_app.windowedX, &m_app.windowedX);
        SDL_GetWindowSize(m_app.window, &m_app.windowedWidth, &m_app.windowedHeigth);

        if (!SDL_SetWindowFullscreen(m_app.window, true))
        {
            LOGE("Switching to fullscreen mode failed: %s\n", SDL_GetError());
        }
    }
#endif
    if (m_verbose)
    {
        int x, y, w, h;
        SDL_GetWindowPosition(m_app.window, &x, &y);
        SDL_GetWindowSize(m_app.window, &w, &h);
        LOGI("x:%d, y:%d, w:%d, h:%d\n", x, y, w, h);
    }
    m_app.isFullscreen = !m_app.isFullscreen;
    m_fullscreen = (int)m_app.isFullscreen;
}

/**
 * @brief resize the scroller for the intro screen
 *
 */
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

/**
 * @brief handles the user menu (character selection screen
 *
 */
void CRuntime::manageUserMenu()
{
    manageMenu(*m_userMenu);
}

/**
 * @brief Handle interractions with the menu
 *
 * @param menu
 */
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
        if (menu.up() && m_sound != nullptr)
            m_sound->play(SOUND_SPRING05);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_joyState[AIM_DOWN])
    {
        if (menu.down() && m_sound != nullptr)
            m_sound->play(SOUND_SPRING05);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
    }
    else if (m_joyState[AIM_LEFT])
    {
        if (item.left() && m_sound != nullptr)
            m_sound->play(SOUND_0009);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
        m_joyState[AIM_LEFT] = KEY_RELEASED;
    }
    else if (m_joyState[AIM_RIGHT])
    {
        if (item.right() && m_sound != nullptr)
            m_sound->play(SOUND_0009);
        m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
        m_joyState[AIM_RIGHT] = KEY_RELEASED;
    }
    else if (m_keyStates[Key_Space] ||
             m_keyStates[Key_Enter] ||
             // m_mouseButtons[SDL_BUTTON_LEFT] ||
             m_buttonState[BUTTON_A])
    {
        if (m_sound)
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
            game.setLevel(m_startLevel);
            m_gameMenuActive = false;
            game.resetStats();
            initUserMenu();
            game.setMode(CGame::MODE_USERSELECT);
            clearKeyStates();
            m_optionCooldown = MAX_OPTION_COOLDOWN;
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
        else if (item.role() == MENU_ITEM_SELECT_USER)
        {
            int userID = item.userData();
            game.setUserID(userID);
            m_game->setMode(CGame::MODE_SKLLSELECT);
            initSkillMenu();
            loadColorMaps(userID);
            m_optionCooldown = MAX_OPTION_COOLDOWN;
        }
        else if (item.role() == MENU_ITEM_SKILLGROUP)
        {
            openMusicForLevel(m_startLevel);
            m_gameMenuActive = false;
            beginLevelIntro(CGame::MODE_LEVEL_INTRO);
            m_skill = item.userData();
            game.setSkill(m_skill);
            m_optionCooldown = DEFAULT_OPTION_COOLDOWN;
        }
        else if (item.role() == MENU_ITEM_QUIT)
        {
            m_isRunning = false;
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

    m_buttonState[BUTTON_A] = BUTTON_RELEASED;
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

/**
 * @brief Toggle Game Menu On/Off (show/hide)
 *
 */
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
#if !defined(__ANDROID__)
    menu.addItem(CMenuItem("OPTIONS", MENU_ITEM_OPTIONS));
#endif
    menu.addItem(CMenuItem("RETURN TO GAME", MENU_ITEM_RETURN_TO_GAME));
}

/**
 * @brief Initialize controller devices
 *
 * @return true
 * @return false
 */
bool CRuntime::initControllers()
{
    LOGI("initControllers()\n");

    // Initialize SDL's video and game controller subsystems
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        LOGE("SDL could not initialize gamepad! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Check for connected controllers initially
    int count;
    SDL_GetJoysticks(&count);
    for (int i = 0; i < count; ++i)
    {
        if (SDL_IsGamepad(i))
        {
            SDL_Gamepad *controller = SDL_OpenGamepad(i);
            if (controller)
            {
                m_gameControllers.push_back(controller);
                LOGI("Opened Game Controller: %d: %s\n", i, SDL_GetGamepadName(controller));
            }
            else
            {
                LOGE("Could not open game controller:%d! SDL_Error: %s\n", i, SDL_GetError());
            }
        }
    }
    if (m_gameControllers.empty())
    {
        LOGW("No game controllers found. Connect one now!\n");
    }

    const std::string controllerDB = AssetMan::getPrefix() + m_config["controllerdb"];
    if (!m_config["controllerdb"].empty() &&
        SDL_AddGamepadMappingsFromFile(controllerDB.c_str()) == -1)
    {
        LOGW("SDL_AddGamepadMappingsFromFile error: %s\n", SDL_GetError());
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
    if (isTrue(m_config["betaui"]))
    {
        m_ui.show();
    }
}

size_t CRuntime::scrollerBufSize()
{
    return WIDTH / FONT_SIZE;
}

/**
 * @brief Draw Ootion Menu
 *
 * @param bitmap
 */
void CRuntime::drawOptions(CFrame &bitmap)
{
    CMenu &menu = *m_optionMenu;
    const int menuBaseY = 32;
    drawMenu(bitmap, menu, 48, menuBaseY);
}

/**
 * @brief Initialize Option Menu
 *
 * @return CMenu&
 */
CMenu &CRuntime::initOptionMenu()
{
    CMenu &menu = *m_optionMenu;
    menu.setScaleY(2);
    menu.setScaleX(1);
    menu.clear();
    menu.addItem(CMenuItem("MUSIC VOLUME: %d%%", 0, 10, &m_musicVolume, 0, 10))
        .setRole(MENU_ITEM_MUSIC_VOLUME);
    menu.addItem(CMenuItem("SOUND VOLUME: %d%%", 0, 10, &m_sndVolume, 0, 10))
        .setRole(MENU_ITEM_SND_VOLUME);
    menu.addItem(CMenuItem("VIEWPORT: %s", {"STATIC", "DYNAMIC"}, &m_cameraMode))
        .setRole(MENU_ITEM_CAMERA);
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    char tmp[16];
    std::vector<std::string> resolutions;
    for (const auto &rez : m_resolutions)
    {
        snprintf(tmp, sizeof(tmp), "%dx%d", rez.w, rez.h);
        resolutions.push_back(tmp);
    }
    menu.addItem(CMenuItem("SCREEN: %s", resolutions, &m_resolution))
        .setRole(MENU_ITEM_RESOLUTION);
    menu.addItem(CMenuItem("DISPLAY: %s", {"WINDOWED", "FULLSCREEN"}, &m_fullscreen))
        .setRole(MENU_ITEM_FULLSCREEN);
#endif
    if (!m_gameControllers.empty())
    {
        addGamePadOptions(menu);
    }
    return menu;
}

void CRuntime::addGamePadOptions(CMenu &menu)
{
    menu.addItem(CMenuItem("X-AXIS SENSITIVITY: %d%%", 0, 10, &m_xAxisSensitivity, 0, 10))
        .setRole(MENU_ITEM_X_AXIS_SENTIVITY);
    menu.addItem(CMenuItem("Y-AXIS SENSITIVITY: %d%%", 0, 10, &m_yAxisSensitivity, 0, 10))
        .setRole(MENU_ITEM_Y_AXIS_SENTIVITY);
}

/**
 * @brief Initialize the User Selection Menu Options
 *
 * @return CMenu&
 */

CMenu &CRuntime::initUserMenu()
{
    CMenu &menu = *m_userMenu;
    menu.disableCaret();
    menu.clear();
    menu.setCurrent(0);
    menu.setScaleY(2);
    menu.setScaleX(1);
    for (size_t i = 0; i < m_userNames.size(); ++i)
    {
        menu.addItem(CMenuItem(m_userNames[i].c_str(), MENU_ITEM_SELECT_USER)).setUserData(static_cast<int>(i));
    }
    return menu;
}

/**
 * @brief Create a list of available resolutions
 *
 */
void CRuntime::createResolutionList()
{
    // Get all connected displays
    int numDisplays;
    SDL_DisplayID *display = SDL_GetDisplays(&numDisplays);
    if (numDisplays <= 0)
    {
        LOGE("No displays found: %s\n", SDL_GetError());
        return;
    }

    const int displayIndex = 0;
    int numModes;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display[displayIndex], &numModes);
    if (numModes < 1)
    {
        LOGE("SDL_GetFullscreenDisplayModes failed: %s\n", SDL_GetError());
        return;
    }
    m_resolutions.clear();
    int pw = -1;
    int ph = -1;
    for (int i = 0; i < numModes; ++i)
    {
        const SDL_DisplayMode &mode = *modes[i];
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
            LOGI("found resolution %dx%d: %d\n", w, h, i);
            return i;
        }
        ++i;
    }
    LOGI("new resolution %dx%d: %d\n", w, h, i);
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
        LOGI("switch to: %dx%d\n", w, h);
    setWidth(w / 2);
    setHeight(h / 2);
    SDL_SetWindowSize(m_app.window, w, h);
    auto oldTexture = m_app.texture;
    m_app.texture = SDL_CreateTexture(
        m_app.renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (m_app.texture == nullptr)
    {
        LOGE("Failed to create texture: %s\n", SDL_GetError());
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
    // Get all connected displays
    int numDisplays;
    SDL_DisplayID *display = SDL_GetDisplays(&numDisplays);
    if (numDisplays <= 0)
    {
        LOGE("No displays found: %s\n", SDL_GetError());
        return;
    }

    int numModes;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display[displayIndex], &numModes);
    if (numModes < 1)
    {
        LOGE("SDL_GetFullscreenDisplayModes failed:%s\n", SDL_GetError());
        return;
    }

    LOGI("Available display modes:\n");
    for (int i = 0; i < numModes; ++i)
    {
        const SDL_DisplayMode &mode = *modes[i];
        LOGI("Mode %d: %dx%d @ %fHz\n", i, mode.w, mode.h, mode.refresh_rate);
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

/**
 * @brief Draw User Selection Screen
 *
 * @param bitmap
 */
void CRuntime::drawUserMenu(CFrame &bitmap)
{
    int baseY = (_HEIGHT - m_userMenu->height()) / 2;
    drawFont(bitmap, 32, baseY - 32, "SELECT DIAMOND HUNTER", GRAY, BLACK, 1, 2);
    drawMenu(bitmap, *m_userMenu, 48, baseY);
}

/**
 * @brief check if all the music files are present, this only applies to desktop version
 *        because remote files cannot reliably be tested.
 *
 * @return true
 * @return false
 */
bool CRuntime::checkMusicFiles()
{
    bool result = true;
    for (const auto &file : m_musicFiles)
    {
        std::string music = getMusicPath(file);
        if (!fileExists(music))
        {
            LOGE("*** File not found: %s\n", music.c_str());
            result = false;
        }
    }
    return result;
}

/**
 * @brief Full path to the music file
 *
 * @param filename
 * @return std::string
 */
std::string CRuntime::getMusicPath(const std::string &filename)
{
#ifdef __EMSCRIPTEN__
    const std::string music = endswith(filename.c_str(), ".xm") ? AssetMan::getPrefix() + std::string("musics/") + filename : filename;
#else
    const std::string music = AssetMan::getPrefix() + std::string("musics/") + filename;
#endif
    return music;
}

/**
 * @brief Load the corresponding ColorMap for a player sprite
 *
 * @param userID
 */
void CRuntime::loadColorMaps(const int userID)
{
    std::string name = m_userNames[userID];
    std::string path = AssetMan::getPrefix() + "colormaps/" + name + ".ini";
    CFileWrap file;
    data_t data;
    if (AssetMan::read(path, data, true))
    {
        parseColorMaps(reinterpret_cast<char *>(data.ptr), m_colormaps);
        AssetMan::free(data);
    }
    else
    {
        LOGE("can't read %s\n", path.c_str());
    }
}

/**
 * @brief Formely enter the game. This can be directly or after leaving ClickStart screen
 *
 */
void CRuntime::enterGame()
{
    LOGI("enterGame()\n");
    initMusic();
    initSounds();
    initControllers();
    setupTitleScreen();
}

/**
 * @brief Draw Post Level Summary
 *
 * @param bitmap
 */
void CRuntime::drawLevelSummary(CFrame &bitmap)
{
    struct Text
    {
        std::string text;
        Color color;
        int scaleX;
        int scaleY;
    };
    std::vector<Text> listStr;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "LEVEL %.2d COMPLETED", m_game->level() + 1);
    if (_WIDTH < MIN_WIDTH_FULL)
        listStr.push_back({tmp, YELLOW, 1, 2});
    else
        listStr.push_back({tmp, YELLOW, 2, 2});
    snprintf(tmp, sizeof(tmp), "Fruits Collected: %d %%", m_summary.ppFruits);
    listStr.push_back({tmp, PINK, 1, 2});
    snprintf(tmp, sizeof(tmp), "Treasures Collected: %d %%", m_summary.ppBonuses);
    listStr.push_back({tmp, ORANGE, 1, 2});
    snprintf(tmp, sizeof(tmp), "Secrets: %d %%", m_summary.ppSecrets);
    listStr.push_back({tmp, GREEN, 1, 2});
    snprintf(tmp, sizeof(tmp), "Time Taken: %.2d:%.2d", m_summary.timeTaken / 60, m_summary.timeTaken % 60);
    listStr.push_back({tmp, CYAN, 1, 2});

    if (m_countdown == 0 && ((m_ticks >> 3) & 1))
    {
        listStr.push_back({"", BLACK, 1, 2});
        listStr.push_back({"PRESS SPACE TO CONTINUE", LIGHTGRAY, 1, 2});
    }
    else
    {
        listStr.push_back({"", BLACK, 1, 2});
        listStr.push_back({"", BLACK, 1, 2});
    }

    int height = 0;
    for (const auto &item : listStr)
    {
        height += FONT_SIZE * item.scaleY + FONT_SIZE;
    }

    int y = (_HEIGHT - height) / 2;
    for (const auto &item : listStr)
    {
        const auto &str = item.text;
        const int x = (_WIDTH - str.length() * FONT_SIZE * item.scaleX) / 2;
        drawFont(bitmap, x, y, str.c_str(), item.color, BLACK, item.scaleX, item.scaleY);
        y += FONT_SIZE * item.scaleY + FONT_SIZE;
    }
}

/**
 * @brief Manage Post Level Summary Screen
 *
 */
void CRuntime::manageLevelSummary()
{
    // resume to next level
    if (m_countdown == 0 &&
        (m_keyStates[Key_Space] ||
         m_keyStates[Key_Enter] ||
         m_buttonState[BUTTON_A] ||
         m_mouseButtons[SDL_BUTTON_LEFT]))
    {
        nextLevel();
    }
}

/**
 * @brief Initialize Post Level Summary Screen Data
 *
 */
void CRuntime::initLevelSummary()
{
    CGame &game = *m_game;
    const MapReport &report0 = game.originalMapReport();
    const MapReport report1 = game.currentMapReport();
    m_summary.ppFruits = report0.fruits ? 100 * (report0.fruits - report1.fruits) / report0.fruits : 100;
    m_summary.ppBonuses = report0.bonuses ? 100 * (report0.bonuses - report1.bonuses) / report0.bonuses : 100;
    m_summary.ppSecrets = report0.secrets ? 100 * (report0.secrets - report1.secrets) / report0.secrets : 100;
    m_summary.timeTaken = game.timeTaken();
}

/**
 * @brief Change game music for specific situation
 *
 * @param mode
 */
void CRuntime::changeMoodMusic(CGame::GameMode mode)
{
    if (mode == CGame::MODE_GAMEOVER)
    {
        openMusic(m_config["gameover_theme"]);
    }
    else if (mode == CGame::MODE_RESTART)
    {
        // openMusic("death.ogg");
    }
    else if (mode == CGame::MODE_PLAY)
    {
        openMusicForLevel(m_game->level());
    }
    else if (mode == CGame::MODE_LEVEL_INTRO)
    {
    }
}

int CRuntime::menuItemAt(int x, int y)
{
    if (!m_lastMenu || !isMenuActive())
        return -1;

    CMenu &menu = *m_lastMenu;
    int baseY = m_lastMenuBaseY;
    int startY = baseY;
    for (size_t i = 0; i < menu.size(); ++i)
    {
        int h = menu.scaleY() * FONT_SIZE;
        if (RANGE(y, startY, startY + h))
        {
            if (m_trace)
                LOGI("menuItem at: %d %d ==> %zu\n", x, y, i);
            return i;
        }
        startY += h + menu.paddingY();
    }
    return -1;
}

void CRuntime::followPointer(int x, int y)
{
    if (!m_lastMenu)
        return;

    CMenu &menu = *m_lastMenu;
    int i = menuItemAt(x, y);
    if (i != INVALID)
        menu.setCurrent(i);
}

void CRuntime::drawSkillMenu(CFrame &bitmap)
{
    const int baseY = (_HEIGHT - m_userMenu->height()) / 2;
    const char *t = "GAME DIFFICULTY";
    int x = (_WIDTH - strlen(t) * FONT_SIZE * 2) / 2;
    drawFont(bitmap, x, baseY - 32, t, GRAY, BLACK, 2, 2);
    drawMenu(bitmap, *m_skillMenu, -1, baseY);
}

void CRuntime::manageSkillMenu()
{
    manageMenu(*m_skillMenu);
}

void CRuntime::initSkillMenu()
{
    CMenu &menu = *m_skillMenu;
    menu.disableCaret();
    menu.clear();
    menu.setScaleX(2);
    menu.setScaleY(2);
    menu.addItem(CMenuItem("EASY", MENU_ITEM_SKILLGROUP).setUserData(SKILL_EASY));
    menu.addItem(CMenuItem("NORMAL", MENU_ITEM_SKILLGROUP).setUserData(SKILL_NORMAL));
    menu.addItem(CMenuItem("HARD", MENU_ITEM_SKILLGROUP).setUserData(SKILL_HARD));
    menu.setCurrent(m_skill);
}

void CRuntime::clearVJoyStates()
{
    memset(m_vjoyState, 0, sizeof(m_vjoyState));
}

void CRuntime::clearMouseButtons()
{
    memset(m_mouseButtons, 0, sizeof(m_mouseButtons));
}

Rez CRuntime::getScreenSize()
{
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(display);
    return Rez{.w = mode->w, .h = mode->h};
}

Rez CRuntime::getWindowSize()
{
    int w;
    int h;
    if (!SDL_GetWindowSizeInPixels(m_app.window, &w, &h))
    {
        LOGE("SDL_GetWindowSizeInPixels failed ! SDL_Error: %s\n", SDL_GetError());
    }
    return Rez{.w = w, .h = h};
}

CRuntime::pos_t CRuntime::windowPos2texturePos(posF_t pos)
{
#if defined(__ANDROID__)
    Rect safeRect = getSafeAreaWindow();
    pos.x -= safeRect.x;
    pos.y -= safeRect.y;
    float w = _WIDTH;
    float h = _HEIGHT;
    return {.x = static_cast<int>(w * pos.x / safeRect.width), .y = static_cast<int>(h * pos.y / safeRect.height)};
#else
    return {.x = static_cast<int>(pos.x) / PIXEL_SCALE, .y = static_cast<int>(pos.y) / PIXEL_SCALE};
#endif
}

void CRuntime::onOrientationChange()
{
    Rez rez = getScreenSize();
    bool isLandscape = rez.w > rez.h;
    LOGI("Orientation changed - %s mode", isLandscape ? "Landscape" : "Portrait");
}

CRuntime::Rect CRuntime::getSafeAreaWindow()
{
    SDL_Rect safe;
    if (!SDL_GetWindowSafeArea(m_app.window, &safe))
    {
        LOGE("SDL_GetWindowSafeArea() failed; error: %s\n", SDL_GetError());
    }
    return Rect{.x = safe.x, .y = safe.y, .width = safe.w, .height = safe.h};
}

CRuntime::Rect CRuntime::windowRect2textureRect(const Rect &wRect)
{
    Rez rez = getWindowSize();
    float w = (float)_WIDTH * 2;
    float h = (float)_HEIGHT * 2;
    auto _c = [rez, w, h](auto u)
    {
        return static_cast<int>(w * (float)u / rez.w);
    };
    return Rect{.x = _c(wRect.x), .y = _c(wRect.y), .width = _c(wRect.width), .height = _c(wRect.height)};
}

/**
 * @brief Initialize the VirtualKeyboard UI (android)
 *
 */
void CRuntime::initVirtualKeyboard()
{
    const int BTN_SIZE = 16;
    const int BTN_PADDING = 4;
    const int rowSize = 12;
    CGameUI &ui = *m_virtualKeyboard;
    ui.setMargin(FONT_SIZE);
    const char chars[]{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        ".,/@$%^&*!"};

    int i = 0;
    for (const char &c : chars)
    {
        if (c == '\0')
            break;
        char t[2];
        t[0] = c;
        t[1] = '\0';
        const int row = i / rowSize;
        const int col = i % rowSize;
        button_t button{
            .id = c,
            .x = col * (BTN_SIZE + BTN_PADDING),
            .y = row * (BTN_SIZE + BTN_PADDING),
            .width = BTN_SIZE,
            .height = BTN_SIZE,
            .text = t,
            .color = WHITE,
        };
        ui.addButton(button);
        ++i;
    }
    std::vector<const char *> keys = {
        "SPACE", "BKSP", "ENTER"};
    const char id[] = {VK_SPACE, VK_BACKSPACE, VK_ENTER};
    int x = 0;
    const int y = ui.height() + BTN_PADDING * 2;
    i = 0;
    for (const auto &key : keys)
    {
        int width = FONT_SIZE * 2 * strlen(key);
        button_t button{
            .id = id[i],
            .x = x,
            .y = y,
            .width = width,
            .height = BTN_SIZE,
            .text = key,
            .color = WHITE,
        };
        x += width + BTN_PADDING;
        ui.addButton(button);
        ++i;
    }
    m_input = "";
    LOGI("virtualkeyboard %d x %d\n", ui.width(), ui.height());
}

/**
 * @brief Draw a virtual keyboard UI on Android
 *
 * @param bitmap
 * @param title
 * @param buffer
 */
void CRuntime::drawVirtualKeyboard(CFrame &bitmap, const std::string &title, std::string &buffer)
{
    const int scaleX = 2;
    const int scaleY = 2;
    CGameUI &ui = *m_virtualKeyboard;
    const char end = (m_ticks >> 4) & 1 ? (char)CHARS_CARET : ' ';
    drawUI(bitmap, ui);
    std::string t = title;
    int x = (_WIDTH - t.length() * FONT_SIZE * scaleX) / 2;
    drawFont(bitmap, x, FONT_SIZE, t.c_str(), LIGHTGRAY, CLEAR, scaleX, scaleY);
    t = buffer + end;
    x = (_WIDTH - t.length() * FONT_SIZE * scaleX) / 2;
    drawFont(bitmap, x, 48, t.c_str(), YELLOW, CLEAR, scaleX, scaleY);
}

void CRuntime::onSDLQuit()
{
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
    SDL_Delay(300);
#else
    if (m_app.isFullscreen)
    {
        toggleFullscreen();
    }
#endif
    LOGI("SQL_QUIT\n");
    m_isRunning = false;
}

bool CRuntime::loadAppIcon()
{
    const std::string iconFile = AssetMan::getPrefix() + m_config["icon"];
    CFrameSet frames;
    data_t data{nullptr, 0};
    if (AssetMan::read(iconFile, data))
    {
        CFileMem mem;
        mem.replace(reinterpret_cast<char *>(data.ptr), data.size);
        if (frames.extract(mem) && frames.getSize() != 0)
        {
            CFrame &frame = *frames[0];
            SDL_Surface *surface = SDL_CreateSurfaceFrom(
                frame.len(), frame.hei(), SDL_PIXELFORMAT_RGBA32, frame.getRGB(), frame.len() * sizeof(uint32_t));
            SDL_SetWindowIcon(m_app.window, surface);
            SDL_DestroySurface(surface);
        }
        AssetMan::free(data);
        return true;
    }
    return false;
}

#ifdef __EMSCRIPTEN__
EM_JS(int, pollGamepadButtons, (), {
    const gp = navigator.getGamepads()[0];
    if (!gp)
        return;
    let i = 0;
    let buttonMask = 0;
    for (const button of gp.buttons)
    {
        if (button.pressed)
        {
            buttonMask |= (1 << i);
        }
        ++i;
        if (i == 31)
            break;
    }
    return buttonMask;
});

void CRuntime::readGamePadJs()
{
    int buttonMask = pollGamepadButtons();
    if (buttonMask && (m_game->mode() == CGame::MODE_CLICKSTART))
    {
        enterGame();
    }
    else if (buttonMask && isTrue(m_config["trace"]))
    {
        printf("buttons: %.4x\n", buttonMask);
    }
    for (int i = 0; i < Button_Count; ++i)
    {
        if (buttonMask & (1 << i))
        {
            m_buttonState[i] = BUTTON_PRESSED;
        }
        else
        {
            m_buttonState[i] = BUTTON_RELEASED;
        }
    }
}

void CRuntime::mountFS()
{

    EM_ASM(
        // Make a directory other than '/'
        FS.mkdir('/offline');
        // Then mount with IDBFS type
        FS.mount(IDBFS, {autoPersist : true}, '/offline');

        // Then sync
        FS.syncfs(true, function(err) {
            console.log(FS.readdir('/offline'));
            err ? console.log(err) : null; })

    );
}
#endif
