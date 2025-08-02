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
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include <cstdio>
#include <cstring>

const uint8_t k1 = 1;
const char *v1 = "test1";

const uint8_t k2 = 0x22;
const uint16_t v2 = 0x44;

static bool checkState(CStates &states, const char *context)
{
    if (strcmp(states.getS(k1), v1))
    {
        printf("%s: string not matching\n", context);
        return false;
    }

    auto a2 = states.getU(k2);
    if (a2 != v2)
    {
        printf("%s: uint8 not matching %d %d\n", context,
               a2, v2);
        return false;
    }
    return true;
}

bool test_states()
{
    CStates states;
    states.setU(k2, v2);
    states.setS(k1, v1);
    if (!checkState(states, "direct"))
        return false;

    FILE *tfile;
    FILE *sfile;
    CFileWrap file;

    // write test
    tfile = fopen("tests/out/state0.bin", "wb");
    states.write(tfile);
    fclose(tfile);

    file.open("tests/out/state1.bin", "wb");
    states.write(file);
    file.close();

    // read test
    states.clear();
    sfile = fopen("tests/out/state0.bin", "rb");
    states.read(sfile);
    fclose(sfile);
    if (!checkState(states, "read FILE*"))
        return false;

    file.open("tests/out/state2.bin", "wb");
    states.write(file);
    file.close();

    states.clear();
    file.open("tests/out/state0.bin", "rb");
    states.read(file);
    file.close();
    if (!checkState(states, "read FileWrap"))
        return false;

    file.open("tests/out/state3.bin", "wb");
    states.write(file);
    file.close();

    CStates states2 = states;
    if (!checkState(states2, "copy constructor"))
        return false;

    return true;
}