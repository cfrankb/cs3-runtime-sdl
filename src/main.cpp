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
#define LOG_TAG "main"
#ifndef __ANDROID__
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#else
#endif
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include "runtime.h"
#include "maparch.h"
#include "parseargs.h"
#include "game.h"
#include "strhelper.h"
#include "assetman.h"
#include "logger.h"

const uint32_t FPS = CRuntime::tickRate();
const uint32_t SLEEP = 1000 / FPS;
uint32_t lastTick = 0;
bool skip = false;
uint32_t sleepDelay = SLEEP;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define DEFAULT_PREFIX "data/"
#define DEFAULT_MAPARCH "levels.mapz"
#define CONF_FILE "game.cfg"

// Platform detection
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC && !TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#define IS_MACOS 1
#define PLATFORM_NAME "macOS"
#else
#define PLATFORM_NAME "Other Apple OS"
#endif
#else
#define PLATFORM_NAME "Non-Apple OS"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
CRuntime *g_runtime = nullptr;

EM_BOOL on_fullscreen_change(int eventType, const EmscriptenFullscreenChangeEvent *e, void *userData)
{
    if (!e->isFullscreen)
    {
        LOGI("Exited fullscreen—likely via ESC\n");
        // Trigger your custom logic here
        g_runtime->notifyExitFullScreen();
    }
    return EM_TRUE;
}
#endif

void loop_handler(void *arg)
{
    uint32_t currTick = SDL_GetTicks();
    uint32_t meantime = currTick - lastTick;
    if (meantime >= sleepDelay)
    {
        CRuntime *runtime = reinterpret_cast<CRuntime *>(arg);
        runtime->doInput();
        runtime->run();
        uint32_t btime = SDL_GetTicks();
        if (!skip)
        {
            runtime->paint();
        }
        uint32_t atime = SDL_GetTicks();
        uint32_t ptime = atime - btime;
        if (ptime < SLEEP)
        {
            sleepDelay = SLEEP - ptime;
            skip = false;
        }
        else
        {
            sleepDelay = SLEEP;
            skip = true;
        }
        lastTick = currTick;
    }
}

const std::string getPrefix()
{
#ifdef IS_MACOS
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef resURL = CFBundleCopyResourcesDirectoryURL(bundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resURL, true, (UInt8 *)path, PATH_MAX))
    {
        return std::string(path) + "/";
    }
    return "";
#elif defined(__ANDROID__)
    return "";
#else
    char *appdir_env = std::getenv("APPDIR");
    if (appdir_env)
    {
        LOGI("APPDIR environment variable found: %s\n", appdir_env);
        // Construct the full path to your embedded data (e.g., an image)
        return std::string(appdir_env) + "/usr/share/data/";
    }
    else
    {
        LOGI("APPDIR environment variable not found.\n");
        // Fallback or error handling: If not running as AppImage,
        const std::vector<std::string> paths = {
            "data",
            "/usr/local/share/cs3-runtime",
            "/usr/share/cs3-runtime",
        };
        for (const auto &path : paths)
        {
            if (std::filesystem::is_directory(path))
            {
                return path + "/";
            }
        }
        return DEFAULT_PREFIX;
    }
#endif
}

#if defined(__MINGW32__)
#pragma message "Compiling with MinGW"
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    std::vector<std::string> list;
    if (lpCmdLine[0])
        splitString2(lpCmdLine, list);
    LOGI("CMD: %s\n", lpCmdLine);
    for (size_t i = 0; i < list.size(); ++i)
    {
        LOGI("%d>>> %s\n", i, list[i].c_str());
    }
#else
int main(int argc, char *args[])
{
    std::vector<std::string> list;
    for (int i = 1; i < argc; ++i)
    {
        list.push_back(args[i]);
    }
#endif

    LOGI("Starting Game\n");
    srand(static_cast<unsigned int>(time(nullptr)));
    CMapArch maparch;
    CRuntime runtime;
    params_t params;
    params.muteMusic = false;
    params.level = 0;
    params.prefix = getPrefix();
    params.mapArch = params.prefix + DEFAULT_MAPARCH;

#if defined(__ANDROID__)
    const char *path = SDL_GetAndroidInternalStoragePath();
    if (!path)
    {
        LOGE("Failed to get internal storage path: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    params.workspace = path;
    LOGI("workspace: %s\n", params.workspace.c_str());
#endif

    bool appExit = false;
    if (!parseArgs(list, params, appExit))
        return EXIT_FAILURE;
    if (appExit)
        return EXIT_SUCCESS;

    LOGI("MapArch: %s\n", params.mapArch.c_str());
    data_t data;
    if (!CAssetMan::read(params.mapArch, data))
        return EXIT_FAILURE;
    if (!maparch.fromMemory(data.ptr))
    {
        CAssetMan::free(data);
        LOGE("mapArch error: %s\n", maparch.lastError());
        return EXIT_FAILURE;
    }
    CAssetMan::free(data);
    std::string configFile = CAssetMan::addTrailSlash(params.prefix) + CONF_FILE;
    runtime.setVerbose(params.verbose);
    CAssetMan::setPrefix(params.prefix);
    runtime.setWorkspace(params.workspace.c_str());

    if (!CAssetMan::read(configFile, data, true))
        return EXIT_FAILURE;
    if (!runtime.parseConfig(data.ptr))
    {
        CAssetMan::free(data);
        LOGE("failed to parse config file: %s\n", configFile.c_str());
        return EXIT_FAILURE;
    }
    CAssetMan::free(data);

    runtime.setSkill(params.skill);
    const int startLevel = (params.level > 0 ? params.level - 1 : 0) % maparch.size();
    runtime.init(&maparch, startLevel);
    runtime.setStartLevel(startLevel);
    if (params.fullscreen)
    {
        runtime.setConfig("fullscreen", "true");
    }
    if (params.muteMusic)
    {
        // override options
        runtime.enableMusic(false);
    }
    if (!runtime.initSDL())
        return EXIT_FAILURE;

    runtime.setWidth(params.width);
    runtime.setHeight(params.height);
    if (!runtime.createSDLWindow())
        return EXIT_FAILURE;
    runtime.debugSDL();
    runtime.initOptions();
    runtime.preRun();
    runtime.paint();

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    runtime.checkMusicFiles();
#endif
#ifdef __EMSCRIPTEN__
    g_runtime = &runtime;
    emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, on_fullscreen_change);
    emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);
#else
    while (runtime.isRunning())
    {
        loop_handler(&runtime);
    }
#endif
    CGame::destroy();
    return EXIT_SUCCESS;
}
