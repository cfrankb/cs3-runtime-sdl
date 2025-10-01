/*
    cs3-runtime-sdl
    Copyright (C) 2025 Francois Blanchette

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
#define LOG_TAG "tmain"
#include <cstdio>
#include <functional>
#include <vector>
#include <typeinfo>
#include "t_game.h"
#include "t_gamestats.h"
#include "t_maparch.h"
#include "t_map.h"
#include "t_recorder.h"
#include "t_runtime.h"
#include "t_states.h"
#include "t_stateparser.h"
#include "t_strhelper.h"
#include "t_ifile.h"
#include "t_frameset.h"
#include "t_pngmagic.h"
#include "../src/logger.h"
#include <filesystem>

#define FCT(x) {x, #x}
using Function = std::function<bool(void)>;
struct Test
{
    Function f;
    const char *s;
};

int main(int argc, char *args[])
{
    (void)argc;
    (void)args;

    std::filesystem::create_directories("tests/out");
    LOGI("running tests\n");
    LOGI("==============\n\n");

    std::vector<Test> fct = {
        FCT(test_recorder),
        FCT(test_states),
        FCT(test_stateparser),
        FCT(test_map),
        FCT(test_map_2),
        FCT(test_maparch_1),
        FCT(test_maparch_2),
        FCT(test_maparch_3),
        FCT(test_strhelper),
        FCT(test_gamestats),
        FCT(test_runtime),
        FCT(test_game),
        FCT(test_ifile),
        FCT(test_png_magic),
        FCT(test_frameset),
    };

    int failed = 0;
    for (size_t i = 0; i < fct.size(); ++i)
    {
        LOGI("==> %s()\n", fct[i].s);
        failed += !fct[i].f();
    }

    if (failed)
        LOGE("\n\n###### failed: %d\n", failed);
    else
        LOGI("\n\n###### completed\n");
    return failed != 0;
}