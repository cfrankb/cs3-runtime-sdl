/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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
#define LOG_TAG "level"
#include <cstring>
#include <stdio.h>
#include "level.h"
#include "map.h"
#include "tilesdata.h"
#include "logger.h"

void splitString(const std::string str, StringVector &list)
{
    int i = 0;
    unsigned int j = 0;
    while (j < str.length())
    {
        if (isspace(str[j]))
        {
            list.push_back(str.substr(i, j - i));
            while (isspace(str[j]) && j < str.length())
            {
                ++j;
            }
            i = j;
            continue;
        }
        ++j;
    }
    list.push_back(str.substr(i, j - i));
}

uint8_t *readFile(const char *fname)
{
    FILE *sfile = fopen(fname, "rb");
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    uint8_t *data = nullptr;
    if (sfile)
    {
        fseek(sfile, 0, SEEK_END);
        int size = ftell(sfile);
        fseek(sfile, 0, SEEK_SET);
        data = new uint8_t[size + 1];
        data[size] = 0;
        if (!readfile(data, size))
        {
            delete[] data;
            LOGE("can't read data for %s\n", fname);
            return nullptr;
        }
        fclose(sfile);
    }
    else
    {
        LOGE("failed to read:%s\n", fname);
    }
    return data;
}

bool getChMap(const char *mapFile, char *chMap)
{
    uint8_t *data = readFile(mapFile);
    if (data == nullptr)
    {
        LOGE("cannot read %s\n", mapFile);
        return false;
    }

    char *p = reinterpret_cast<char *>(data);
    LOGI("parsing tiles.map: %zull\n", strlen(p));
    int i = 0;

    while (p && *p)
    {
        char *n = strstr(p, "\n");
        if (n)
        {
            *n = 0;
            ++n;
        }
        StringVector list;
        splitString(std::string(p), list);
        uint8_t ch = std::stoi(list[3], 0, 16);
        p = n;
        chMap[ch] = i;
        ++i;
    }

    delete[] data;
    return true;
}

bool processLevel(CMap &map, const char *fname)
{
    LOGI("reading file: %s\n", fname);
    uint8_t *data = readFile(fname);
    if (data == nullptr)
    {
        LOGE("failed read: %s\n", fname);
        return false;
    }

    // get level size
    char *ps = reinterpret_cast<char *>(data);
    int maxRows = 0;
    int maxCols = 0;
    while (ps)
    {
        ++maxRows;
        char *u = strstr(ps, "\n");
        if (u)
        {
            *u = 0;
            maxCols = std::max(static_cast<int>(strlen(ps) + 1), maxCols);
            *u = '\n';
        }

        ps = u ? u + 1 : nullptr;
        //   printf("maxrows %d\n", maxRows);
    }
    LOGI("maxRows: %d, maxCols:%d\n", maxRows, maxCols);

    map.resize(maxCols, maxRows, true);
    map.clear();

    // convert ascii to map
    uint8_t *p = data;
    int x = 0;
    int y = 0;
    while (*p)
    {
        uint8_t c = *p;
        ++p;
        if (c == '\n')
        {
            ++y;
            x = 0;
            continue;
        }

        uint8_t m = getChTile(c);
        if (c != ' ' && m == 0)
        {
            LOGW("undefined %c found at %d %d.\n", c, x, y);
        }
        map.set(x, y, m);
        ++x;
    }
    delete[] data;
    return true;
}

bool convertCs3Level(CMap &map, const char *fname)
{
    const uint16_t convTable[] = {
        TILES_BLANK,
        TILES_WALL_BRICK,
        TILES_ANNIE2,
        TILES_STOP,
        TILES_DIAMOND,
        TILES_AMULET1,
        TILES_CHEST,
        TILES_TRIFORCE,
        TILES_BOULDER,
        TILES_KEY01,
        TILES_DOOR01,
        TILES_KEY02,
        TILES_DOOR02,
        TILES_KEY03,
        TILES_DOOR03,
        TILES_KEY04,
        TILES_DOOR04,
        TILES_WALLS93,
        TILES_WALLS93_2,
        TILES_WALLS93_3,
        TILES_WALLS93_4,
        TILES_WALLS93_5,
        TILES_WALLS93_6,
        TILES_WALLS93_7,
        TILES_FLOWERS_2,
        TILES_TREE,
        TILES_ROCKS0,
        TILES_ROCKS1,
        TILES_ROCKS2,
        TILES_TOMB,
        TILES_SWAMP,
        TILES_VAMPLANT,
        TILES_INSECT1,
        TILES_TEDDY93,
        TILES_OCTOPUS,
        TILES_BLANK,
        TILES_BLANK,
        TILES_BLANK,
        TILES_DIAMOND + 0x100,
        TILES_WALLS93_2 + 0x100,
        TILES_DIAMOND + 0x200,
        TILES_WALLS93_2 + 0x200,
        TILES_DIAMOND + 0x300,
        TILES_WALLS93_2 + 0x300,
        TILES_DIAMOND + 0x400,
        TILES_BLANK + 0x400,
        TILES_DIAMOND + 0x500,
        TILES_BLANK + 0x500,
        TILES_DIAMOND + 0x600,
        TILES_BLANK + 0x600, // 0x31
        TILES_BLANK,         // 0x32
        TILES_BLANK,         // 0x33
        TILES_BLANK,         // 0x34
        TILES_YAHOO,         // 0x35
    };

    uint8_t *data = readFile(fname);
    if (data == nullptr)
    {
        LOGE("failed read: %s\n", fname);
        return false;
    }

    map.clear();
    map.resize(64, 64, true);
    uint8_t *p = data + 7;
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            uint8_t oldTile = *p;
            if (oldTile >= sizeof(convTable) / 2)
            {
                LOGI("oldTile: %d\n", oldTile);
                oldTile = 0;
            }
            const uint16_t data = convTable[oldTile];
            const uint8_t attr = static_cast<uint8_t>(data >> 8);
            const uint8_t tile = static_cast<uint8_t>(data & 0xff);
            map.set(x, y, tile);
            map.setAttr(x, y, attr);
            ++p;
        }
    }

    delete[] data;
    return true;
}

bool fetchLevel(CMap &map, const char *fname, std::string &error)
{
    const int bufferSize = strlen(fname) + 128;
    char *tmp = new char[bufferSize];
    LOGI("fetching: %s\n", fname);

    FILE *sfile = fopen(fname, "rb");
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    if (!sfile)
    {
        snprintf(tmp, bufferSize, "can't open file: %s", fname);
        error = tmp;
        delete[] tmp;
        return false;
    }
    delete[] tmp;

    const char MAPZ_SIGNATURE[] = {'M', 'A', 'P', 'Z'};
    char sig[sizeof(MAPZ_SIGNATURE)] = {0, 0, 0, 0};
    readfile(sig, sizeof(sig));
    if (memcmp(sig, MAPZ_SIGNATURE, sizeof(MAPZ_SIGNATURE)) == 0)
    {
        fclose(sfile);
        LOGI("level is MAPZ\n");
        return map.read(fname);
    }

    fseek(sfile, 0, SEEK_END);
    int size = ftell(sfile);
    const int cs3LevelSize = 64 * 64 + 7;
    if (size == cs3LevelSize)
    {
        fclose(sfile);
        LOGI("level is cs3\n");
        return convertCs3Level(map, fname);
    }

    fclose(sfile);
    LOGI("level is map\n");
    return processLevel(map, fname);
}
