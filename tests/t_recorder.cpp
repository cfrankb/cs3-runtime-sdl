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
#include <filesystem>
#include <cstring>
#include "../src/recorder.h"
#include "../src/shared/FileWrap.h"
#include "../src/logger.h"

bool test_recorder()
{
    CFileWrap tfile;
    CRecorder rec(5);

    constexpr const char *path0 = "tests/out/test0000.rec";
    if (!tfile.open(path0, "wb"))
    {
        LOGE("can't write file %s", path0);
        return false;
    }
    rec.start(&tfile, true);
    rec.stop();

    enum
    {
        NA = 0xff,
        UP = 0,
        DOWN = 1,
        LEFT = 2,
        RIGHT = 3,
        BUFSIZE = 4
    };

    const uint8_t data[] = {
        NA, NA, NA,                   // 30
        UP, UP, UP,                   // 31
        DOWN, DOWN, DOWN,             // 32
        LEFT, LEFT, LEFT, LEFT, LEFT, // F4
        LEFT, LEFT, LEFT, LEFT, LEFT,
        LEFT, LEFT, LEFT, LEFT, LEFT,
        UP, UP, UP, // 31
        LEFT, LEFT, LEFT, LEFT, LEFT,
        LEFT, LEFT, LEFT, LEFT, LEFT,
        LEFT, LEFT, LEFT, LEFT, LEFT, LEFT, // F4 14
        DOWN, DOWN, DOWN};                  // 32

    // record a pattern (encode)
    uint8_t input[BUFSIZE];
    constexpr const char *path1 = "tests/out/test0001.rec";
    // tfile = fopen("tests/out/test0001.rec", "wb");
    if (!tfile.open(path1, "wb"))
    {
        LOGE("can't write file %s", path1);
        return false;
    }

    rec.start(&tfile, true);
    for (size_t i = 0; i < sizeof(data); ++i)
    {
        int j = data[i];
        memset(input, 0, sizeof(input));
        if (j != NA)
            input[j] = 1;
        rec.append(input);
    }
    rec.stop();

    // decode the pattern and check it against
    // the original source
    // FILE *sfile = fopen("tests/out/test0001.rec", "rb");
    CFileWrap sfile;
    if (!sfile.open(path1, "rb"))
    {
        LOGE("can't read file %s", path1);
        return false;
    }
    rec.start(&sfile, false);
    uint8_t output[BUFSIZE];
    for (size_t i = 0; i < sizeof(data); ++i)
    {
        memset(output, 0, sizeof(output));
        bool result = rec.get(output);
        if (!result)
        {
            LOGE("premature exit");
            return false;
        }
        uint8_t k = NA;
        for (size_t j = 0; j < sizeof(output); ++j)
        {
            if (output[j])
            {
                k = j;
                break;
            }
        }
        //    printf("%-2d %.2x %.2x\n", i, data[i], k);
        if (data[i] != k)
        {
            LOGE("mismatch at %ld [%.2x %.2x]", i, data[i], k);
            return false;
        }
    }
    rec.stop();

    // clean up
    std::filesystem::remove(path0);
    std::filesystem::remove(path1);
    return true;
}