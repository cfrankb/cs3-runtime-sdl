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
#include "parseargs.h"

void showHelp()
{
    puts("\ncs3v2-runtime\n"
         "\n"
         "options:\n"
         "-p <prefix>               set game prefix path\n"
         "-m <maparch>              set maparch override\n"
         "-w <workspace>            set user workspace\n"
         //"-s 999x999                set window size\n"
         "\n"
         "flags:\n"
         "-f                        start in fullscreen\n"
         "-h                        show this screen\n"
         "-q                        mute music by default\n");
}

void initArgs(params_t &params)
{
    params.fullscreen = false;
    params.level = 0;
    params.muteMusic = false;
}

bool parseArgs(const int argc, char *args[], params_t &params)
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
        // handle switch
        else if (args[i][0] == '-')
        {
            for (int j = 1; j < strlen(args[i]); ++j)
            {
                switch (args[i][j])
                {
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
                    return EXIT_SUCCESS;
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
