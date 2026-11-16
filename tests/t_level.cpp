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

#include <cstdio>
#include <cstring>
#include <filesystem>
#include "../src/logger.h"
#include "../src/level.h"
#include "../src/map.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "t_level.h"

// level.cpp internals (format constants)
namespace
{
    constexpr int CS3_SIZE = 64 * 64 + 7;

    bool writeFile(const char *name, const std::vector<uint8_t> &data)
    {
        FILE *f = fopen(name, "wb");
        if (!f)
            return false;
        const size_t written = fwrite(data.data(), 1, data.size(), f);
        fclose(f);
        return written == data.size();
    }
}

bool test_level()
{
    std::string error;
    CMap map;

    // missing file must report an error
    error.clear();
    if (fetchLevel(map, "tests/in/no-such-level.dat", error) || error.empty())
    {
        LOGE("level: missing file should fail with an error message");
        return false;
    }

    // ---- cs3 binary level (64x64 + 7 byte header) ----
    std::vector<uint8_t> cs3(CS3_SIZE, 0);
    cs3[7 + 0 * 64 + 0] = 0x01;  // TILES_WALL_BRICK, attr 0
    cs3[7 + 0 * 64 + 1] = 0x26;  // TILES_DIAMOND + 0x100
    cs3[7 + 0 * 64 + 2] = 0x35;  // TILES_YAHOO
    cs3[7 + 0 * 64 + 3] = 0x40;  // beyond table -> TILES_BLANK
    if (!writeFile("tests/out/level_cs3.dat", cs3))
    {
        LOGE("level: can't write level_cs3.dat");
        return false;
    }
    if (!fetchLevel(map, "tests/out/level_cs3.dat", error))
    {
        LOGE("level: fetchLevel of cs3 level failed: %s", error.c_str());
        return false;
    }
    if (map.width() != 64 || map.height() != 64)
    {
        LOGE("level: cs3 map (%dx%d); expecting (64x64)", map.width(), map.height());
        return false;
    }
    struct Expect
    {
        int x;
        int y;
        uint16_t tile;
        uint8_t attr;
    };
    const Expect expects[] = {
        {0, 0, TILES_WALL_BRICK, 0},
        {1, 0, TILES_DIAMOND, 0x01},
        {2, 0, TILES_YAHOO, 0},
        {3, 0, TILES_BLANK, 0},
    };
    for (const auto &e : expects)
    {
        if (map.at(e.x, e.y) != e.tile || map.getAttr(e.x, e.y) != e.attr)
        {
            LOGE("level: cs3 tile at (%d, %d) got 0x%.2x/0x%.2x; "
                 "expecting 0x%.2x/0x%.2x",
                 e.x, e.y, map.at(e.x, e.y), map.getAttr(e.x, e.y), e.tile, e.attr);
            return false;
        }
    }
    std::filesystem::remove("tests/out/level_cs3.dat");

    // ---- MAPZ passthrough dispatch (write a real MAPZ, fetch it back) ----
    CMap src;
    src.resize(5, 7, 0, true);
    src.set(1, 2, 0x2a);
    if (!src.write("tests/out/level_gen.mapz"))
    {
        LOGE("level: can't write level_gen.mapz");
        return false;
    }
    if (!fetchLevel(map, "tests/out/level_gen.mapz", error))
    {
        LOGE("level: fetchLevel of mapz failed: %s", error.c_str());
        return false;
    }
    if (map.width() != src.width() || map.height() != src.height() || map.at(1, 2) != 0x2a)
    {
        LOGE("level: mapz dispatch (%dx%d, 0x%.2x); expecting (%dx%d, 0x2a)",
             map.width(), map.height(), map.at(1, 2), src.width(), src.height());
        return false;
    }
    std::filesystem::remove("tests/out/level_gen.mapz");

    // ---- plain text .map ----
    const char *text = "12\n34\n";
    if (!writeFile("tests/out/level_text.map",
                   std::vector<uint8_t>(text, text + std::strlen(text))))
    {
        LOGE("level: can't write level_text.map");
        return false;
    }
    if (!fetchLevel(map, "tests/out/level_text.map", error))
    {
        LOGE("level: fetchLevel of text map failed: %s", error.c_str());
        return false;
    }
    if (map.width() != 2 || map.height() != 2)
    {
        LOGE("level: text map (%dx%d); expecting (2x2)", map.width(), map.height());
        return false;
    }
    const char chars[2][2] = {{'1', '2'}, {'3', '4'}};
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            if (map.at(x, y) != getChTile(static_cast<uint8_t>(chars[y][x])))
            {
                LOGE("level: text tile at (%d, %d) mismatches getChTile", x, y);
                return false;
            }
        }
    }
    std::filesystem::remove("tests/out/level_text.map");

    return true;
}
