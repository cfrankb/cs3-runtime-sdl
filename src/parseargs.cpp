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
#include "runtime.h"

void showHelp()
{
    puts("\ncs3v2-runtime (Creepspread III)\n"
         "\n"
         "options:\n"
         "-p <prefix>               set data path prefix\n"
         "-m <maparch>              set maparch override (full path)\n"
         "-w <workspace>            set user workspace\n"
         "--window 999x999          set window size\n"
         "\n"
         "flags:\n"
         "--easy                    switch to easy mode\n"
         "--hard                    switch to hard mode\n"
         "--normal                  switch to normal mode\n"
         "-f                        start in fullscreen\n"
         "-v                        verbose\n"
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
    params.width = CRuntime::DEFAULT_WIDTH;
    params.height = CRuntime::DEFAULT_HEIGHT;
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

bool parseArgs(const std::vector<std::string> &list, params_t &params, bool &appExit)
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
    for (size_t i = 0; i < list.size(); ++i)
    {
        size_t j;
        for (j = 0; j < defcount; ++j)
        {
            if (strcmp(list[i].c_str(), paramdefs[j].param) == 0)
            {
                break;
            }
        }
        // handle options
        if (j != defcount)
        {
            if (i + 1 < list.size() && list[i + 1].c_str()[0] != '-')
            {
                *(paramdefs[j].dest) = list[i + 1].c_str();
                ++i;
            }
            else
            {
                fprintf(stderr, "missing %s value on cmdline\n", paramdefs[j].name);
                result = false;
            }
        }
        else if (strcmp(list[i].c_str(), "--window") == 0)
        {
            if (i + 1 < list.size() && list[i + 1].c_str()[0] != '-')
            {
                std::string s = list[++i];
                const char *w = s.c_str();
                const char *h = strstr(w, "x");
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
        else if (strcmp(list[i].c_str(), "--short") == 0)
        {
            printf("switching to short resolution\n");
            params.width = 480 / 2;
            params.height = 640 / 2;
        }
        else if (strcmp(list[i].c_str(), "--hard") == 0)
        {
            params.skill = SKILL_HARD;
        }
        else if (strcmp(list[i].c_str(), "--normal") == 0)
        {
            params.skill = SKILL_NORMAL;
        }
        else if (strcmp(list[i].c_str(), "--easy") == 0)
        {
            params.skill = SKILL_EASY;
        }
        else if (strcmp(list[i].c_str(), "--help") == 0)
        {
            showHelp();
            appExit = true;
            return true;
        }
        else if (memcmp(list[i].c_str(), "--", 2) == 0)
        {
            fprintf(stderr, "invalid option: %s\n", list[i].c_str());
            result = false;
        }
        // handle switch
        else if (list[i].c_str()[0] == '-')
        {
            for (size_t j = 1; j < strlen(list[i].c_str()); ++j)
            {
                switch (list[i].c_str()[j])
                {
                case 'v':
                    printf("verbose\n");
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
                    fprintf(stderr, "invalid switch: %c\n", list[i].c_str()[j]);
                    result = false;
                }
            }
        }
        else
        {
            params.level = atoi(list[i].c_str());
        }
    }
    return result;
}
