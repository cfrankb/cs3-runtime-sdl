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
#include <algorithm>
#include <memory>
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
#include "color.h"
#include "filemacros.h"
#include "font8x8.h"
#include "statedata.h"
#include "recorder.h"
#include "gamestats.h"
#include "tilesdata.h"
#include "tilesdefs.h"
#include "animator.h"
#include "gamesfx.h"
#include "states.h"
#include "sheetdata.h"
#include "boss.h"

#define _f(x) static_cast<float>(x)

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
const char HISCORE_FILE[] = "hiscores-cs3.dat";
const char SAVEGAME_FILE[] = "savegame-cs3.dat";

constexpr const int SCALE = 2;
constexpr const float SCALEF = 2.0f;
constexpr const int SCALE_2X = 4;

BitmapFont8x8 g_font;

namespace SColor
{
    constexpr SDL_Color WHITE{255, 255, 255, 255};           // #ffffff
    constexpr SDL_Color YELLOW{255, 255, 0, 255};            // #ffff00
    constexpr SDL_Color CYAN{0, 255, 255, 255};              // #00ffff
    constexpr SDL_Color GREEN{0, 255, 0, 255};               // #00ff00
    constexpr SDL_Color PURPLE{255, 0, 255, 255};            // #ff00ff
    constexpr SDL_Color LIGHTGRAY{0xa9, 0xa9, 0xa9, 0xff};   // #a9a9a9
    constexpr SDL_Color DEEPSKYBLUE(0x00, 0xBF, 0xFF, 0xff); // #00bfff
    constexpr SDL_Color BLACK(0x00, 0x00, 0x00, 0xff);       // #000000
    constexpr SDL_Color DARKGREEN(0, 128, 0, 255);           // #008000
    constexpr SDL_Color RED(255, 0, 0, 255);                 // #ff0000
    constexpr SDL_Color ORANGE(0xf5, 0x9b, 0x14, 255);       // #f59b14
    constexpr SDL_Color DARKORANGE(0xff, 0x8c, 0x00, 255);   // #ff8c00
    constexpr SDL_Color CORAL(0xff, 0x7f, 0x50, 255);        // #ff7f50
    constexpr SDL_Color PINK(0xff, 0xc0, 0xcb, 255);         // #ffc0cb
    constexpr SDL_Color HOTPINK(0xff, 0x69, 0xb4, 255);      // #ff69b4
    constexpr SDL_Color DEEPPINK(0xff, 0x14, 0x93, 255);     // #ff1493
    constexpr SDL_Color DARKGRAY(0x44, 0x44, 0x44, 255);     // #444444

    auto toSColor = [](const uint32_t c) -> SDL_Color
    {
        SDL_Color out;
        out.r = c & 0xFF;
        out.g = (c >> 8) & 0xFF;
        out.b = (c >> 16) & 0xFF;
        out.a = 255;
        return out;
    };
};

CRuntime::CRuntime() : CGameMixin()
{
    m_music = nullptr;
    m_app = App();
    m_startLevel = 0;
    m_mainMenu = std::make_unique<CMenu>(MENUID_MAINMENU);
    m_gameMenu = std::make_unique<CMenu>(MENUID_GAMEMENU);
    m_optionMenu = std::make_unique<CMenu>(MENUID_OPTIONMENU);
    m_userMenu = std::make_unique<CMenu>(MENUID_USERS);
    m_skillMenu = std::make_unique<CMenu>(MENUID_SKILLS);
    m_skill = 0;
    m_verbose = false;
    m_summary = Summary{0, 0, 0, 0};
    m_hasFocus = false;
    m_virtualKeyboard = new CGameUI();
    initVirtualKeyboard();

#ifdef __EMSCRIPTEN__
    mountFS();
#endif
}

CRuntime::~CRuntime()
{
    if (m_music != nullptr)
        m_music->close();
    if (m_app.window)
    {
        if (!m_quiet)
            LOGI("detroying SDL3 objects");
        SDL_DestroyTexture(m_app.texture);
        SDL_DestroyRenderer(m_app.renderer);
        SDL_DestroyWindow(m_app.window);
    }
    delete[] m_scroll;
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
        m_bitmap = new CFrame(getWidth(), getHeight());
    }

    SDL_SetRenderDrawColor(m_app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_app.renderer);
    bool renderBitmap = true;

    CFrame &bitmap = *m_bitmap;
    bitmap.fill(BLACK);

    switch (m_game->mode())
    {
    case CGame::MODE_LEVEL_INTRO:
    case CGame::MODE_RESTART:
    case CGame::MODE_GAMEOVER:
    case CGame::MODE_TIMEOUT:
    case CGame::MODE_CHUTE:
        drawLevelIntro();
        renderBitmap = false;
        break;
    case CGame::MODE_PLAY:
        // drawScreen(bitmap);
        drawScreen();
        renderBitmap = false;
        break;
    case CGame::MODE_HISCORES:
        drawScores(bitmap);
        break;
    case CGame::MODE_CLICKSTART:
        drawPreScreen();
        renderBitmap = false;
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
        break;
    case CGame::MODE_TEST:
        drawTest(bitmap);
    };

    if (renderBitmap)
    {
        SDL_UpdateTexture(m_app.texture, nullptr, bitmap.getRGB().data(), getWidth() * sizeof(uint32_t));
#if defined(__ANDROID__)
        rect_t safeArea = getSafeAreaWindow();
        SDL_FRect rectDest{.x = _f(safeArea.x), .y = _f(safeArea.y), .w = _f(safeArea.width), .h = _f(safeArea.height)};
#else
        int width;
        int height;
        SDL_GetWindowSize(m_app.window, &width, &height);
        SDL_FRect rectDest{0, 0, static_cast<float>(width), static_cast<float>(height)};
#endif
        SDL_RenderTexture(m_app.renderer, m_app.texture, nullptr, &rectDest);
    }
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
    if (!m_quiet)
        LOGI("SDL Init()");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOGE("SDL could not initialize video! SDL_Error: %s", SDL_GetError());
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
    const int width = PIXEL_SCALE * getWidth();
    const int height = PIXEL_SCALE * getHeight();
    m_app.window = SDL_CreateWindow(title.c_str(), width, height, windowFlags);
    if (m_app.window == nullptr)
    {
        LOGE("Window could not be created! SDL_Error: %s", SDL_GetError());
        return false;
    }
    else
    {
        if (!m_quiet)
            LOGI("SDL Window created: %d x %d", width, height);
        m_app.renderer = SDL_CreateRenderer(m_app.window, nullptr);
        if (m_app.renderer == nullptr)
        {
            LOGE("Failed to create renderer: %s", SDL_GetError());
            return false;
        }
        if (!m_quiet)
            LOGI("SDL Rendered created");

        // SDL_SetRenderIntegerScale(renderer, SDL_TRUE);
        // SDL_SetRenderScale(renderer, scale, scale);

        // create streaming texture used to composite the screen
        m_app.texture = SDL_CreateTexture(
            m_app.renderer,
            SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, getWidth(), getHeight());
        if (m_app.texture == nullptr)
        {
            LOGE("Failed to create texture: %s", SDL_GetError());
            return false;
        }
        if (!m_quiet)
            LOGI("texture created: %dx%d", getWidth(), getHeight());

        // this prevents the pixelart from looking fuzy
        if (!SDL_SetTextureScaleMode(m_app.texture, SDL_SCALEMODE_NEAREST))
        {
            LOGE("SDL_SetTextureScaleMode  failed: %s", SDL_GetError());
            return false;
        }
    }

    if (!loadAppIcon())
    {
        LOGE("Failed to load appIcon");
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
        LOGE("SDL_GetCurrentRenderOutputSize failed: %s", SDL_GetError());
    }
    LOGI("SDL_GetCurrentRenderOutputSize() %d x %d", w, h);

    const int displayIndex = SDL_GetDisplayForWindow(m_app.window);
    SDL_DisplayOrientation currentOrientation = SDL_GetCurrentDisplayOrientation(displayIndex);
    const Rez rezScreen = getScreenSize();
    LOGI("SCREENSIZE: %d x %d; Orientation: %d", rezScreen.w, rezScreen.h, currentOrientation);

    rect_t safeAreaWmd = getSafeAreaWindow();
    LOGI("safeAreaWnd: x: %d, y: %d, w: %d, h: %d", safeAreaWmd.x, safeAreaWmd.y, safeAreaWmd.width, safeAreaWmd.height);
    rect_t safeAreaTexture = windowRect2textureRect(safeAreaWmd);
    LOGI("safeAreaTexture: x: %d, y: %d, w: %d, h: %d", safeAreaTexture.x, safeAreaTexture.y, safeAreaTexture.width, safeAreaTexture.height);

    int pixelW = 0, pixelH = 0;
    if (!SDL_GetWindowSizeInPixels(m_app.window, &pixelW, &pixelH))
    {
        LOGE("SDL_GetWindowSizeInPixels failed: %s", SDL_GetError());
    }
    LOGI("SDL_GetWindowSizeInPixels() w: %d h:%d", pixelW, pixelH);
}

void CRuntime::cleanup()
{
    LOGI("cleanup()");
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
        // SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
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
            LOGI("SDL_EVENT_WINDOW_SHOWN");
            break;
        case SDL_EVENT_WINDOW_HIDDEN:
            LOGI("SDL_EVENT_WINDOW_HIDDEN");
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            LOGI("SDL_EVENT_WINDOW_RESIZED");
            if (!m_app.isFullscreen)
            {
                LOGI("resized");
                SDL_SetWindowSize(m_app.window, event.window.data1, event.window.data2);
            }
            // updateScreenInfo();
            onOrientationChange();
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            LOGI("SDL_EVENT_WINDOW_MINIMIZED");
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            LOGI("SDL_EVENT_WINDOW_RESTORED");
            // updateScreenInfo();
            break;

        case SDL_EVENT_DISPLAY_ORIENTATION:
            LOGI("SDL_EVENT_DISPLAY_ORIENTATION");
            // updateScreenInfo();
            onOrientationChange();
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            LOGI("SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED");
            break;

        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            // LOGI("SDL_EVENT_WINDOW_MOUSE_ENTER");
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            memset(m_mouseButtons, BUTTON_RELEASED, sizeof(m_mouseButtons));
            // LOGI("SDL_EVENT_WINDOW_MOUSE_LEAVE");
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            LOGI("SDL_EVENT_WINDOW_FOCUS_GAINED");
            m_hasFocus = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            LOGI("SDL_EVENT_WINDOW_FOCUS_LOST");
            m_hasFocus = false;
            break;

        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            LOGI("SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED");
            break;

        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
            LOGI("SDL_EVENT_WINDOW_SAFE_AREA_CHANGED");
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
            LOGI("SDL_EVENT_CLIPBOARD_UPDATE");
            break;

        case SDL_EVENT_AUDIO_DEVICE_ADDED:
            LOGI("SDL_EVENT_AUDIO_DEVICE_ADDED");
            break;

        default:
            if (m_trace)
                LOGI("UNHANDLED EVENT: %d", event.type);
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
    (void)isAndroid;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        m_mouseButtons[event.button.button] = BUTTON_RELEASED;
        pos_t pos = windowPos2texturePos(posF_t{event.button.x, event.button.y});
        if (m_trace)
            LOGI("SDL_EVENT_MOUSE_BUTTON_UP button: %d x=%f y=%f c=%u",
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
            LOGI("SDL_EVENT_MOUSE_BUTTON_DOWN button: %d x=%f y=%f c=%u",
                 event.button.button, event.button.x, event.button.y, event.button.clicks);
            LOGI("==>>> (%d, %d) SDL_EVENT_MOUSE_BUTTON_DOWN BTN%d[%d] from {%f, %f}",
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
            LOGI("SDL_EVENT_FINGER_DOWN x=%f y=%f",
                 event.tfinger.x, event.tfinger.y);
        handleFingerDown(event.tfinger.x, event.tfinger.y);
    }
    else if (event.type == SDL_EVENT_FINGER_UP)
    {
        if (m_trace)
            LOGI("SDL_EVENT_FINGER_UP x=%f y=%f",
                 event.tfinger.x, event.tfinger.y);
    }
    else if (event.type == SDL_EVENT_FINGER_MOTION)
    {
        LOGI("SDL_EVENT_FINGER_MOTION");
    }
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        if (m_trace)
            LOGI("SDL_EVENT_MOUSE_MOTION x=%f y=%f",
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
        // LOGI("SDL_EVENT_MOUSE_WHEEL x=%f y=%f",
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
        LOGW("unhandled mouseEvent: %d", event.type);
    }
}

void CRuntime::onGamePadEvent(const SDL_Event &event)
{
    int xAxisSensitivity = 8000 / 10 * m_xAxisSensitivity;
    int yAxisSensitivity = 8000 / 10 * m_yAxisSensitivity;
    if (event.type == SDL_EVENT_JOYSTICK_ADDED)
    {
        LOGI("SDL_EVENT_JOYSTICK_ADDED");
    }
    else if (event.type == SDL_EVENT_GAMEPAD_REMAPPED)
    {
        LOGI("SDL_EVENT_GAMEPAD_REMAPPED");
    }
    else if (event.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        int joystick_index = event.gdevice.which;
        if (SDL_IsGamepad(joystick_index))
        {
            SDL_Gamepad *controller = SDL_OpenGamepad(joystick_index);
            if (controller)
            {
                m_gameControllers.emplace_back(controller);
                LOGI("Controller ADDED: %s (Index:%d)",
                     SDL_GetGamepadNameForID(event.cdevice.which), joystick_index);
            }
            else
            {
                LOGE("Failed to open new controller! SDL_Error: %s", SDL_GetError());
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
            LOGI("Controller REMOVED: %s", controllerName.c_str());
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
            LOGI("Controller: %s - Button %s: %s [%d]",
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
            LOGI("Controller: %s - Axis MOTION: %s -- Value: %d", SDL_GetGamepadName(controller),
                 SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event.gaxis.axis),
                 event.gaxis.value);
        }
    }
    else if (event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
    {
        LOGI("SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN");
    }
    else
    {
        LOGW("unhandled gamepad event: %d", event.type);
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
    (void)x;
    (void)y;
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
    if (m_verbose && !m_quiet)
        LOGI("loading: %s", path.c_str());
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
        if (!m_quiet)
            LOGI("-> loaded %s: %d bytes", path.c_str(), size);
        *dest = data;
        return true;
    }
    else
    {
        LOGE("failed to open %s", path.c_str());
        return false;
    }
}

void CRuntime::preloadAssets()
{
    std::unique_ptr<CFrameSet> *frameSets[] = {
        &m_tiles,
        &m_animz,
        &m_users,
        &m_sheet0,
        &m_sheet1,
        &m_uisheet,
        &m_titlePix,
    };
    CFileMem mem;
    for (size_t i = 0; i < m_assetFiles.size(); ++i)
    {
        const std::string filename = AssetMan::getPrefix() + "pixels/" + m_assetFiles[i];
        *frameSets[i] = std::make_unique<CFrameSet>();
        data_t data = AssetMan::read(filename);
        if (!data.empty())
        {
            if (!m_quiet)
                LOGI("reading %s", filename.c_str());
            mem.replace(data.data(), data.size());
            if ((*frameSets[i])->extract(mem))
            {
                if (!m_quiet)
                    LOGI("extracted: %ld", ((*frameSets[i])->getSize()));
            }
        }
        else
        {
            LOGE("can't read: %s", filename.c_str());
        }
    }

    const std::string fontName = AssetMan::getPrefix() + m_config["font"];
    data_t data = AssetMan::read(fontName);
    if (data.empty())
        return;
    m_fontData = std::move(data);

    const std::string creditsFile = AssetMan::getPrefix() + m_config["credits"];
    data = AssetMan::read(creditsFile, true);
    if (!data.empty())
    {
        std::string str = reinterpret_cast<char *>(data.data());
        std::transform(str.begin(), str.end(), str.begin(), [](char c)
                       { return isspace(c) ? ' ' : c; });
        m_credits = str;
    }

    const std::string hintsFile = AssetMan::getPrefix() + m_config["hints"];
    data = AssetMan::read(hintsFile, true);
    if (!data.empty())
    {
        m_game->parseHints(reinterpret_cast<char *>(data.data()));
    }

    const std::string helpFile = AssetMan::getPrefix() + m_config["help"];
    data = AssetMan::read(helpFile, true);
    if (!data.empty())
    {
        parseHelp(reinterpret_cast<char *>(data.data()));
    }

    /////////////////////////////////////////////////////////////////////////////////
    // SDL3 hardware

    if (!g_font.loadRawFont("data/bitfont.bin"))
    {
        LOGE("cannot load font");
    }
    g_font.buildAtlas(m_app.renderer);

    m_textureTitlePix = createTexture(m_app.renderer, (*m_titlePix)[0]);
    if (!m_textureTitlePix)
    {
        LOGE("failed to create titlepix texture");
    }

    m_tileset_tiles.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[0], TILE_SIZE, TILE_SIZE);
    m_tileset_animz.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[1], TILE_SIZE, TILE_SIZE);
    m_tileset_users.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[2], TILE_SIZE, TILE_SIZE);
    m_tileset_scroll.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[5], 16, 48);

    const uint32_t colorFilter = fazFilter(FAZ_INV_BITSHIFT);
    m_tileset_users.cacheMask(m_app.renderer, COLOR_FADE, [colorFilter](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color = ((color >> FAZ_INV_BITSHIFT) & colorFilter) | ALPHA;
                                  //
                              });

    m_tileset_users.cacheMask(m_app.renderer, COLOR_INVERTED, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color ^= 0x00ffffff;
                                  //
                              });

    m_tileset_users.cacheMask(m_app.renderer, COLOR_GRAYSCALE, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                  {
                                      uint8_t *c = reinterpret_cast<uint8_t *>(&color);
                                      const uint16_t avg = (c[0] + c[1] + c[2]) / 3;
                                      c[0] = c[1] = c[2] = avg;
                                  } //
                              });

    m_tileset_users.cacheMask(m_app.renderer, COLOR_ALL_WHITE, [](uint32_t &color)
                              {
                                  if (color & ALPHA)
                                      color = WHITE;
                                  //
                              });

    m_tileset_sheet0.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[3], sheet0_data);
    m_tileset_sheet1.load(m_app.renderer, AssetMan::getPrefix() + "pixels/" + m_assetFiles[4], sheet1_data);

    preloadHearts();
}

void CRuntime::parseHelp(char *text)
{
    const auto lines = split(std::string(text), '\n');
    const char marker[] = {'_', '_', 'E', 'M', ' '};
    m_helptext.clear();
    for (const auto &line : lines)
    {
        if (memcmp(line.data(), marker, sizeof(marker)) == 0)
        {
#ifndef __EMSCRIPTEN__
            m_helptext.emplace_back(line.data() + sizeof(marker));
#endif
        }
        else
        {
            m_helptext.emplace_back(line.data());
        }
    }
}

void CRuntime::preRun()
{
    if (isTrue(m_config["test"]))
    {
        m_game->setMode(CGame::MODE_TEST);
    }
    else if (isTrue(m_config["clickstart"]))
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
    m_music = std::make_unique<CMusicSDL>(); //  new CMusicSDL();
    m_music->setVolume(ISound::MAX_VOLUME);
    if (!m_quiet)
        LOGI("volume: %d", m_music->getVolume());
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
    std::string path = m_workspace + HISCORE_FILE;
    if (!m_quiet)
        LOGI("reading %s", path.c_str());
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
            LOGE("hiscore size mismatch. resetting to default.");
            clearScores();
        }
        return true;
    }
    LOGE("can't read %s", path.c_str());
    return false;
}

bool CRuntime::saveScores()
{
    std::string path = m_workspace + HISCORE_FILE;
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
    LOGE("can't write %s", path.c_str());
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
    std::string path = m_workspace + SAVEGAME_FILE;
    if (m_game->mode() != CGame::MODE_PLAY)
    {
        LOGE("cannot save while not playing");
        return;
    }
    std::string name{"Testing123"};
    saveToFile(path, name);
}

void CRuntime::load()
{
    std::string path = m_workspace + SAVEGAME_FILE;
    CGame &game = *m_game;
    game.setMode(CGame::MODE_IDLE);
    std::string name;
    if (!loadFromFile(path, name))
    {
        // TODO: fix this later
        LOGI("failed loading: %s", path.c_str());
    }
    game.setMode(CGame::MODE_PLAY);
    openMusicForLevel(m_game->level());
    centerCamera();
    int userID = game.getUserID();
    loadColorMaps(userID);
}

void CRuntime::initSounds()
{
    if (!m_quiet)
        LOGI("initSound");
    m_sound = std::make_shared<CSndSDL>(); //  new CSndSDL();
    CFileWrap file;
    for (size_t i = 0; i < m_soundFiles.size(); ++i)
    {
        const auto soundName = AssetMan::getPrefix() + std::string("sounds/") + m_soundFiles[i];
        bool result = m_sound->add(soundName.c_str(), i + 1);
        if (m_verbose && !m_quiet)
            LOGI("%s %s", result ? "loaded" : "failed to load", soundName.c_str());
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
    if (!m_quiet)
        LOGI("open music: %s", filename.c_str());
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
                LOGE("missing section terminator on line %d", line);
            }
            section = p;
        }
        else if (p[0])
        {
            if (section == "musics")
            {
                m_musicFiles.emplace_back(p);
            }
            else if (section == "sounds")
            {
                m_soundFiles.emplace_back(p);
            }
            else if (section == "assets")
            {
                m_assetFiles.emplace_back(p);
            }
            else if (section == "users")
            {
                m_userNames.emplace_back(p);
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
                    LOGE("string %s on line %d split into %zu parts", p, line, list.size());
                }
            }
            else
            {
                LOGE("item for unknown section %s on line %d", section.c_str(), line);
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
        const int bx = baseX == -1 ? (getWidth() - text.size() * FONT_SIZE * scaleX) / 2 : baseX;
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
            uint8_t tmp[2];
            tmp[0] = CHARS_TRIGHT;
            tmp[1] = '\0';
            drawFont(bitmap, 32, y, (char *)tmp, RED, CLEAR, scaleX, scaleY);
        }
        if (item.type() == CMenuItem::ITEM_BAR)
        {
            const int scaleX = 1;
            size_t len = 0;
            for (size_t j = 0; j < item.size(); ++j)
                len += item.option(j).size() + (size_t)(j != 0);

            x = (getWidth() - len * FONT_SIZE * scaleX) / 2;
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
    if (m_titlePix == nullptr || m_titlePix->getSize() == 0)
        return;

    auto &title = *(*m_titlePix)[0];
    const int offsetY = 12;
    drawTitlePix(bitmap, offsetY);

    const int baseY = 2 * offsetY + title.height();
    const rect_t rect{
        8,
        baseY,
        getWidth() - 16,
        getHeight() - baseY - 24};
    CGameMixin::drawRect(bitmap, rect, DARKRED, false);

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
    auto &title = *(*m_titlePix)[0];
    int baseX = (bitmap.width() - title.width()) / 2;
    for (int y = 0; y < title.height(); ++y)
    {
        for (int x = 0; x < title.width(); ++x)
        {
            const int xx = baseX + x;
            if (xx < bitmap.width() && xx >= 0)
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
    drawFont(bitmap, 0, getHeight() - FONT_SIZE * 2, m_scroll, YELLOW);
    if (m_ticks & 1 && !m_credits.empty())
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
    m_game->resetStatsUponDeath();
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
    if (getWidth() > 450)
    {
        menu.setScaleX(4);
    }
    else if (getWidth() > 350)
    {
        menu.setScaleX(3);
    }
    else
    {
        menu.setScaleX(2);
    }

    if (getHeight() >= 450)
    {
        menu.setScaleY(6);
    }
    else if (getHeight() >= 350)
    {
        menu.setScaleY(5);
    }
    else if (getHeight() >= 300)
    {
        menu.setScaleY(3);
    }
    else
    {
        menu.setScaleY(2);
    }
    menu.addItem(CMenuItem("NEW GAME", MENU_ITEM_NEW_GAME));
    menu.addItem(CMenuItem("LOAD GAME", MENU_ITEM_LOAD_GAME))
        .disable(!isValidSavegame(getSavePath()));
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
    CFrame bitmap(getWidth(), getHeight());
    if (m_bitmap)
        bitmap.copy(m_bitmap);
    else
        bitmap.fill(BLACK);
    auto &rgba = bitmap.getRGB();
    for (int i = 0; i < bitmap.width() * bitmap.height(); ++i)
    {
        auto &color = rgba[i];
        if ((color >> 24) < 128)
        {
            rgba[i] = BLACK;
        }
    }
    bitmap.enlarge();
    std::vector<uint8_t> png;
    bitmap.toPng(png);
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
        file.write(png.data(), png.size());
        file.close();
        if (!m_quiet)
            LOGI("screenshot saved: %s", path.c_str());
    }
    else
    {
        LOGE("can't write png: %s", path.c_str());
    }
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
        if (!m_quiet)
            LOGI("is full screen?");
        toggleFullscreen();
    }

    if (m_config["viewport"] == "static")
    {
        m_cameraMode = CAMERA_MODE_STATIC;
        if (!m_quiet)
            LOGI("using viewport static");
    }
    else if (m_config["viewport"] == "dynamic")
    {
        m_cameraMode = CAMERA_MODE_DYNAMIC;
        if (!m_quiet)
            LOGI("using viewport dynamic");
    }
    else
    {
        if (!m_quiet)
            LOGI("using viewport default");
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
            LOGE("Fullscreen toggle error: %s", SDL_GetError());
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
        LOGI("Switching to windowed mode.");
        if (!SDL_SetWindowFullscreen(m_app.window, false)) // false means windowed mode
        {
            LOGE("Switching to windowed mode failed: %s", SDL_GetError());
        }

        // Restore original window size and position
        if (!SDL_SetWindowSize(m_app.window, m_app.windowedWidth, m_app.windowedHeigth))
        {
            LOGE("SDL_SetWindowSize failed: %s", SDL_GetError());
        }

        if (!SDL_SetWindowPosition(m_app.window, m_app.windowedX, m_app.windowedX))
        {
            LOGE("SDL_SetWindowPosition failed: %s", SDL_GetError());
        }
    }
    else
    {
        // Currently in windowed, switch to fullscreen
        LOGI("Switching to fullscreen desktop mode.");

        // Save current windowed position and size before going fullscreen
        SDL_GetWindowPosition(m_app.window, &m_app.windowedX, &m_app.windowedX);
        SDL_GetWindowSize(m_app.window, &m_app.windowedWidth, &m_app.windowedHeigth);

        if (!SDL_SetWindowFullscreen(m_app.window, true))
        {
            LOGE("Switching to fullscreen mode failed: %s", SDL_GetError());
        }
    }
#endif
    if (m_verbose)
    {
        int x, y, w, h;
        SDL_GetWindowPosition(m_app.window, &x, &y);
        SDL_GetWindowSize(m_app.window, &w, &h);
        LOGI("x:%d, y:%d, w:%d, h:%d", x, y, w, h);
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
    if (getWidth() > MIN_WIDTH_FULL)
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
        .disable(!isValidSavegame(getSavePath()));
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
    if (!m_quiet)
        LOGI("initControllers()");

    // Initialize SDL's video and game controller subsystems
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        LOGE("SDL could not initialize gamepad! SDL_Error: %s", SDL_GetError());
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
                m_gameControllers.emplace_back(controller);
                if (!m_quiet)
                    LOGI("Opened Game Controller: %d: %s", i, SDL_GetGamepadName(controller));
            }
            else
            {
                LOGE("Could not open game controller:%d! SDL_Error: %s", i, SDL_GetError());
            }
        }
    }
    if (m_gameControllers.empty())
    {
        LOGW("No game controllers found. Connect one now!");
    }

    const std::string controllerDB = AssetMan::getPrefix() + m_config["controllerdb"];
    if (!m_config["controllerdb"].empty() &&
        SDL_AddGamepadMappingsFromFile(controllerDB.c_str()) == -1)
    {
        LOGW("SDL_AddGamepadMappingsFromFile error: %s", SDL_GetError());
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
    return getWidth() / FONT_SIZE;
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
        resolutions.emplace_back(tmp);
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
        LOGE("No displays found: %s", SDL_GetError());
        return;
    }

    const int displayIndex = 0;
    int numModes;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display[displayIndex], &numModes);
    if (numModes < 1)
    {
        LOGE("SDL_GetFullscreenDisplayModes failed: %s", SDL_GetError());
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
            m_resolutions.emplace_back(Rez{mode.w, mode.h});
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
    const int w = getWidth() * 2;
    const int h = getHeight() * 2;
    int i = 0;
    for (const auto &rez : m_resolutions)
    {
        if (rez.h == h && rez.w == w)
        {
            if (!m_quiet)
                LOGI("found resolution %dx%d: %d", w, h, i);
            return i;
        }
        ++i;
    }
    if (!m_quiet)
        LOGI("new resolution %dx%d: %d", w, h, i);
    m_resolutions.emplace_back(Rez{w, h});
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
    if (m_verbose && !m_quiet)
        LOGI("switch to: %dx%d", w, h);
    setWidth(w / 2);
    setHeight(h / 2);
    SDL_SetWindowSize(m_app.window, w, h);
    auto oldTexture = m_app.texture;
    m_app.texture = SDL_CreateTexture(
        m_app.renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, getWidth(), getHeight());
    if (m_app.texture == nullptr)
    {
        LOGE("Failed to create texture: %s", SDL_GetError());
        return;
    }
    SDL_DestroyTexture(oldTexture);
    if (m_bitmap)
    {
        delete m_bitmap;
    }
    m_bitmap = new CFrame(getWidth(), getHeight());
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
        LOGE("No displays found: %s", SDL_GetError());
        return;
    }

    int numModes;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display[displayIndex], &numModes);
    if (numModes < 1)
    {
        LOGE("SDL_GetFullscreenDisplayModes failed:%s", SDL_GetError());
        return;
    }

    if (!m_quiet)
        LOGI("Available display modes:");
    for (int i = 0; i < numModes; ++i)
    {
        const SDL_DisplayMode &mode = *modes[i];
        if (!m_quiet)
            LOGI("Mode %d: %dx%d @ %fHz", i, mode.w, mode.h, mode.refresh_rate);
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
    int baseY = (getHeight() - m_userMenu->height()) / 2;
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
            LOGE("*** File not found: %s", music.c_str());
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
    data_t data = AssetMan::read(path, true);
    if (!data.empty())
    {
        parseColorMaps(reinterpret_cast<char *>(data.data()), m_colormaps);

        // update colodMasks
        m_tileset_users.cacheMask(m_app.renderer, COLOR_SUGARRUSH, [this](uint32_t &color)
                                  {
                                      auto &colorMap = m_colormaps.sugarRush;
                                      if (colorMap.count(color))
                                          color = colorMap[color];
                                      //
                                  });

        m_tileset_users.cacheMask(m_app.renderer, COLOR_GODMODE, [this](uint32_t &color)
                                  {
                                      auto &colorMap = m_colormaps.godMode;
                                      if (colorMap.count(color))
                                          color = colorMap[color];
                                      //
                                  });

        m_tileset_users.cacheMask(m_app.renderer, COLOR_GODMODE, [this](uint32_t &color)
                                  {
                                      auto &colorMap = m_colormaps.rage;
                                      if (colorMap.count(color))
                                          color = colorMap[color];
                                      //
                                  });
    }
    else
    {
        LOGE("can't read %s", path.c_str());
    }
}

/**
 * @brief Formely enter the game. This can be directly or after leaving ClickStart screen
 *
 */
void CRuntime::enterGame()
{
    if (!m_quiet)
        LOGI("enterGame()");
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
    if (getWidth() < MIN_WIDTH_FULL)
        listStr.emplace_back(Text{tmp, YELLOW, 1, 2});
    else
        listStr.emplace_back(Text{tmp, YELLOW, 2, 2});
    snprintf(tmp, sizeof(tmp), "Fruits Collected: %d %%", m_summary.ppFruits);
    listStr.emplace_back(Text{tmp, PINK, 1, 2});
    snprintf(tmp, sizeof(tmp), "Treasures Collected: %d %%", m_summary.ppBonuses);
    listStr.emplace_back(Text{tmp, ORANGE, 1, 2});
    snprintf(tmp, sizeof(tmp), "Secrets: %d %%", m_summary.ppSecrets);
    listStr.emplace_back(Text{tmp, GREEN, 1, 2});
    snprintf(tmp, sizeof(tmp), "Time Taken: %.2d:%.2d", m_summary.timeTaken / 60, m_summary.timeTaken % 60);
    listStr.emplace_back(Text{tmp, CYAN, 1, 2});

    if (m_countdown == 0 && ((m_ticks >> 3) & 1))
    {
        listStr.emplace_back(Text{"", BLACK, 1, 2});
        listStr.emplace_back(Text{"PRESS SPACE TO CONTINUE", LIGHTGRAY, 1, 2});
    }
    else
    {
        listStr.emplace_back(Text{"", BLACK, 1, 2});
        listStr.emplace_back(Text{"", BLACK, 1, 2});
    }

    int height = 0;
    for (const auto &item : listStr)
    {
        height += FONT_SIZE * item.scaleY + FONT_SIZE;
    }

    int y = (getHeight() - height) / 2;
    for (const auto &item : listStr)
    {
        const auto &str = item.text;
        const int x = (getWidth() - str.length() * FONT_SIZE * item.scaleX) / 2;
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
            if (m_trace && !m_quiet)
                LOGI("menuItem at: %d %d ==> %zu", x, y, i);
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
    const int baseY = (getHeight() - m_userMenu->height()) / 2;
    const char *t = "GAME DIFFICULTY";
    int x = (getWidth() - strlen(t) * FONT_SIZE * 2) / 2;
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
    return Rez{mode->w, mode->h};
}

Rez CRuntime::getWindowSize()
{
    int w;
    int h;
    if (!SDL_GetWindowSizeInPixels(m_app.window, &w, &h))
    {
        LOGE("SDL_GetWindowSizeInPixels failed ! SDL_Error: %s", SDL_GetError());
    }
    return Rez{w, h};
}

CRuntime::pos_t CRuntime::windowPos2texturePos(posF_t pos)
{
#if defined(__ANDROID__)
    rect_t safeRect = getSafeAreaWindow();
    pos.x -= safeRect.x;
    pos.y -= safeRect.y;
    float w = getWidth();
    float h = getHeight();
    return {.x = static_cast<int>(w * pos.x / safeRect.width), .y = static_cast<int>(h * pos.y / safeRect.height)};
#else
    return {
        static_cast<int>(pos.x) / PIXEL_SCALE,
        static_cast<int>(pos.y) / PIXEL_SCALE,
    };
#endif
}

void CRuntime::onOrientationChange()
{
    Rez rez = getScreenSize();
    bool isLandscape = rez.w > rez.h;
    if (!m_quiet)
        LOGI("Orientation changed - %s mode", isLandscape ? "Landscape" : "Portrait");
}

rect_t CRuntime::getSafeAreaWindow()
{
    SDL_Rect safe;
    if (!SDL_GetWindowSafeArea(m_app.window, &safe))
    {
        LOGE("SDL_GetWindowSafeArea() failed; error: %s", SDL_GetError());
    }
    return rect_t{
        safe.x,
        safe.y,
        safe.w,
        safe.h,
    };
}

rect_t CRuntime::windowRect2textureRect(const rect_t &wRect)
{
    Rez rez = getWindowSize();
    float w = (float)getWidth() * 2;
    // float h = (float)getHeight() * 2;
    auto _c = [rez, w](auto u)
    {
        return static_cast<int>(w * (float)u / rez.w);
    };
    return rect_t{
        _c(wRect.x),
        _c(wRect.y),
        _c(wRect.width),
        _c(wRect.height),
    };
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
            c,
            col * (BTN_SIZE + BTN_PADDING),
            row * (BTN_SIZE + BTN_PADDING),
            BTN_SIZE,
            BTN_SIZE,
            t,
            WHITE,
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
            id[i],
            x,
            y,
            width,
            BTN_SIZE,
            key,
            WHITE};
        x += width + BTN_PADDING;
        ui.addButton(button);
        ++i;
    }
    m_input = "";
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
    int x = (getWidth() - t.length() * FONT_SIZE * scaleX) / 2;
    drawFont(bitmap, x, FONT_SIZE, t.c_str(), LIGHTGRAY, CLEAR, scaleX, scaleY);
    t = buffer + end;
    x = (getWidth() - t.length() * FONT_SIZE * scaleX) / 2;
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
    if (!m_quiet)
        LOGI("SDL_QUIT");
    m_isRunning = false;
}

bool CRuntime::loadAppIcon()
{
    const std::string iconFile = AssetMan::getPrefix() + m_config["icon"];
    CFrameSet frames;
    data_t data = AssetMan::read(iconFile);
    if (!data.empty())
    {
        CFileMem mem;
        mem.replace(data.data(), data.size());
        if (frames.extract(mem) && frames.getSize() != 0)
        {
            CFrame &frame = *frames[0];
            SDL_Surface *surface = SDL_CreateSurfaceFrom(
                frame.width(), frame.height(), SDL_PIXELFORMAT_RGBA32, frame.getRGB().data(), frame.width() * sizeof(uint32_t));
            SDL_SetWindowIcon(m_app.window, surface);
            SDL_DestroySurface(surface);
        }
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
        LOGI("buttons: %.4x", buttonMask);
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
            //console.log(FS.readdir('/offline'));
            err ? console.log(err) : null; })

    );
}
#endif

bool CRuntime::saveToFile(const std::string filepath, const std::string name)
{
    if (!m_quiet)
        LOGI("writing: %s", filepath.c_str());
    CFileWrap tfile;
    if (tfile.open(filepath.c_str(), "wb"))
    {
        if (!write(tfile, name))
        {
            LOGE("failed to write file: %s", filepath.c_str());
            return false;
        }
        tfile.close();
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
        LOGE("can't write: %s", filepath.c_str());
        return false;
    }
    return true;
}

bool CRuntime::loadFromFile(const std::string filepath, std::string &name)
{
    if (!m_quiet)
        LOGI("reading: %s", filepath.c_str());
    CFileWrap sfile;
    if (sfile.open(filepath.c_str(), "rb"))
    {
        if (!read(sfile, name))
        {
            sfile.close();
            LOGE("incompatible file: %s", filepath.c_str());
            return false;
        }
        sfile.close();
    }
    else
    {
        LOGE("can't read: %s", filepath.c_str());
        return false;
    }
    return true;
}

bool CRuntime::isValidSavegame(const std::string &filepath)
{
    CFileWrap sfile;
    auto readfile = [&sfile](auto ptr, auto size) -> bool
    {
        return sfile.read(ptr, size) == IFILE_OK;
    };

    if (!fileExists(filepath))
    {
        return false;
    }

    if (sfile.open(filepath, "rb"))
    {
        char sig[4];
        uint32_t version;
        _R(sig, sizeof(sig));
        _R(&version, sizeof(version));
        sfile.close();
        if (!CGame::validateSignature(sig, version))
        {
            return false;
        }
        return true;
    }

    LOGE("couldn't read %s", filepath.c_str());
    return false;
}

void CRuntime::drawTest(CFrame &bitmap)
{
    drawFont(bitmap, 16, 16, "COMING SOON 2025", YELLOW, BLACK, 2, 2);
}

SDL_Texture *CRuntime::createTexture(SDL_Renderer *renderer, CFrame *frame)
{
    // --- Create a texture with SDL_PIXELFORMAT_ABGR8888 pixels ---
    SDL_PropertiesID texProps = SDL_CreateProperties();
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame->width());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame->height());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
    SDL_Texture *texture = SDL_CreateTextureWithProperties(renderer, texProps);
    SDL_DestroyProperties(texProps);

    if (!texture)
        return nullptr;

    int pitch = frame->width() * sizeof(uint32_t); // 4 bytes per pixel
    if (!SDL_UpdateTexture(texture, nullptr, frame->getRGB().data(), pitch))
    {
        std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << "\n";
    }
    return texture;
}

void CRuntime::drawPreScreen()
{
    const char t[] = "CLICK TO START";
    const int x = (getWidth() - strlen(t) * FONT_SIZE) / 2;
    const int y = (getHeight() - FONT_SIZE) / 2;
    g_font.drawText(m_app.renderer, t, SColor::WHITE, nullptr, x * SCALE, y * SCALE, SCALE, SCALE);
}

void CRuntime::drawLevelIntro()
{
    const int mode = m_game->mode();
    char t[32];
    switch (mode)
    {
    case CGame::MODE_LEVEL_INTRO:
        snprintf(t, sizeof(t), "LEVEL %.2d", m_game->level() + 1);
        break;
    case CGame::MODE_RESTART:
        if (m_game->lives() > 1)
        {
            snprintf(t, sizeof(t), "LIVES LEFT %.2d", m_game->lives());
        }
        else
        {
            strncpy(t, "LAST LIFE !", sizeof(t));
        }
        break;
    case CGame::MODE_GAMEOVER:
        strncpy(t, "GAME OVER", sizeof(t));
        break;
    case CGame::MODE_TIMEOUT:
        strncpy(t, "OUT OF TIME", sizeof(t));
        break;
    case CGame::MODE_CHUTE:
        snprintf(t, sizeof(t), "DROP TO LEVEL %.2d", m_game->level() + 1);
    };

    const int x = (getWidth() - strlen(t) * 2 * FONT_SIZE) / 2;
    const int y = (getHeight() - FONT_SIZE * 2) / 2;
    // drawFont(bitmap, x, y, t, YELLOW, BLACK, 2, 2);
    g_font.drawText(m_app.renderer, t, SColor::YELLOW, nullptr, x * SCALE, y * SCALE, SCALE_2X, SCALE_2X);

    if (mode == CGame::MODE_LEVEL_INTRO || mode == CGame::MODE_CHUTE)
    {
        const char *t = m_game->getMap().title();
        const int x = (getWidth() - strlen(t) * FONT_SIZE) / 2;
        // drawFont(bitmap, x, y + 3 * FONT_SIZE, t, WHITE);
        g_font.drawText(m_app.renderer, t, SColor::WHITE, nullptr, x * SCALE, (y + 3 * FONT_SIZE) * SCALE, SCALE, SCALE);
    }

    if (mode != CGame::MODE_GAMEOVER && getWidth() >= MIN_WIDTH_FULL)
    {
        const char *hint = m_game->getHintText();
        const int x = (getWidth() - strlen(hint) * FONT_SIZE) / 2;
        const int y = getHeight() - FONT_SIZE * 4;
        // drawFont(bitmap, x, y, hint, CYAN);
        g_font.drawText(m_app.renderer, hint, SColor::CYAN, nullptr, x * SCALE, y * SCALE, SCALE, SCALE);
    }
    m_currentEvent = EVENT_NONE;
    m_timer = TICK_RATE;
}

void CRuntime::drawScreen()
{
    const CGame &game = *m_game;

    // draw viewport
    if (m_cameraMode == CAMERA_MODE_DYNAMIC)
    {
        drawViewPortDynamic();
    }
    else if (m_cameraMode == CAMERA_MODE_STATIC)
    {
        drawViewPortStatic();
    }

    const int flash = game.statsConst().at(S_FLASH);
    if (flash)
        flashScreen();

    const int hurtStage = game.statsConst().at(S_PLAYER_HURT);
    const bool isPlayerHurt = hurtStage != CGame::HurtNone;
    //  if (isPlayerHurt)
    //      for (int i = 0; i < SCREEN_SHAKES; ++i)
    //          bitmap.shiftLEFT(false);

    // visual cues
    const visualCues_t visualcues{
        .diamondShimmer = game.goalCount() < m_visualStates.rGoalCount,
        .livesShimmer = game.lives() > m_visualStates.rLives,
    };
    m_visualStates.rGoalCount = game.goalCount();
    m_visualStates.rLives = game.lives();

    // draw game status
    drawGameStatus(visualcues);

    // draw bottom rect
    const bool isFullWidth = getWidth() >= MIN_WIDTH_FULL;

    if (m_currentEvent >= MSG0)
    {
        drawScroll();
    }
    else if (m_currentEvent == EVENT_SUGAR)
    {
        constexpr const float tileSize = TILE_SIZE;
        const SDL_Color bgColor = isFullWidth && m_currentEvent >= MSG0 ? SColor::WHITE : SColor::DARKGRAY;
        const SDL_Color borderColor = isPlayerHurt              ? SColor::PINK
                                      : visualcues.livesShimmer ? SColor::GREEN
                                                                : SColor::LIGHTGRAY;
        SDL_FRect rect{0, SCALEF * (getHeight() - tileSize), SCALEF * getWidth(), SCALEF * tileSize};
        drawRect(m_app.renderer, rect, bgColor, true);
        drawRect(m_app.renderer, rect, borderColor, false);
    }

    // draw current event text
    drawEventText();

    if (!isFullWidth || m_currentEvent < MSG0)
    {
        // draw Healthbar
        drawHealthBar(isPlayerHurt);

        // draw keys
        drawKeys();
    }

    // drawButtons
    // if (m_ui.isVisible())
    //    drawUI(bitmap, m_ui);

    // draw timeout
    drawTimeout();

    if (m_gameMenuActive)
    {
        fazeScreen();
        resizeGameMenu();
        drawMenu(*m_gameMenu, -1, (getHeight() - m_gameMenu->height()) / 2);
    }
}

void CRuntime::drawGameStatus(const visualCues_t &visualcues)
{
    CGame &game = *m_game;
    char tmp[32];

    std::string_view msg;
    if (m_paused)
    {
        msg = "PRESS [F4] TO RESUME PLAYING...";
    }
    else if (m_prompt == PROMPT_ERASE_SCORES)
    {
        msg = "ERASE HIGH SCORES, CONFIRM (Y/N)?";
    }
    else if (m_prompt == PROMPT_RESTART_GAME)
    {
        msg = "RESTART GAME, CONFIRM (Y/N)?";
    }
    else if (m_prompt == PROMPT_LOAD)
    {
        msg = "LOAD PREVIOUS SAVEGAME, CONFIRM (Y/N)?";
    }
    else if (m_prompt == PROMPT_SAVE)
    {
        msg = "SAVE GAME, CONFIRM (Y/N)?";
    }
    else if (m_prompt == PROMPT_HARDCORE)
    {
        msg = "HARDCORE MODE, CONFIRM (Y/N)?";
    }
    else if (m_prompt == PROMPT_TOGGLE_MUSIC)
    {
        msg = m_musicMuted ? "PLAY MUSIC, CONFIRM (Y/N)?"
                           : "MUTE MUSIC, CONFIRM (Y/N)?";
    }

    if (!msg.empty())
    {
        g_font.drawText(m_app.renderer, msg.data(), SColor::LIGHTGRAY, &SColor::BLACK, 0, Y_STATUS * SCALE, SCALE, SCALE);
    }
    else
    {
        int tx;
        int bx = 0;
        tx = snprintf(tmp, sizeof(tmp), "%.8d ", game.score());
        // drawFont(bitmap, 0, Y_STATUS, tmp, WHITE);
        g_font.drawText(m_app.renderer, tmp, SColor::WHITE, &SColor::BLACK, 0, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;
        tx = snprintf(tmp, sizeof(tmp), "DIAMONDS %.2d ", game.goalCount());
        // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, tmp, visualcues.diamondShimmer ? DEEPSKYBLUE : YELLOW);
        g_font.drawText(m_app.renderer, tmp, visualcues.diamondShimmer ? SColor::DEEPSKYBLUE : SColor::YELLOW, &SColor::BLACK, bx * FONT_SIZE * SCALE, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;
        tx = snprintf(tmp, sizeof(tmp), "LIVES %.2d ", game.lives());
        // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, tmp, visualcues.livesShimmer ? GREEN : PURPLE);
        g_font.drawText(m_app.renderer, tmp, visualcues.livesShimmer ? SColor::GREEN : SColor::PURPLE, &SColor::BLACK, bx * FONT_SIZE * SCALE, Y_STATUS * SCALE, SCALE, SCALE);
        bx += tx;
        if (m_recorder->isRecording())
        {
            // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, "REC!", WHITE, RED);
            g_font.drawText(
                m_app.renderer,
                "REC!",
                SColor::RED,
                &SColor::WHITE,
                bx * FONT_SIZE * SCALE,
                Y_STATUS * SCALE, SCALE, SCALE);
        }
        else if (m_recorder->isReading())
        {
            // drawFont(bitmap, bx * FONT_SIZE, Y_STATUS, "PLAY", WHITE, DARKGREEN);
            g_font.drawText(
                m_app.renderer,
                "PLAY",
                SColor::WHITE,
                &SColor::DARKGREEN,
                bx * FONT_SIZE * SCALE,
                Y_STATUS * SCALE, SCALE, SCALE);
        }
        if (getWidth() >= MIN_WIDTH_FULL)
            drawSugarMeter(bx);
    }
}

void CRuntime::drawSugarMeter(const int bx)
{
    constexpr const SDL_Color sugarColors[] = {
        SColor::RED,
        SColor::DEEPPINK,
        SColor::HOTPINK,
        SColor::ORANGE,
        SColor::PINK,
        SColor::BLACK,
        SColor::BLACK,
        SColor::YELLOW,
    };
    const bool updateNow = ((m_ticks >> 1) & 1);
    const int MAX_FX_COLOR = sizeof(sugarColors) / sizeof(sugarColors[0]) - 1;
    CGame &game = *m_game;
    const int sugar = game.sugar();
    if ((sugar > m_visualStates.rSugar || game.hasExtraSpeed()) &&
        !m_visualStates.sugarFx)
    {
        if (sugar > 0)
            m_visualStates.sugarCubes[sugar - 1] = MAX_FX_COLOR;
        m_visualStates.sugarFx = MAX_FX_COLOR;
    }
    else if (m_visualStates.sugarFx && updateNow)
    {
        --m_visualStates.sugarFx;
    }
    m_visualStates.rSugar = sugar;
    for (int i = 0; i < (int)CGame::MAX_SUGAR_RUSH_LEVEL; ++i)
    {
        const SDL_FRect rect{
            .x = SCALEF * bx * (int)FONT_SIZE + SCALEF * i * 5,
            .y = SCALEF * (Y_STATUS + 2),
            .w = SCALEF * 4,
            .h = SCALEF * 4};
        if (static_cast<int>(i) < sugar || game.hasExtraSpeed())
        {
            if (sugar < SUGAR_CUBES && !game.hasExtraSpeed())
            {
                const uint8_t &idx = m_visualStates.sugarCubes[i];
                drawRect(m_app.renderer, rect, sugarColors[idx], true);
            }
            else
            {
                drawRect(m_app.renderer, rect, sugarColors[m_visualStates.sugarFx], true);
            }
        }
        else
        {
            drawRect(m_app.renderer, rect, SColor::WHITE, false);
        }
    }

    // indicate sugar level
    const int x = bx * (int)FONT_SIZE;
    const int y = Y_STATUS + 2 + (int)FONT_SIZE;
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "Lvl %d", m_game->stats().get(S_SUGAR_LEVEL) + 1);
    // drawFont6x6(bitmap, x, y, tmp, WHITE, CLEAR);
    g_font.drawText(
        m_app.renderer,
        tmp,
        SColor::WHITE,
        nullptr,
        x * SCALE,
        y * SCALE);

    if (updateNow)
    {
        for (int i = 0; i < SUGAR_CUBES; ++i)
        {
            if (m_visualStates.sugarCubes[i])
                --m_visualStates.sugarCubes[i];
        }
    }
}

void CRuntime::drawRect(SDL_Renderer *renderer, const SDL_FRect &rect, const SDL_Color &color, const bool filled)
{
    // Save current draw color
    Uint8 oldR, oldG, oldB, oldA;
    SDL_GetRenderDrawColor(renderer, &oldR, &oldG, &oldB, &oldA);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (filled)
    {
        // filled  rectangle
        SDL_RenderFillRect(renderer, &rect);
    }
    else
    {
        // outlined rectangle
        SDL_RenderRect(renderer, &rect);
    }

    // Restore old draw color
    SDL_SetRenderDrawColor(renderer, oldR, oldG, oldB, oldA);
}

const Tile *CRuntime::tile2Frame(const uint8_t tileID)
{
    ColorMask colorMask = COLOR_NOCHANGE;
    const CGame &game = *m_game;
    const Tile *tile;
    if (tileID == TILES_STOP || tileID == TILES_BLANK || m_animator->isSpecialCase(tileID))
    {
        // skip blank tiles and special cases
        tile = nullptr;
    }
    else if (tileID == TILES_ANNIE2)
    {
        const int hurtStage = game.statsConst().at(S_PLAYER_HURT);
        if (hurtStage == CGame::HurtFlash)
            colorMask = COLOR_ALL_WHITE;
        else if (hurtStage == CGame::HurtInv)
            colorMask = COLOR_INVERTED;
        else if (hurtStage == CGame::HurtFaz)
            colorMask = COLOR_FADE;
        else
            colorMask = COLOR_NOCHANGE;

        if (m_game->isFrozen())
        {
            colorMask = COLOR_GRAYSCALE;
        }
        else if (m_game->hasExtraSpeed())
        {
            // colorMap = &m_colormaps.sugarRush;
            colorMask = COLOR_SUGARRUSH;
        }
        else if (m_game->isGodMode())
        {
            // colorMap = &m_colormaps.godMode;
            colorMask = COLOR_GODMODE;
        }
        else if (m_game->isRageMode())
        {
            // colorMap = &m_colormaps.rage;
            colorMask = COLOR_RAGE;
        }

        const uint8_t userID = game.getUserID();
        const uint32_t userBaseFrame = PLAYER_TOTAL_FRAMES * userID;
        const int aim = game.playerConst().getAim();
        if (!game.health())
        {
            // tile = annie[INDEX_PLAYER_DEAD * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(INDEX_PLAYER_DEAD * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame, colorMask);
        }
        else if (!game.goalCount() && game.isClosure())
        {
            // tile = annie[static_cast<uint8_t>(AIM_DOWN) * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(static_cast<uint8_t>(AIM_DOWN) * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame, colorMask);
        }
        else if (aim == AIM_DOWN && game.m_gameStats->get(S_IDLE_TIME) > IDLE_ACTIVATION)
        {
            const int idleTime = game.m_gameStats->get(S_IDLE_TIME);
            const int idleFrame = PLAYER_IDLE_BASE + ((idleTime >> 4) & 3);
            const int frame = idleTime & 0x08 ? idleFrame : static_cast<int>(PLAYER_DOWN_INDEX);
            // tile = annie[frame + userBaseFrame];
            tile = m_tileset_users.getTile(frame + userBaseFrame, colorMask);
        }
        else if (game.m_gameStats->get(S_BOAT) != 0 && game.playerConst().getPU() == TILES_SWAMP)
        {
            // tile = annie[PLAYER_BOAT_FRAME + userBaseFrame];
            tile = m_tileset_users.getTile(PLAYER_BOAT_FRAME + userBaseFrame, colorMask);
        }
        else
        {
            // tile = annie[aim * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame];
            tile = m_tileset_users.getTile(aim * PLAYER_FRAMES + m_playerFrameOffset + userBaseFrame, colorMask);
        }
    }
    else
    {
        const uint16_t j = m_animator->at(tileID);
        if (j == NO_ANIMZ)
        {
            // tile = tiles[tileID];
            tile = m_tileset_tiles.getTile(tileID);
        }
        else
        {
            // tile = animz[j];
            tile = m_tileset_animz.getTile(j);
        }
    }
    return tile;
}

void CRuntime::drawViewPortStatic()
{
    const CMap *map = &m_game->getMap();
    const CGame &game = *m_game;

    const int maxRows = getHeight() / TILE_SIZE;
    const int maxCols = getWidth() / TILE_SIZE;
    const int rows = std::min(maxRows, map->height());
    const int cols = std::min(maxCols, map->width());

    const int lmx = std::max(0, game.playerConst().x() - cols / 2);
    const int lmy = std::max(0, game.playerConst().y() - rows / 2);
    const int mx = std::min(lmx, map->width() > cols ? map->width() - cols : 0);
    const int my = std::min(lmy, map->height() > rows ? map->height() - rows : 0);

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            uint8_t tileID = map->at(x + mx, y + my);
            const Tile *tile = tile2Frame(tileID);
            if (tile)
                drawTile(tile, x * TILE_SIZE, y * TILE_SIZE);
        }
    }

    std::vector<sprite_t> sprites;
    gatherSprites(sprites, {.mx = mx, .ox = 0, .my = my, .oy = 0});

    // ColorMask colorMask = COLOR_NOCHANGE;
    for (const auto &sprite : sprites)
    {
        // special case animations
        const int x = sprite.x - mx;
        const int y = sprite.y - my;
        const Tile *tile = calcSpecialFrame(sprite);
        drawTile(tile, x * TILE_SIZE, y * TILE_SIZE); //, colorMask, nullptr);
    }

    // draw Bosses
    drawBossses(
        mx * CBoss::BOSS_GRANULAR_FACTOR,
        my * CBoss::BOSS_GRANULAR_FACTOR,
        maxCols * CBoss::BOSS_GRANULAR_FACTOR,
        maxRows * CBoss::BOSS_GRANULAR_FACTOR);
}

const Tile *CRuntime::calcSpecialFrame(const sprite_t &sprite)
{
    if (RANGE(sprite.attr, ATTR_IDLE_MIN, ATTR_IDLE_MAX))
    {
        // CFrameSet &tiles = *m_tiles;
        // return tiles[sprite.tileID];
        return m_tileset_tiles.getTile(sprite.tileID);
    }
    int saim = 0;
    if (sprite.tileID < TILES_TOTAL_COUNT)
    {
        // safeguard
        saim = sprite.aim;
        const TileDef &def = getTileDef(sprite.tileID);
        if (def.type == TYPE_DRONE)
        {
            saim &= 1;
        }
    }
    const animzInfo_t &info = m_animator->getSpecialInfo(sprite.tileID);
    return m_tileset_animz.getTile(saim * info.frames + info.base + info.offset);
}

void CRuntime::drawScroll()
{
    constexpr int SCROLL_LEFT = 0;
    constexpr int SCROLL_MID = 1;
    constexpr int SCROLL_RIGHT = 2;
    constexpr int scrollHeight = 48;
    constexpr int partWidth = 16;
    const int y = getHeight() - scrollHeight;

    drawTile(m_tileset_scroll.getTile(SCROLL_LEFT), 0, y);

    for (int x = partWidth; x < getWidth() - partWidth; x += partWidth)
        drawTile(m_tileset_scroll.getTile(SCROLL_MID), x, y);

    drawTile(m_tileset_scroll.getTile(SCROLL_RIGHT), (getWidth() - partWidth), y);
}

void CRuntime::drawEventText()
{
    if (m_currentEvent != EVENT_NONE)
    {
        const int baseY = getHeight() - 4;
        const message_t message = getEventText(baseY);
        const size_t maxLines = sizeof(message.lines) / sizeof(message.lines[0]);
        for (size_t i = 0; i < maxLines; ++i)
        {
            const auto &line = std::move(message.lines[i]);
            if (line.size() == 0)
                break;
            const int x = (getWidth() - line.size() * FONT_SIZE * message.scaleX) / 2;
            const int y = message.baseY - FONT_SIZE * message.scaleY + i * 10;
            g_font.drawText(m_app.renderer, line.c_str(), SColor::toSColor(message.color), nullptr, x * SCALE, y * SCALE, SCALE * message.scaleX, SCALE * message.scaleY);
        }
    }
}

void CRuntime::drawTimeout()
{
    const CMap *map = &m_game->getMap();
    const CStates &states = map->statesConst();
    const uint16_t timeout = states.getU(TIMEOUT);
    if (timeout)
    {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%.2d", timeout - 1);
        const bool lowTime = timeout <= 15;
        const int scaleX = !lowTime ? 3 : 5;
        const int scaleY = !lowTime ? 4 : 5;
        const int x = getWidth() - scaleX * FONT_SIZE * strlen(tmp) - FONT_SIZE;
        const int y = 2 * FONT_SIZE;
        const Color color = lowTime && (m_ticks >> 3) & 1 ? ORANGE : YELLOW;
        g_font.drawText(m_app.renderer, tmp, SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, SCALE * scaleX, SCALE * scaleY);
    }
}

void CRuntime::preloadHearts()
{
    LOGI("preloadHearts");

    CFrame *bitmap = new CFrame((FONT_SIZE + 1) * FONT_SIZE, 3 * FONT_SIZE);

    auto drawHeart = [&bitmap, this](auto bx, auto by, auto health, auto color)
    {
        //      const uint8_t *heart = getCustomChars() + (CHARS_HEART - CHARS_CUSTOM) * FONT_SIZE;
        const uint8_t *heart = m_fontData.data() + (CHARS_HEART - CHARS_CUSTOM + CHARS_CUSTOM_BASE) * FONT_SIZE;
        for (uint32_t y = 0; y < FONT_SIZE; ++y)
        {
            for (uint32_t x = 0; x < FONT_SIZE; ++x)
            {
                const uint8_t bit = heart[y] & (1 << x);
                if (bit)
                    bitmap->at(bx + x, by + y) = x < (uint32_t)health ? color : BLACK;
            }
        }
    };

    for (int i = 0; i <= FONT_SIZE; ++i)
    {
        drawHeart(i * FONT_SIZE, 0, i, RED);
        drawHeart(i * FONT_SIZE, FONT_SIZE, i, PINK);
        drawHeart(i * FONT_SIZE, FONT_SIZE * 2, i, WHITE);
    }

    m_tileset_hearts.load(m_app.renderer, bitmap, FONT_SIZE, FONT_SIZE);
}

void CRuntime::drawHealthBar(const bool isPlayerHurt)
{
    constexpr const int RED_HEARTS = 0;
    constexpr const int PINK_HEARTS = FONT_SIZE + 1;
    constexpr const int WHITE_HEARTS = PINK_HEARTS * 2;

    uint32_t baseID = RED_HEARTS;
    if (m_game->isGodMode())
    {
        baseID = WHITE_HEARTS;
    }
    else if (isPlayerHurt)
    {
        baseID = PINK_HEARTS;
    }

    // draw health bar
    CGame &game = *m_game;
    int step = FONT_SIZE;
    const int maxHealth = game.maxHealth() / 2 / FONT_SIZE;
    int health = game.health() / 2;
    int bx = 2;
    int by = getHeight() - 12;
    for (int i = 0; i < maxHealth; ++i)
    {
        auto hearts = health > 0 ? std::min(health, static_cast<int>(FONT_SIZE)) : 0;
        const Tile *tile = m_tileset_hearts.getTile(hearts + baseID);
        if (tile)
            drawTile(tile, bx, by);
        bx += FONT_SIZE;
        health -= step;
    }
}

void CRuntime::drawKeys()
{
    CGame &game = *m_game;
    const int y = getHeight() - TILE_SIZE;
    int x = getWidth() - TILE_SIZE;
    const CGame::userKeys_t &keys = game.keys();
    for (size_t i = 0; i < CGame::MAX_KEYS; ++i)
    {
        const uint8_t &k = keys.tiles[i];
        const uint8_t &u = keys.indicators[i];
        if (k)
        {
            // add visual sfx for key pickup
            const Tile *tile = m_tileset_tiles.getTile(k);
            SDL_Texture *texture = tile->texture;
            if (u == CGame::MAX_KEY_STATE)
            {
                // all white
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD); // multiply blend
                SDL_SetTextureColorMod(texture, 255, 255, 255);      // make RGB = 1.0
                SDL_SetTextureAlphaMod(texture, 255);                // keep alpha as is
                drawTile(tile, x, y);
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            }
            else if (u)
            {
                // fade in
                uint8_t mod = 2 << u;
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(texture, mod, mod, mod);
                // SDL_SetTextureAlphaMod(texture, mod);
                drawTile(tile, x, y);
                SDL_SetTextureColorMod(texture, 255, 255, 255);
                //   SDL_SetTextureAlphaMod(texture, 255);
            }
            else
            {
                // normal
                drawTile(tile, x, y);
            }
            x -= TILE_SIZE;
        }
    }
    if ((m_ticks >> 1) & 1)
        game.decKeyIndicators();
}

void CRuntime::drawViewPortDynamic()
{
    const CMap *map = &m_game->getMap();
    const int maxRows = getHeight() / TILE_SIZE;
    const int maxCols = getWidth() / TILE_SIZE;
    const int rows = std::min(maxRows, map->height());
    const int cols = std::min(maxCols, map->width());
    const int mx = m_cx / 2;
    const int ox = m_cx & 1;
    const int my = m_cy / 2;
    const int oy = m_cy & 1;
    const int halfOffset = TILE_SIZE / 2;

    int py = oy ? -halfOffset : 0;
    for (int y = 0; y < rows + oy; ++y)
    {
        int px = ox ? -halfOffset : 0;
        for (int x = 0; x < cols + ox; ++x)
        {
            const uint8_t tileID = map->at(x + mx, y + my);
            const Tile *tile = tile2Frame(tileID);
            if (tile)
            {
                drawTile(tile, px, py);
            }
            px += TILE_SIZE;
        }
        py += TILE_SIZE;
    }

    /////////////////////////////////////////////////////////////////////////////
    // overlay special case monsters and sfx

    std::vector<sprite_t> sprites;
    gatherSprites(sprites, {.mx = mx, .ox = ox, .my = my, .oy = oy});
    for (const auto &sprite : sprites)
    {
        // special case animations
        const int x = sprite.x - mx;
        const int y = sprite.y - my;
        const Tile *tile = calcSpecialFrame(sprite);
        int px = x * TILE_SIZE;
        int py = y * TILE_SIZE;
        if (x && ox)
            px -= halfOffset;
        if (y && oy)
            py -= halfOffset;
        drawTile(tile, px, py);
    }

    // draw Bosses
    drawBossses(m_cx, m_cy, maxCols * CBoss::BOSS_GRANULAR_FACTOR, maxRows * CBoss::BOSS_GRANULAR_FACTOR);
}

void CRuntime::drawBossses(const int mx, const int my, const int sx, const int sy)
{
    auto between = [](int a1, int a2, int b1, int b2)
    {
        return a1 < b2 && a2 > b1;
    };

    auto betweenRect = [&between](const rect_t &bRect, const rect_t &sRect)
    {
        return between(bRect.x, bRect.x + bRect.width, sRect.x, sRect.x + sRect.width) &&
               between(bRect.y, bRect.y + bRect.height, sRect.y, sRect.y + sRect.height);
    };

    auto printRect = [](const rect_t &rect, const std::string_view &name)
    {
        LOGI("%s (%d, %d) w:%d h: %d", name.data(), rect.x, rect.y, rect.width, rect.height);
    };

    (void)printRect;

    constexpr int MAX_HP_GAUGE = 64;
    constexpr int GRID_SIZE = 8;
    constexpr int HP_BAR_HEIGHT = 8;
    constexpr int HP_BAR_SPACING = 2;

    // bosses are drawn on a 8x8 grid overlayed on top of the regular 16x16 grid
    for (const auto &boss : m_game->bosses())
    {
        // don't process completed bosses
        if (boss.isDone())
            continue;

        TileSet *tileset = nullptr;
        if (boss.data()->sheet == 0)
        {
            tileset = &m_tileset_sheet0;
        }
        else if (boss.data()->sheet == 1)
        {
            tileset = &m_tileset_sheet1;
        }
        else
        {
            LOGE("invalid sheet: %d", boss.data()->sheet);
            continue;
        }

        const int num = boss.currentFrame();
        const Tile *tile = tileset->getTile(num);
        if (!tile)
            continue;
        const hitbox_t &hitbox = boss.hitbox();

        // Logical coordonates comverted to screen positions
        // (using GRID_SIZE)

        // Boss Rect
        const rect_t bRect{
            GRID_SIZE * (boss.x() - hitbox.x),
            GRID_SIZE * (boss.y() - hitbox.y),
            (int)tile->rect.w,
            (int)tile->rect.h};

        // Screen Rect
        const rect_t sRect{
            .x = GRID_SIZE * mx,
            .y = GRID_SIZE * my,
            .width = GRID_SIZE * sx,
            .height = GRID_SIZE * sy,
        };

        if (betweenRect(bRect, sRect))
        {
            const int x = bRect.x - sRect.x;
            const int y = bRect.y - sRect.y;
            // draw boss
            drawTile(tile, x, y);
        }

        // skip drawing the healthbar and name
        if (!boss.data()->show_details)
            continue;

        // Hp Rect
        const float hpRatio = (float)boss.maxHp() / MAX_HP_GAUGE;
        const rect_t hRect{
            .x = bRect.x,
            .y = bRect.y - HP_BAR_HEIGHT - HP_BAR_SPACING,
            .width = MAX_HP_GAUGE, // boss.maxHp(),
            .height = HP_BAR_HEIGHT,
        };
        if (betweenRect(hRect, sRect))
        {
            const float x = hRect.x - sRect.x;
            const float y = hRect.y - sRect.y;
            SDL_FRect rect{x * SCALEF, y * SCALEF, hRect.width * SCALEF, hRect.height * SCALEF};
            SDL_FRect rectH{x * SCALEF, y * SCALEF, static_cast<int>(boss.hp() / hpRatio) * SCALEF, hRect.height * SCALEF};
            drawRect(m_app.renderer, rect, SColor::BLACK, true);
            drawRect(m_app.renderer, rectH, SColor::ORANGE, true);
            drawRect(m_app.renderer, rect, SColor::WHITE, false);
        }

        // draw Boss Name
        const int x = hRect.x - sRect.x;
        const int y = hRect.y - sRect.y - FONT_SIZE;
        g_font.drawText(m_app.renderer, boss.name(), SColor::ORANGE, nullptr, x * SCALE, y * SCALE, 1, 1);
    }
}

void CRuntime::fazeScreen()
{
    // save default
    SDL_BlendMode oldMode = 0;
    SDL_GetRenderDrawBlendMode(m_app.renderer, &oldMode);

    SDL_SetRenderDrawBlendMode(m_app.renderer, SDL_BLENDMODE_BLEND);
    SDL_Color color{0, 0, 0, 0xe0};
    SDL_FRect rect{0, 0, getWidth() * SCALEF, getHeight() * SCALEF};
    drawRect(m_app.renderer, rect, color, true);

    // restore
    SDL_SetRenderDrawBlendMode(m_app.renderer, oldMode);
}

void CRuntime::flashScreen()
{
    // save default
    SDL_BlendMode oldMode;
    SDL_GetRenderDrawBlendMode(m_app.renderer, &oldMode);

    SDL_SetRenderDrawBlendMode(m_app.renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(m_app.renderer, 255, 255, 255, 255);

    SDL_FRect rect{0, 0, getWidth() * SCALEF, getHeight() * SCALEF};
    SDL_RenderFillRect(m_app.renderer, &rect);

    // restore
    SDL_SetRenderDrawBlendMode(m_app.renderer, oldMode);
}

/**
 * @brief Draw Menu at coordinates baseX, baseY
 *
 * @param bitmap
 * @param menu
 * @param baseX
 * @param baseY
 */
void CRuntime::drawMenu(CMenu &menu, const int baseX, const int baseY)
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
        const int bx = baseX == -1 ? (getWidth() - text.size() * FONT_SIZE * scaleX) / 2 : baseX;
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
            uint8_t tmp[2];
            tmp[0] = CHARS_TRIGHT - CHARS_CUSTOM + CHARS_CUSTOM_BASE + ' ';
            tmp[1] = '\0';
            g_font.drawText(m_app.renderer, (char *)tmp, SColor::RED, nullptr, 32 * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
            // drawFont(bitmap, 32, y, (char *)tmp, RED, CLEAR, scaleX, scaleY);
        }
        if (item.type() == CMenuItem::ITEM_BAR)
        {
            const int scaleX = 1;
            size_t len = 0;
            for (size_t j = 0; j < item.size(); ++j)
                len += item.option(j).size() + (size_t)(j != 0);

            x = (getWidth() - len * FONT_SIZE * scaleX) / 2;
            for (size_t j = 0; j < item.size(); ++j)
            {
                const Color color = selected && (j == (size_t)item.value()) ? YELLOW : BLUE;
                const std::string option = item.option(j);
                //                drawFont(bitmap, x, y, option.c_str(), BLACK, color, scaleX, scaleY);
                g_font.drawText(m_app.renderer, option.c_str(), SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
                x += (FONT_SIZE + 1) * option.size() * scaleX;
            }
            continue;
        }
        else if (item.role() == MENU_ITEM_SELECT_USER)
        {
            const uint16_t animeOffset = selected ? (m_ticks / 3) & 0x1f : 0;
            // const CFrameSet &users = *m_users;
            // CFrame &frame = *users[PLAYER_TOTAL_FRAMES * item.userData() + PLAYER_DOWN_INDEX + animeOffset];
            // drawTileFaz(bitmap, x, y, frame, 0, selected ? COLOR_NOCHANGE : COLOR_GRAYSCALE);
            const Tile *tile = m_tileset_users.getTile(PLAYER_TOTAL_FRAMES * item.userData() + PLAYER_DOWN_INDEX + animeOffset,
                                                       selected ? COLOR_NOCHANGE : COLOR_GRAYSCALE);
            drawTile(tile, x, y);
            x += 32;
        }
        // drawFont(bitmap, x, y, text.c_str(), color, CLEAR, scaleX, scaleY);
        g_font.drawText(m_app.renderer, text.c_str(), SColor::toSColor(color), nullptr, x * SCALE, y * SCALE, scaleX * SCALE, scaleY * SCALE);
    }
}