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
#include "../src/shared/DotArray.h"
#include "t_dotarray.h"

bool test_dotarray()
{
    CDotArray dots;
    if (!dots.isEmpty() || dots.getSize() != 0)
    {
        LOGE("fresh CDotArray should be empty");
        return false;
    }

    dots.add(0xff, 3, 4);
    dots.add(Dot(5, 6, 0xee));
    if (dots.isEmpty() || dots.getSize() != 2)
    {
        LOGE("CDotArray size %zu; expecting 2", dots.getSize());
        return false;
    }
    if (dots[0].x != 3 || dots[0].y != 4 || dots[0].color != 0xff ||
        dots[1].x != 5 || dots[1].y != 6 || dots[1].color != 0xee)
    {
        LOGE("CDotArray stored dot mismatch");
        return false;
    }

    dots.flush();
    if (!dots.isEmpty())
    {
        LOGE("flush did not clear");
        return false;
    }

    // clipping
    dots.setLimit(10, 10);
    dots.add(0x11, 9, 9);
    dots.add(0x11, 10, 5); // x out of bounds
    dots.add(0x11, 5, 10); // y out of bounds
    dots.add(0x11, -1, 5); // negative
    if (dots.getSize() != 1)
    {
        LOGE("setLimit clipping kept %zu; expecting 1", dots.getSize());
        return false;
    }
    dots.flush();

    // horizontal line draws every column
    dots.setLimit(-1, -1);
    const int n = dots.lineTab(0x22, Dot(0, 0), Dot(10, 0));
    if (n != 11 || (int)dots.getSize() != n)
    {
        LOGE("lineTab drawn %d; expecting 11", n);
        return false;
    }
    if (dots[0].x != 0 || dots[n - 1].x != 10)
    {
        LOGE("lineTab endpoints mismatch");
        return false;
    }
    for (int i = 0; i < n; ++i)
    {
        if (dots[i].x != i || dots[i].y != 0)
        {
            LOGE("lineTab gap at %d", i);
            return false;
        }
    }

    // circle: r = |10-20|/2 = 5 around (20,15), symmetric plot
    dots.setLimit(400, 400);
    const int c = dots.circle(0x33, Dot(20, 10), Dot(20, 20));
    if (c <= 0 || (int)dots.getSize() != c)
    {
        LOGE("circle returned %d", c);
        return false;
    }
    for (const auto &d : dots.dots())
    {
        if (d.x < 15 || d.x > 25 || d.y < 10 || d.y > 20)
        {
            LOGE("circle dot (%d, %d) outside bounds", d.x, d.y);
            return false;
        }
    }

    // degenerate circle (r == 0) plots a single dot
    dots.circle(0x44, Dot(1, 1), Dot(1, 1));
    if (dots.getSize() != 1)
    {
        LOGE("zero-radius circle plotted %zu; expecting 1", dots.getSize());
        return false;
    }

    return true;
}
