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
#include "recorder.h"
#include <cstring>

void test_recorder()
{
    FILE *tfile;
    CRecorder rec;
    tfile = fopen("test0000.rec", "wb");
    rec.start(tfile, true);
    rec.stop();

    enum
    {
        NA = 0xff,
        UP = 0,
        DOWN = 1,
        LEFT = 2,
        RIGHT = 3,
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

    uint8_t input[4];
    tfile = fopen("test0001.rec", "wb");
    rec.start(tfile, true);
    for (size_t i = 0; i < sizeof(data); ++i)
    {
        int j = data[i];
        memset(input, 0, 4);
        if (j != 0xff)
            input[j] = 1;
        rec.append(input);
    }
    rec.stop();

    FILE *sfile = fopen("test0001.rec", "rb");
    rec.start(tfile, false);
    uint8_t output[4];
    for (size_t i = 0; i < sizeof(data); ++i)
    {
        memset(output, 0, 4);
        bool result = rec.get(output);
        if (!result)
        {
            printf("premature exit\n");
            break;
        }
        uint8_t k = 0xff;
        for (int j = 0; j < 4; ++j)
        {
            if (output[j])
            {
                k = j;
                break;
            }
        }
        // printf("%-2d %.2x %.2x\n", i, data[i], k);
        if (data[i] != k)
        {
            printf("mismatch at %d [%x %x]\n", i, data[i], k);
        }
    }
    rec.stop();
}