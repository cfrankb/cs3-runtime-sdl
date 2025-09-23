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
#define LOG_TAG "t_maparch"
#include <filesystem>
#include <cstring>
#include "t_map.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"

#define IN_FILE "tests/in/levels1.mapz"

bool test_map()
{
    CMap map;
    if (!map.resize(10, 15, true))
    {
        LOGE("failed to resize map\n");
        return false;
    }
    if (map.len() != 10 || map.hei() != 15)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)\n",
             map.len(), map.hei(), 10, 15);
        return false;
    }

    if (!map.resize(20, 25, false))
    {
        LOGE("failed to resize map\n");
        return false;
    }
    if (map.len() != 20 || map.hei() != 25)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)\n",
             map.len(), map.hei(), 20, 25);
        return false;
    }

    map.fill(' ');
    if (map.count(' ') != map.size())
    {
        LOGE("map cound failed. returned %d while expecting %d\n",
             map.count(' '), map.size());
        return false;
    }

    map.set(10, 15, 0x40);
    if (map.at(10, 15) != 0x40)
    {
        LOGE("map set failed. map get returned 0x%.x. expecting 0x%.x\n", map.at(10, 15), 0x40);
        return false;
    }

    const Pos posD{19, 10};
    map.set(posD.x, posD.y, 0x3);
    const Pos pos = map.findFirst(0x3);
    if (pos != posD)
    {
        LOGE("map find failed. pos returned (%d, %d) expecting (%d, %d)n",
             pos.x, pos.y, posD.x, posD.y);
        return false;
    }

    map.set(4, 6, 0x3);
    map.set(1, 8, 0x3);
    map.set(3, 0, 0x3);
    if (map.count(0x03) != 4)
    {
        LOGE("map cound failed. returned %d while expecting %d\n",
             map.count(0x03), 4);
        return false;
    }

    map.setAttr(12, 13, 0xf);
    map.setAttr(5, 6, 0xf);
    if (map.getAttr(12, 13) != 0xf)
    {
        LOGE("map getAttr failed. returned %x while expecting %x\n",
             map.getAttr(12, 13), 0xf);
        return false;
    }

    if (map.attrs().size() != 2)
    {
        LOGE("incorrect attrs count:%ld; expecting :%d\n",
             map.attrs().size(), 2);
        return false;
    }

    const std::string title = "Great Sun";
    map.setTitle(title.c_str());
    if (strcmp(map.title(), title.c_str()))
    {
        LOGE("map getTitle failed. returned %s while expecting %s\n",
             map.title(), title.c_str());
        return false;
    }

    return true;
}