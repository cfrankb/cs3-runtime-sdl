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
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "parseargs.h"
#include "skills.h"

void showHelp()
{
    puts("\ncs3v2-runtime\n"
         "\n"
         "options:\n"
         "-p <prefix>               set data path prefix\n"
         "-m <maparch>              set maparch override (full path)\n"
         "-w <workspace>            set user workspace\n"
         "--window 999x999          set window size\n"
         "\n"
         "flags:\n"
         "--hard                    switch to hard mode\n"
         "--normal                  switch to normal mode\n"
         "-f                        start in fullscreen\n"
         "-h --help                 show this screen\n"
         "-q                        mute music by default\n");
}

void initArgs(params_t &params)
{
    params.fullscreen = false;
    params.level = 0;
    params.muteMusic = false;
    params.skill = SKILL_NORMAL;
    params.hardcore = false;
    params.verbose = false;
    params.width = 320;
    params.height = 240;
}

void verbose(const char *path)
{
    printf("app:%s\n", path);
    try
    {
        // Get the current working directory
        std::filesystem::path currentPath = std::filesystem::current_path();

        // Print the path to the console
        printf("Current folder (std::filesystem): %s\n", currentPath.c_str());
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        // Handle potential errors (e.g., permissions issues)
        fprintf(stderr, "Filesystem error: %s\n", e.what());
    }
}

bool parseArgs(const int argc, char *args[], params_t &params, bool &appExit)
{
    typedef struct
    {
        const char *param;
        const char *name;
        std::string *dest;
    } paramdef_t;
    initArgs(params);

    const paramdef_t paramdefs[] = {
        {"-p", "prefix", &params.prefix},
        {"-m", "mapArch", &params.mapArch},
        {"-w", "workspace", &params.workspace},
    };
    const size_t defcount = sizeof(paramdefs) / sizeof(paramdefs[0]);
    bool result = true;
    for (int i = 1; i < argc; ++i)
    {
        size_t j;
        for (j = 0; j < defcount; ++j)
        {
            if (strcmp(args[i], paramdefs[j].param) == 0)
            {
                break;
            }
        }
        // handle options
        if (j != defcount)
        {
            if (i + 1 < argc && args[i + 1][0] != '-')
            {
                *(paramdefs[j].dest) = args[i + 1];
                ++i;
            }
            else
            {
                fprintf(stderr, "missing %s value on cmdline\n", paramdefs[j].name);
                result = false;
            }
        }
        else if (strcmp(args[i], "--window") == 0)
        {
            if (i + 1 < argc && args[i + 1][0] != '-')
            {
                char *w = args[++i];
                char *h = strstr(w, "x");
                if (w && h)
                {
                    params.width = strtol(w, nullptr, 10) / 2;
                    if (params.width < 240)
                    {
                        fprintf(stderr, "invalid width: %d for --window\n", params.width);
                        result = false;
                    }
                    params.height = strtol(++h, nullptr, 10) / 2;
                    if (params.height < 240)
                    {
                        fprintf(stderr, "invalid height: %d for --window\n", params.height);
                        result = false;
                    }
                    if (params.verbose)
                        printf("w: %d h: %d\n", params.width, params.height);
                }
                else
                {
                    fprintf(stderr, "invalid size for --window\n");
                    result = false;
                }
            }
            else
            {
                fprintf(stderr, "missing size for --window\n");
                result = false;
            }
        }
        else if (strcmp(args[i], "--hard") == 0)
        {
            params.skill = SKILL_HARD;
        }
        else if (strcmp(args[i], "--normal") == 0)
        {
            params.skill = SKILL_NORMAL;
        }
        else if (strcmp(args[i], "--help") == 0)
        {
            showHelp();
            appExit = true;
            return true;
        }
        else if (memcmp(args[i], "--", 2) == 0)
        {
            fprintf(stderr, "invalid option: %s\n", args[i]);
            result = false;
        }
        // handle switch
        else if (args[i][0] == '-')
        {
            for (size_t j = 1; j < strlen(args[i]); ++j)
            {
                switch (args[i][j])
                {
                case 'v':
                    printf("verbose\n");
                    verbose(args[0]);
                    params.verbose = true;
                    break;
                case 'f':
                    printf("fullscreen\n");
                    params.fullscreen = true;
                    break;
                case 'q':
                    printf("muted music\n");
                    params.muteMusic = true;
                    break;
                case 'h':
                    showHelp();
                    appExit = true;
                    return true;
                default:
                    fprintf(stderr, "invalid switch: %c\n", args[i][j]);
                    result = false;
                }
            }
        }
        else
        {
            params.level = atoi(args[i]);
        }
    }
    return result;
}
