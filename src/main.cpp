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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
CRuntime *g_runtime = nullptr;
extern "C"
{
    int savegame(int x)
    {
        /// printf("savegame: %d\n", x);
        if (x == 0)
        {
            g_runtime->load();
        }
        else
        {
            g_runtime->save();
        }
        return 0;
    }

    int mute(int x)
    {
        if (x == 0)
        {
            g_runtime->stopMusic();
        }
        else
        {
            g_runtime->startMusic();
        }
        return 0;
    }
}

EM_BOOL on_fullscreen_change(int eventType, const EmscriptenFullscreenChangeEvent *e, void *userData)
{
    if (!e->isFullscreen)
    {
        printf("Exited fullscreen—likely via ESC\n");
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
    char *appdir_env = std::getenv("APPDIR");
    if (appdir_env)
    {
        printf("APPDIR environment variable found: %s\n", appdir_env);
        // Construct the full path to your embedded data (e.g., an image)
        return std::string(appdir_env) + "/usr/share/data/";
    }
    else
    {
        printf("APPDIR environment variable not found.\n");
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
}

int main(int argc, char *args[])
{
    srand(static_cast<unsigned int>(time(NULL)));
    CMapArch maparch;
    CRuntime runtime;
    params_t params;
    params.muteMusic = false;
    params.level = 0;
    params.prefix = getPrefix();
    params.mapArch = params.prefix + DEFAULT_MAPARCH;
    bool appExit = false;
    if (!parseArgs(argc, args, params, appExit))
        return EXIT_FAILURE;
    if (appExit)
        return EXIT_SUCCESS;
    if (!maparch.read(params.mapArch.c_str()))
    {
        fprintf(stderr, "failed to read maparch: %s %s\n", params.mapArch.c_str(), maparch.lastError());
        return EXIT_FAILURE;
    }
    std::string configFile = CRuntime::addTrailSlash(params.prefix) + CONF_FILE;
    runtime.setVerbose(params.verbose);
    runtime.setPrefix(params.prefix.c_str());
    runtime.setWorkspace(params.workspace.c_str());
    runtime.setWidth(params.width);
    runtime.setHeight(params.height);
    if (!runtime.parseConfig(configFile.c_str()))
    {
        printf("failed to parse config file: %s\n", configFile.c_str());
        return EXIT_FAILURE;
    }
    runtime.setSkill(params.skill);
    const int startLevel = (params.level > 0 ? params.level - 1 : 0) % maparch.size();
    runtime.init(&maparch, startLevel);
    runtime.setStartLevel(startLevel);
    if (params.fullscreen == true)
    {
        runtime.setConfig("fullscreen", "true");
    }
    if (params.muteMusic)
    {
        // override options
        runtime.enableMusic(false);
    }
    if (!runtime.initSDL())
    {
        return EXIT_FAILURE;
    }
    runtime.initOptions();
    runtime.preRun();
    runtime.paint();

#ifdef __EMSCRIPTEN__
    g_runtime = &runtime;
    emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, on_fullscreen_change);
    emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);
#else
    runtime.checkMusicFiles();
    while (runtime.isRunning())
    {
        loop_handler(&runtime);
    }
#endif
    CGame::destroy();
    return EXIT_SUCCESS;
}
