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
#include <stdio.h>
#include "runtime.h"
#include "maparch.h"
#include "shared/implementers/mu_sdl.h"

const uint32_t FPS = 24;
const uint32_t SLEEP = 1000 / FPS;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

typedef struct
{
    int level;
    bool musicEnabled;
    std::string prefix;
} config_t;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
CRuntime *g_runtime = nullptr;
extern void playXM(void *userData);
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
#endif

uint32_t lastTick = 0;
bool skip = false;
uint32_t sleepDelay = SLEEP;

void loop_handler(void *arg)
{
    uint32_t currTick = SDL_GetTicks();
    uint32_t meantime = currTick - lastTick;
    if (meantime >= sleepDelay)
    {
        CRuntime *runtime = reinterpret_cast<CRuntime *>(arg);
        uint32_t bticks = SDL_GetTicks();
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

void parseArgs(int argc, char *args[], config_t &config)
{
    config.level = 0;
    config.musicEnabled = true;
    config.prefix = "data/";
    for (int i = 1; i < argc; ++i)
    {
        // set prefix
        if (strcmp(args[i], "-p") == 0)
        {
            if (i + 1 < argc && args[i + 1][0] != '-')
            {
                config.prefix = args[i + 1];
                if (config.prefix.back() != '/')
                {
                    config.prefix += "/";
                }
                ++i;
            }
            else
            {
                printf("missing prefix on cmdline\n");
            }
        }
        // handle flags
        else if (args[i][0] == '-')
        {
            for (int j = 1; j < strlen(args[i]); ++j)
            {
                switch (args[i][j])
                {
                case 'm':
                    config.musicEnabled = false;
                    break;
                default:
                    printf("invalid switch: %c\n", args[i][j]);
                }
            }
        }
        else
        {
            config.level = atoi(args[i]);
        }
    }
}

int main(int argc, char *args[])
{
    CRuntime runtime;
    CMapArch maparch;
    config_t config;
    parseArgs(argc, args, config);
    std::string archFile = config.prefix + "levels.mapz";
    if (!maparch.read(archFile.c_str()))
    {
        fprintf(stderr, "failed to read maparch: %s %s\n", archFile.c_str(), maparch.lastError());
        return EXIT_FAILURE;
    }
    std::string configFile = config.prefix + "game.cfg";
    runtime.setPrefix(config.prefix.c_str());
    runtime.parseConfig(configFile.c_str());
    runtime.init(&maparch, config.level % maparch.size());
    runtime.enableHiScore();
    if (config.musicEnabled)
    {
        runtime.enableMusic();
    }
    if (!runtime.SDLInit())
    {
        return EXIT_FAILURE;
    }
    runtime.preRun();
    runtime.paint();
#ifdef __EMSCRIPTEN__
    g_runtime = &runtime;
    emscripten_set_interval(playXM, 30, nullptr);
    emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);
#else
    while (true)
    {
        loop_handler(&runtime);
    }
#endif
    return EXIT_SUCCESS;
}
