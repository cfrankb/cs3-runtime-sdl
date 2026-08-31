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
#if !defined(__ANDROID__) && !defined(SDL_MAIN_HANDLED)
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL_main.h>
// #include <unistd.h>
#if defined(_WIN32) && !defined(__MINGW32__)
// For MSVC/Clang-MSVC
#include <io.h>
#include <process.h>
// If you were using sleep(), use Windows.h or SDL_Delay()
#include <windows.h>
#else
// For Linux/MinGW
#include <unistd.h>
#endif
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include "runtime.h"
#include "maparch.h"
#include "parseargs.h"
#include "game.h"
#include "assetman.h"
#include "logger.h"
#include "build.h"
#include "statedata.h"
#include "states.h"
#include "strhelper.h"
#ifdef STEAM_BUILD
#include "steam_helper.h"
#endif

const uint32_t FPS = CRuntime::tickRate();
const uint32_t SLEEP = 1000 / FPS;
uint32_t g_lastTick = 0;
bool g_skip = false;
uint32_t g_sleepDelay = SLEEP;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
constexpr const char *DEFAULT_PREFIX = "data/";
constexpr const char *DEFAULT_MAPARCH = "levels.mapz";
constexpr const char *CONF_FILE = "game.cfg";
constexpr const char *WINDOWS_GAME_ROAMPATH = "\\cs3-runtime";

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

CRuntime *g_runtime = nullptr;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

EM_BOOL on_fullscreen_change(int eventType, const EmscriptenFullscreenChangeEvent *e, void *userData)
{
    if (!e->isFullscreen)
    {
        LOGI("Exited fullscreen—likely via ESC");
        // Trigger your custom logic here
        g_runtime->notifyExitFullScreen();
    }
    return EM_TRUE;
}
#endif

void stripMaps(CMapArch &arch)
{
    int mapRemoved = 0;
    for (int i = static_cast<int>(arch.size()) - 1; i >= 0; --i)
    {
        CMap *map = arch.at(i);
        CStates &states = map->states();
        if (states.getU(PRIVATE) != 0)
        {
            ++mapRemoved;
            LOGI("stripping out: level %d - %s", i + 1, map->title());
            arch.removeAt(i);
        }
    }
    if (mapRemoved)
    {
        LOGI("removed %d map%s", mapRemoved, mapRemoved > 1 ? "s" : "");
        LOGI("%lu maps remaining.", arch.size());
    }
}

void loop_handler(void *)
{
    uint32_t currTick = SDL_GetTicks();
    uint32_t meantime = currTick - g_lastTick;
    if (meantime >= g_sleepDelay)
    {
        g_runtime->doInput();
        g_runtime->run();
        uint32_t btime = SDL_GetTicks();
        if (!g_skip)
        {
            g_runtime->paint();
        }
        uint32_t atime = SDL_GetTicks();
        uint32_t ptime = atime - btime;
        if (ptime < SLEEP)
        {
            g_sleepDelay = SLEEP - ptime;
            g_skip = false;
        }
        else
        {
            g_sleepDelay = SLEEP;
            g_skip = true;
        }
        g_lastTick = currTick;
    }
}

const std::string getPrefix()
{
#if defined(IS_MACOS)
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
#elif defined(__EMSCRIPTEN__)
    return DEFAULT_PREFIX;
#else
    char *appdir_env = std::getenv("APPDIR");
    if (appdir_env)
    {
        LOGI("APPDIR environment variable found: %s", appdir_env);
        // Construct the full path to your embedded data (e.g., an image)
        return std::string(appdir_env) + "/usr/share/data/";
    }
    else
    {
        LOGI("APPDIR environment variable not found.");
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

#if defined(__MINGW32__) || defined(_MSC_VER)
#if defined(__MINGW32__)
#pragma message "Compiling with MinGW"
#elif defined(_MSC_VER)
#pragma message "Compiling with MSVC"
#else
#pragma message "Compiling unknown"
#endif
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <vector>
#include <filesystem> // Requires C++17
namespace fs = std::filesystem;

std::string GetAppDataPath()
{
    std::string result = "";

    // Check if we are using a modern compiler/SDK that supports the new API
    // MinGW usually needs _WIN32_WINNT >= 0x0600 for SHGetKnownFolderPath

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path)))
    {
        // Convert Wide string to UTF-8/string (simplified)
        std::wstring ws(path);
        result = std::string(ws.begin(), ws.end());
        CoTaskMemFree(path);
    }
#else
    // Fallback for MinGW or older Windows targets
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        result = std::string(path);
    }
#endif

    return result;
}

bool makePath(const std::string &path)
{
    // std::string path = "C:\\Users\\YourUser\\AppData\\Roaming\\MyAppData";
    try
    {
        // create_directories creates the entire path (parents too) if they don't exist
        if (fs::create_directories(path))
        {
            std::cout << "Directory created successfully\n";
        }
        else
        {
            std::cout << "Directory already exists or could not be created\n";
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    std::vector<std::string> list;
    for (int i = 1; i < __argc; i++)
        list.emplace_back(__argv[i]);

    for (size_t i = 0; i < list.size(); ++i)
    {
        LOGI("%d>>> %s", i, list[i].c_str());
    }
#elif !defined(EMSCRIPTEN)
int main(int argc, char *args[])
{
    std::vector<std::string> list;
    if (argc)
        list.reserve(argc - 1);
    for (int i = 1; i < argc; ++i)
    {
        list.emplace_back(args[i]);
    }
#else
int main(int argc, char *args[])
{
    (void)argc;
    (void)args;
    std::vector<std::string> list;
#endif

    LOGI("Starting Game %s: Build [%s]", VERSION, BUILD_HASH);

#if STEAM_BUILD
    bool skipSteam = true;

    LOGI("running special steam build.");
    if (!skipSteam && !InitSteam())
    {
        LOGE("failed to init steam");
        return EXIT_FAILURE;
    }
#endif

    srand(static_cast<unsigned int>(time(nullptr)));
    CMapArch maparch;
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
#elif defined(__MINGW32__) || defined(_MSC_VER)
    params.workspace = GetAppDataPath() + WINDOWS_GAME_ROAMPATH;
    if (!makePath(params.workspace))
    {
        LOGE("failed to create appDataRoamPath: %s", params.workspace.c_str());
    }
    params.workspace += "\\";
#elif defined(__EMSCRIPTEN__)
    params.workspace = "/offline/";
#endif
    LOGI("workspace: %s", params.workspace.c_str());

    bool appExit = false;
    if (!parseArgs(list, params, appExit))
        return EXIT_FAILURE;
    else if (appExit)
        return EXIT_SUCCESS;

    LOGI("prefix: %s", params.prefix.c_str());

    LOGI("MapArch: %s", params.mapArch.c_str());
    data_t data = AssetMan::read(params.mapArch);
    if (data.empty())
        return EXIT_FAILURE;
    if (!maparch.fromMemory(data.data()))
    {
        LOGE("mapArch error: %s", maparch.lastError());
        return EXIT_FAILURE;
    }
    if (params.strip_private)
        stripMaps(maparch);

    std::string configFile = AssetMan::addTrailSlash(params.prefix) + CONF_FILE;
    std::unique_ptr<CRuntime> runtime = std::make_unique<CRuntime>();
    g_runtime = runtime.get();
    g_runtime->setVerbose(params.verbose);
    AssetMan::setPrefix(params.prefix);
    g_runtime->setWorkspace(params.workspace.c_str());

    data = AssetMan::read(configFile, true);
    if (data.empty())
        return EXIT_FAILURE;
    if (!g_runtime->parseConfig(data.data()))
    {
        LOGE("failed to parse config file: %s", configFile.c_str());
        return EXIT_FAILURE;
    }
    data.clear();

    g_runtime->setSkill(params.skill);
    const int startLevel = (params.level > 0 ? params.level - 1 : 0) % maparch.size();
    g_runtime->setStartLevel(startLevel);
    if (params.fullscreen)
    {
        g_runtime->setConfig("fullscreen", "true");
    }
    if (params.muteMusic)
    {
        // override options
        g_runtime->enableMusic(false);
    }
    if (!g_runtime->initSDL())
        return EXIT_FAILURE;

    g_runtime->setWidth(params.width);
    g_runtime->setHeight(params.height);
    if (!g_runtime->createSDLWindow())
        return EXIT_FAILURE;
    g_runtime->initEngine();
    g_runtime->init(&maparch, startLevel);
    g_runtime->debugSDL();
    g_runtime->initOptions();
    g_runtime->preRun();
    g_runtime->paint();

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    g_runtime->checkMusicFiles();
#endif
#ifdef __EMSCRIPTEN__
    // emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, on_fullscreen_change);
    emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_TRUE, on_fullscreen_change);
    emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);
#else
    while (g_runtime->isRunning())
    {
        loop_handler(nullptr);
    }
#endif
    CGame::destroy();
#ifdef STEAM_BUILD
    ShutdownSteam();
#endif
    return EXIT_SUCCESS;
}
