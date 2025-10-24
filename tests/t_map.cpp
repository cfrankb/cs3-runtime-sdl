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
#include "t_map.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include <filesystem>
#include <cassert>

static constexpr const char *IN_FILE = "tests/in/levels1.mapz";
static constexpr const char *OUT_FILE1 = "tests/out/map1.mapz";
static constexpr uint16_t KEY1 = 0x1234;
static constexpr uint16_t KEY2 = 0x1455;
static constexpr uint16_t MISSING_KEY = 0x5511;
static constexpr const char *STRING1 = "Green Grass";
static constexpr uint16_t VALUE2 = 0x1990;
static constexpr const char *TITLE = "Roses Are Red";
static constexpr uint8_t ATTRX = 4;
static constexpr uint8_t ATTRY = 6;
static constexpr uint8_t ATTRA = 0x99;

struct Size
{
    int w;
    int h;
};

struct Attr
{
    int x;
    int y;
    int a;
};

bool test_map()
{
    CMap map;
    const Size mapSize{10, 15};
    if (!map.resize(mapSize.w, mapSize.h, '\0', true))
    {
        LOGE("failed to resize map");
        return false;
    }
    if (map.len() != mapSize.w || map.hei() != mapSize.h)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)",
             map.len(), map.hei(), mapSize.w, mapSize.h);
        return false;
    }

    const Size mapSize2 = Size{20, 25};
    if (!map.resize(mapSize2.w, mapSize2.h, '\0', false))
    {
        LOGE("failed to resize map");
        return false;
    }
    if (map.len() != mapSize2.w || map.hei() != mapSize2.h)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)",
             map.len(), map.hei(), mapSize2.w, mapSize2.h);
        return false;
    }

    map.fill(' ');
    if (map.count(' ') != map.size())
    {
        LOGE("map cound failed. returned %d while expecting %d",
             map.count(' '), map.size());
        return false;
    }

    Attr attr{10, 15, 0x40};
    map.set(attr.x, attr.y, attr.a);
    if (map.at(attr.x, attr.y) != attr.a)
    {
        LOGE("map set failed. map get returned 0x%.x. expecting 0x%.x", map.at(attr.x, attr.y), attr.a);
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
        LOGE("map cound failed. returned %d while expecting %d",
             map.count(0x03), 4);
        return false;
    }

    map.setAttr(12, 13, 0xf);
    map.setAttr(5, 6, 0xf);
    if (map.getAttr(12, 13) != 0xf)
    {
        LOGE("map getAttr failed. returned %x while expecting %x",
             map.getAttr(12, 13), 0xf);
        return false;
    }

    if (map.attrs().size() != 2)
    {
        LOGE("incorrect attrs count:%ld; expecting :%d",
             map.attrs().size(), 2);
        return false;
    }

    const std::string title = "Great Sun";
    map.setTitle(title.c_str());
    if (strcmp(map.title(), title.c_str()))
    {
        LOGE("map getTitle failed. returned %s while expecting %s",
             map.title(), title.c_str());
        return false;
    }

    return true;
}

static bool checkMap(CMap &map)
{
    CStates &states = map.states();
    if (!states.hasS(KEY1))
    {
        LOGE("map doesn't contains sKey %.4x", KEY1);
        return false;
    }
    if (states.hasS(MISSING_KEY))
    {
        LOGE("map states contains a U-key that shouldn't have %.4x", MISSING_KEY);
        return false;
    }
    if (states.hasU(KEY1))
    {
        LOGE("map states contains a U-key that shouldn't have %.4x", KEY1);
        return false;
    }
    if (!states.hasU(KEY2))
    {
        LOGE("map doesn't contains uKey %.4x", KEY2);
        return false;
    }
    if (strcmp(states.getS(KEY1), STRING1))
    {
        LOGE("stateS %.4x contains `%s`; expecting `%s`", KEY1, states.getS(KEY1), STRING1);
        return false;
    }

    if (states.getU(KEY2) != VALUE2)
    {
        LOGE("stateU %.4x contains `%x`; expecting %x", KEY2, states.getU(KEY2), VALUE2);
        return false;
    }

    if (strcmp(map.title(), TITLE))
    {
        LOGE("map title wrong: `%s`; expecting `%s`", map.title(), TITLE);
        return false;
    }

    if (map.getAttr(ATTRX, ATTRY) != ATTRA)
    {
        LOGE("attr at %x,%x is 0x%x; expecting 0x%x", ATTRX, ATTRY, map.getAttr(ATTRX, ATTRY), ATTRA);
        return false;
    }

    return true;
}

bool testSeq(uint16_t len, uint16_t hei)
{
    CMap map(len, hei);
    if (map.len() != len || map.hei() != hei)
    {
        LOGE("map sizes are wrong: %dx%d; expecting %dx%d", map.len(), map.hei(), len, hei);
        return false;
    }
    map.setAttr(ATTRX, ATTRY, ATTRA);
    CStates &states = map.states();
    states.setS(KEY1, STRING1);
    states.setU(KEY2, 0x1990);
    map.setTitle(TITLE);

    if (!checkMap(map))
    {
        LOGE("testing test_map_2() original map failed");
        return false;
    }

    CMap map2{map};
    if (map2.title() == map.title())
    {
        LOGE("shared ptr");
        return false;
    }

    if (&map2.get(0, 0) == &map.get(0, 0))
    {
        LOGE("shared ptr");
        return false;
    }

    if (!checkMap(map2))
    {
        LOGE("testing test_map_2() Constructor failed");
        return false;
    }

    CMap map3 = map;
    if (!checkMap(map3))
    {
        LOGE("testing test_map_2() Constructor failed");
        return false;
    }
    map3 = map;
    if (!checkMap(map3))
    {
        LOGE("testing test_map_2()  Operator= failed");
        return false;
    }

    map.write(OUT_FILE1);
    CMap map4;
    map4.read(OUT_FILE1);
    if (!checkMap(map4))
    {
        LOGE("testing test_map_2() Read(filename) failed");
        return false;
    }

    CFileWrap file;
    if (!file.open(OUT_FILE1))
    {
        LOGE("can't open %s", OUT_FILE1);
        return false;
    }
    CMap map5;
    map5.read(file);
    file.close();
    if (!checkMap(map5))
    {
        LOGE("testing test_map_2() Read(IFile&) failed");
        return false;
    }

    if (!file.open(OUT_FILE1))
    {
        LOGE("can't open %s", OUT_FILE1);
        return false;
    }
    int size = file.getSize();
    char *buf = new char[size];
    if (file.read(buf, size) != IFILE_OK)
    {
        file.close();
        delete[] buf;
        LOGE("can't read %s", OUT_FILE1);
        return false;
    }
    file.close();
    CMap map6;
    if (!map6.fromMemory(reinterpret_cast<uint8_t *>(buf)))
    {
        LOGE("failed to serialize map fromMemory");
        return false;
    }
    delete[] buf;
    if (!checkMap(map6))
    {
        LOGE("testing test_map_2() fromMemory() failed");
        return false;
    }
    if (map6.len() != len || map6.hei() != hei)
    {
        LOGE("map sizes are wrong: %dx%d; expecting %dx%d", map6.len(), map6.hei(), len, hei);
        return false;
    }

    std::filesystem::remove(OUT_FILE1);
    return true;
}

bool test_map_2()
{
    return testSeq(10, 15) &&
           testSeq(256, 256);
}

static void fillMap(CMap &map)
{
    // Set tiles: [1, 2, 3]
    //            [4, 5, 6]
    //            [7, 8, 9]
    for (int y = 0, v = 1; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x, ++v)
        {
            map.set(x, y, v);
        }
    }
}

bool test_map_up()
{
    CMap map(3, 3, 0);
    // Set tiles: [1, 2, 3]
    //            [4, 5, 6]
    //            [7, 8, 9]
    fillMap(map);
    map.setAttr(1, 1, 0xFF); // Attribute at (1,1)
    map.shift(CMap::Direction::UP);
    // Expected: [4, 5, 6]
    //           [7, 8, 9]
    //           [1, 2, 3]
    // Attribute at (1,0)
    assert(map.at(1, 0) == 5);
    assert(map.at(1, 2) == 2);
    assert(map.getAttr(1, 0) == 0xFF);
    return true;
}

bool test_map_left()
{
    CMap map(3, 3, 0);
    // Set tiles: [1, 2, 3]
    //            [4, 5, 6]
    //            [7, 8, 9]
    fillMap(map);
    map.setAttr(0, 0, 0xff);
    map.shift(CMap::Direction::LEFT);

    // Expected : [2, 3, 1]
    //            [5, 6, 4]
    //            [8, 9, 7]

    if (map.getAttr(2, 0) != 0xff)
        return false;

    if (map.at(0, 0) != 2)
        return false;

    if (map.at(1, 1) != 6)
        return false;

    if (map.at(2, 2) != 7)
        return false;

    return true;
}

bool test_map_right()
{
    CMap map(3, 3, 0);
    // Set tiles: [1, 2, 3]
    //            [4, 5, 6]
    //            [7, 8, 9]
    fillMap(map);
    map.setAttr(2, 2, 0xff);
    map.shift(CMap::Direction::RIGHT);

    // Expected : [3, 1, 2]
    //            [6, 4, 5]
    //            [9, 7, 8]

    if (map.getAttr(0, 2) != 0xff)
        return false;

    if (map.at(0, 0) != 3)
        return false;

    if (map.at(1, 1) != 4)
        return false;

    if (map.at(2, 2) != 8)
        return false;

    return true;
}

bool test_map_down()
{
    CMap map(3, 3, 0);
    // Set tiles: [1, 2, 3]
    //            [4, 5, 6]
    //            [7, 8, 9]
    fillMap(map);
    map.setAttr(2, 2, 0xff);
    map.shift(CMap::Direction::DOWN);

    // Expected : [7, 8, 9]
    //            [1, 2, 3]
    //            [4, 5, 6]

    if (map.getAttr(2, 0) != 0xff)
        return false;

    if (map.at(0, 0) != 7)
        return false;

    if (map.at(1, 1) != 2)
        return false;

    if (map.at(2, 2) != 6)
        return false;

    return true;
}
