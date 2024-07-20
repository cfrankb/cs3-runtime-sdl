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
#include <unistd.h>
#include <stdio.h>
#include "runtime.h"
#include "maparch.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define FPS 24

void loop_handler(void *arg)
{
    CRuntime *runtime = reinterpret_cast<CRuntime *>(arg);
    usleep(1000 / FPS * 1000);
    runtime->doInput();
    runtime->paint();
    runtime->run();
}

int main(int argc, char *args[])
{
    CRuntime runtime;
    CMapArch maparch;
    if (!maparch.read("data/levels.mapz"))
    {
        printf("failed to read maparch: %s\n", maparch.lastError());
    }

    runtime.init(&maparch, 0);
    runtime.enableHiScore();
    runtime.enableMusic();
    runtime.SDLInit();
    runtime.preRun();
    runtime.paint();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop_handler, &runtime, -1, 1);
// emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 1000/FPS*1000);
#else
    while (true)
    {
        loop_handler(&runtime);
    }
#endif
    return 0;
}
