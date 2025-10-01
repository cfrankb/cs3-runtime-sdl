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
#define LOG_TAG "t_map"
#include <cstring>
#include "t_map.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include <filesystem>

constexpr const char *IN_FILE = "tests/in/levels1.mapz";
constexpr const char *OUT_FILE1 = "tests/out/map1.mapz";
constexpr uint16_t KEY1 = 0x1234;
constexpr uint16_t KEY2 = 0x1455;
constexpr uint16_t MISSING_KEY = 0x5511;
constexpr const char *STRING1 = "Green Grass";
constexpr uint16_t VALUE2 = 0x1990;
constexpr const char *TITLE = "Roses Are Red";
constexpr uint8_t ATTRX = 4;
constexpr uint8_t ATTRY = 6;
constexpr uint8_t ATTRA = 0x99;

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
    if (!map.resize(mapSize.w, mapSize.h, true))
    {
        LOGE("failed to resize map\n");
        return false;
    }
    if (map.len() != mapSize.w || map.hei() != mapSize.h)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)\n",
             map.len(), map.hei(), mapSize.w, mapSize.h);
        return false;
    }

    const Size mapSize2 = Size{20, 25};
    if (!map.resize(mapSize2.w, mapSize2.h, false))
    {
        LOGE("failed to resize map\n");
        return false;
    }
    if (map.len() != mapSize2.w || map.hei() != mapSize2.h)
    {
        LOGE("map sized (%dx%d) was expected (%dx%d)\n",
             map.len(), map.hei(), mapSize2.w, mapSize2.h);
        return false;
    }

    map.fill(' ');
    if (map.count(' ') != map.size())
    {
        LOGE("map cound failed. returned %d while expecting %d\n",
             map.count(' '), map.size());
        return false;
    }

    Attr attr{10, 15, 0x40};
    map.set(attr.x, attr.y, attr.a);
    if (map.at(attr.x, attr.y) != attr.a)
    {
        LOGE("map set failed. map get returned 0x%.x. expecting 0x%.x\n", map.at(attr.x, attr.y), attr.a);
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

bool checkMap(CMap &map)
{
    CStates &states = map.states();
    if (!states.hasS(KEY1))
    {
        LOGE("map doesn't contains sKey %.4x\n", KEY1);
        return false;
    }
    if (states.hasS(MISSING_KEY))
    {
        LOGE("map states contains a U-key that shouldn't have %.4x\n", MISSING_KEY);
        return false;
    }
    if (states.hasU(KEY1))
    {
        LOGE("map states contains a U-key that shouldn't have %.4x\n", KEY1);
        return false;
    }
    if (!states.hasU(KEY2))
    {
        LOGE("map doesn't contains uKey %.4x\n", KEY2);
        return false;
    }
    if (strcmp(states.getS(KEY1), STRING1))
    {
        LOGE("stateS %.4x contains `%s`; expecting `%s`\n", KEY1, states.getS(KEY1), STRING1);
        return false;
    }

    if (states.getU(KEY2) != VALUE2)
    {
        LOGE("stateU %.4x contains `%x`; expecting %x\n", KEY2, states.getU(KEY2), VALUE2);
        return false;
    }

    if (strcmp(map.title(), TITLE))
    {
        LOGE("map title wrong: `%s`; expecting `%s`\n", map.title(), TITLE);
        return false;
    }

    if (map.getAttr(ATTRX, ATTRY) != ATTRA)
    {
        LOGE("attr at %x,%x is 0x%x; expecting 0x%x\n", ATTRX, ATTRY, map.getAttr(ATTRX, ATTRY), ATTRA);
        return false;
    }

    return true;
}

bool testSeq(uint16_t len, uint16_t hei)
{
    CMap map(len, hei);
    if (map.len() != len || map.hei() != hei)
    {
        LOGE("map sizes are wrong: %dx%d; expecting %dx%d\n", map.len(), map.hei(), len, hei);
        return false;
    }
    map.setAttr(ATTRX, ATTRY, ATTRA);
    CStates &states = map.states();
    states.setS(KEY1, STRING1);
    states.setU(KEY2, 0x1990);
    map.setTitle(TITLE);

    if (!checkMap(map))
    {
        LOGE("testing test_map_2() original map failed\n");
        return false;
    }

    CMap map2{map};
    if (map2.title() == map.title())
    {
        LOGE("shared ptr\n");
        return false;
    }

    if (&map2.get(0, 0) == &map.get(0, 0))
    {
        LOGE("shared ptr\n");
        return false;
    }

    if (!checkMap(map2))
    {
        LOGE("testing test_map_2() Constructor failed\n");
        return false;
    }

    CMap map3 = map;
    if (!checkMap(map3))
    {
        LOGE("testing test_map_2() Constructor failed\n");
        return false;
    }
    map3 = map;
    if (!checkMap(map3))
    {
        LOGE("testing test_map_2()  Operator= failed\n");
        return false;
    }

    map.write(OUT_FILE1);
    CMap map4;
    map4.read(OUT_FILE1);
    if (!checkMap(map4))
    {
        LOGE("testing test_map_2() Read(filename) failed\n");
        return false;
    }

    CFileWrap file;
    if (!file.open(OUT_FILE1))
    {
        LOGE("can't open %s\n", OUT_FILE1);
        return false;
    }
    CMap map5;
    map5.read(file);
    file.close();
    if (!checkMap(map5))
    {
        LOGE("testing test_map_2() Read(IFile&) failed\n");
        return false;
    }

    if (!file.open(OUT_FILE1))
    {
        LOGE("can't open %s\n", OUT_FILE1);
        return false;
    }
    int size = file.getSize();
    char *buf = new char[size];
    if (file.read(buf, size) != 1)
    {
        file.close();
        delete[] buf;
        LOGE("can't read %s\n", OUT_FILE1);
        return false;
    }
    file.close();
    CMap map6;
    if (!map6.fromMemory(reinterpret_cast<uint8_t *>(buf)))
    {
        LOGE("failed to serialize map fromMemory\n");
        return false;
    }
    delete[] buf;
    if (!checkMap(map6))
    {
        LOGE("testing test_map_2() fromMemory() failed\n");
        return false;
    }
    if (map6.len() != len || map6.hei() != hei)
    {
        LOGE("map sizes are wrong: %dx%d; expecting %dx%d\n", map6.len(), map6.hei(), len, hei);
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