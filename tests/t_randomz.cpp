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

#include "../src/logger.h"
#include "../src/randomz.h"
#include "t_randomz.h"

bool test_random()
{
    // same seed -> same sequence
    Random a(42);
    Random b(42);
    for (int i = 0; i < 100; ++i)
    {
        if (a.next() != b.next())
        {
            LOGE("Random: same seed diverged at %d", i);
            return false;
        }
    }

    // reset replays the sequence
    Random r(1234);
    uint32_t first[10];
    for (auto &v : first)
        v = r.next();
    r.reset();
    for (auto v : first)
    {
        if (r.next() != v)
        {
            LOGE("Random: reset did not replay sequence");
            return false;
        }
    }

    // different seeds diverge
    if (Random(1).next() == Random(2).next())
    {
        LOGE("Random: different seeds produced identical first value");
        return false;
    }

    // zero state is avoided (xorshift would get stuck at 0)
    if (Random(0, 0).next() == 0)
    {
        LOGE("Random: zero state not avoided");
        return false;
    }

    // setTick matches constructing with that tick
    Random c(42, 7);
    Random d(42);
    d.setTick(7);
    if (c.next() != d.next())
    {
        LOGE("Random: setTick inconsistent with constructor");
        return false;
    }

    // range stays in [min, max)
    Random e(777);
    for (int i = 0; i < 1000; ++i)
    {
        int v = e.range(5, 10);
        if (v < 5 || v >= 10)
        {
            LOGE("Random.range out of bounds: %d", v);
            return false;
        }
    }
    if (Random().range(7, 7) != 7)
    {
        LOGE("Random.range(min==min) should return min");
        return false;
    }

    // precomputed table wraps at TABLE_SIZE
    if (PrecomputedRandom::get(0) != PrecomputedRandom::get(PrecomputedRandom::TABLE_SIZE) ||
        PrecomputedRandom::get(37) != PrecomputedRandom::table[37])
    {
        LOGE("PrecomputedRandom: table lookup/wrap mismatch");
        return false;
    }

    return true;
}
