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

#include <cstring>
#include <filesystem>
#include "../src/shared/FileMem.h"
#include "../src/logger.h"
#include "../src/animator.h"
#include "../src/game.h"
#include "t_animator.h"

bool test_animator()
{
    constexpr const char *OUT_FILE = "tests/out/animator.dat";

    auto &automators = CGame::getGame()->automators();
    automators.clear();
    automators.push_back({0x1234, 10, 20, 1, 30, 0x40, 1});
    automators.push_back({0x0001, -5, 3, 0, 0, 0x11, 1});

    CAnimator animator1;
    CFileMem mem;
    if (!mem.open(OUT_FILE, "rw+"))
    {
        LOGE("can't write %s", OUT_FILE);
        return false;
    }
    if (!animator1.write(mem))
    {
        LOGE("failed to serialize CAnimator");
        mem.close();
        return false;
    }

    // simulate restore-from-save: wipe game state, then read it back
    automators.clear();
    if (!mem.seek(0))
    {
        LOGE("seek(0) failed");
        mem.close();
        return false;
    }
    CAnimator animator2;
    if (!animator2.read(mem))
    {
        LOGE("failed to deserialize CAnimator");
        mem.close();
        return false;
    }
    mem.close();

    if (automators.size() != 2)
    {
        LOGE("automators restored %zu; expecting 2", automators.size());
        return false;
    }
    const uint16_t want[] = {0x1234, 0x0001};
    for (size_t i = 0; i < 2; ++i)
    {
        if (automators[i].tileID != want[i] ||
            automators[i].x != (int16_t)(i ? -5 : 10) ||
            automators[i].y != (int16_t)(i ? 3 : 20))
        {
            LOGE("automator[%zu] mismatch", i);
            return false;
        }
    }

    // tile animation tables must survive the round-trip
    for (uint8_t tileID = 0; tileID < 0xff; tileID += 0x11)
    {
        if (animator2.at(tileID) != animator1.at(tileID))
        {
            LOGE("animator tile 0x%.2x mismatch", tileID);
            return false;
        }
    }

    automators.clear();
    std::filesystem::remove(OUT_FILE);
    return true;
}
