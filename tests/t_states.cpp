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
#define LOG_TAG "states"
#include "thelper.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include <cstdio>
#include <cstring>
#include "../src/logger.h"
#include <filesystem>

constexpr const uint8_t k1 = 1;
constexpr const char *v1 = "test1";
constexpr const uint8_t k2 = 0x22;
constexpr const uint16_t v2 = 0x44;

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

    constexpr const char *outpath0 = "tests/out/state0.bin";
    constexpr const char *outpath1 = "tests/out/state1.bin";
    constexpr const char *outpath2 = "tests/out/state2.bin";
    constexpr const char *outpath3 = "tests/out/state3.bin";

    // write test
    tfile = fopen(outpath0, "wb");
    if (!tfile)
    {
        LOGE("fail to create %s\n", outpath0);
        return false;
    }
    if (!states.write(tfile))
    {
        LOGE("fail to write %s\n", outpath0);
        return false;
    }
    fclose(tfile);

    if (!file.open(outpath1, "wb"))
    {
        LOGE("fail to create %s\n", outpath1);
        return false;
    }
    if (!states.write(file))
    {
        LOGE("fail to write %s\n", outpath1);
        return false;
    }
    file.close();

    // read test
    states.clear();
    sfile = fopen(outpath0, "rb");
    if (!sfile)
    {
        LOGE("fail to open for reading %s\n", outpath0);
        return false;
    }
    if (!states.read(sfile))
    {
        LOGE("fail to read %s\n", outpath0);
        return false;
    }
    fclose(sfile);
    if (!checkState(states, "read FILE*"))
        return false;

    if (!file.open(outpath2, "wb"))
    {
        LOGE("fail to create %s\n", outpath2);
        return false;
    }
    if (!states.write(file))
    {
        LOGE("fail to write %s\n", outpath2);
        return false;
    }
    file.close();

    states.clear();
    if (!file.open(outpath0, "rb"))
    {
        LOGE("fail to open for reading %s\n", outpath0);
        return false;
    }
    if (!states.read(file))
    {
        LOGE("fail to read %s\n", outpath0);
        return false;
    }
    file.close();
    if (!checkState(states, "read FileWrap"))
        return false;

    if (!file.open(outpath3, "wb"))
    {
        LOGE("fail to create %s\n", outpath3);
        return false;
    }
    if (!states.write(file))
    {
        LOGE("fail to write %s\n", outpath3);
        return false;
    }
    file.close();

    CStates states2 = states;
    if (!checkState(states2, "copy constructor"))
        return false;

    // clean up
    std::filesystem::remove(outpath0);
    std::filesystem::remove(outpath1);
    std::filesystem::remove(outpath2);
    std::filesystem::remove(outpath3);
    return true;
}