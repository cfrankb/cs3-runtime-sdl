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
#include <cstdio>
#include <functional>
#include <vector>
#include <typeinfo>
#include "t_recorder.h"
#include "t_states.h"
#include "t_stateparser.h"
#include "t_maparch.h"
#include "t_strhelper.h"

#define FCT(x) {x, #x}
using Function = std::function<bool(void)>;
struct Test
{
    Function f;
    const char *s;
};

int main(int argc, char *args[])
{
    printf("running tests\n");
    printf("==============\n\n");

    std::vector<Test> fct = {
        FCT(test_recorder),
        FCT(test_states),
        FCT(test_stateparser),
        FCT(test_maparch),
        FCT(test_strhelper),
    };

    int failed = 0;
    for (size_t i = 0; i < fct.size(); ++i)
    {
        printf("==> %s()\n", fct[i].s);
        failed += !fct[i].f();
    }

    if (failed)
        fprintf(stderr, "\n\n###### failed: %d\n", failed);
    else
        printf("\n\n###### completed\n");
    return failed != 0;
}