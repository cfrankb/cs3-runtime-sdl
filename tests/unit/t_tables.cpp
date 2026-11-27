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
#include <fstream>

#include "../../src/logger.h"
#include "../../src/color.h"
#include "../../src/sounds.h"
#include "../../src/names.h"
#include "../../src/bossdata.h"
#include "../../src/tilesdata.h"
#include "../../src/tilesdefs.h"
#include "../../src/colormap.h"
#include "../../src/layerdata.h"
#include "t_tables.h"

namespace
{
    bool testNames()
    {
        // path names with and without the "Path::" prefix
        if (resolvePath("Path::ASTAR") != BossData::ASTAR ||
            resolvePath("BFS") != BossData::BFS ||
            resolvePath("LOS") != BossData::LOS ||
            resolvePath("ASTAR_SMOOTH") != BossData::ASTAR_SMOOTH ||
            resolvePath("DIJKSTRA") != BossData::DIJKSTRA ||
            resolvePath("Path::DIJKSTRA") != BossData::DIJKSTRA ||
            resolvePath("FLOWFIELD") != BossData::FLOWFIELD ||
            resolvePath("Path::FLOWFIELD") != BossData::FLOWFIELD ||
            resolvePath("THETASTAR") != BossData::THETASTAR ||
            resolvePath("Path::THETASTAR") != BossData::THETASTAR ||
            resolvePath("RAYCAST") != BossData::RAYCAST ||
            resolvePath("Path::RAYCAST") != BossData::RAYCAST ||
            resolvePath("RAYCAST_BULLET") != BossData::RAYCAST_BULLET ||
            resolvePath("Path::RAYCAST_BULLET") != BossData::RAYCAST_BULLET ||
            resolvePath("ASTAR_BUDGET") != BossData::ASTAR_BUDGET ||
            resolvePath("Path::ASTAR_BUDGET") != BossData::ASTAR_BUDGET ||
            resolvePath("GBFS") != BossData::GBFS ||
            resolvePath("Path::GBFS") != BossData::GBFS ||
            resolvePath("FLEE") != BossData::FLEE ||
            resolvePath("Path::FLEE") != BossData::FLEE ||
            resolvePath("BOGUS") != 0)
        {
            LOGE("resolvePath mismatch");
            return false;
        }
        if (resolveTile("TILES_BLANK") != TILES_BLANK ||
            resolveTile("TILES_EGG_ROTTEN") != TILES_EGG_ROTTEN ||
            resolveTile("NO_SUCH_TILE") != 0)
        {
            LOGE("resolveTile mismatch");
            return false;
        }
        if (resolveSound("SOUND_KEY") != SOUND_KEY ||
            resolveSound("NO_SUCH_SOUND") != 0)
        {
            LOGE("resolveSound mismatch");
            return false;
        }
        if (resolveColor("WHITE") != WHITE ||
            resolveColor("SEAGREEN") != SEAGREEN ||
            resolveColor("NO_SUCH_COLOR") != CLEAR)
        {
            LOGE("resolveColor mismatch");
            return false;
        }
        // flags: single, OR of two, explicit none, numeric fallback, empty
        if (resolveFlags("FLAG_FIREBALL") != FLAG_FIREBALL ||
            resolveFlags("FLAG_FIREBALL BOSS_FLAG_ICE_DAMAGE") !=
                static_cast<uint32_t>(FLAG_FIREBALL | BOSS_FLAG_ICE_DAMAGE) ||
            resolveFlags("0") != 0 ||
            resolveFlags("") != 0 ||
            resolveFlags("16") != 16)
        {
            LOGE("resolveFlags mismatch");
            return false;
        }
        return true;
    }

    bool testColorMaps()
    {
        ColorMaps maps;
        const char *good =
            "[sugarrush]\n"
            "0x01 0x0A\n"
            "\n"
            "[godmode]\n"
            "0x02 0x0B\n"
            "[rage]\n"
            "0x03 0x0C\n"
            "[poison]\n"
            "0x04 0x0D\n";
        if (!parseColorMaps(good, maps) ||
            maps.sugarRush.size() != 1 || maps.godMode.size() != 1 || maps.rage.size() != 1 ||
            maps.poison.size() != 1 ||
            maps.sugarRush.count(0x01) != 1 || maps.godMode.count(0x02) != 1 ||
            maps.rage.count(0x03) != 1 || maps.poison.count(0x04) != 1)
        {
            LOGE("parseColorMaps valid input mismatch");
            return false;
        }
        // reparse replaces previous content
        const char *smaller = "[rage]\n0x10 0x20\n";
        if (!parseColorMaps(smaller, maps) || !maps.sugarRush.empty() ||
            !maps.godMode.empty() || !maps.poison.empty() || maps.rage.size() != 1)
        {
            LOGE("parseColorMaps did not clear previous maps");
            return false;
        }
        ColorMaps empty;
        if (parseColorMaps("", empty) || parseColorMaps("[bogus]\n", empty) ||
            parseColorMaps("0x01 0x02\n", empty) || // kv before any section
            parseColorMaps("[rage]\n1 2\n", empty)) // missing 0x prefix
        {
            LOGE("parseColorMaps accepted invalid input");
            return false;
        }
        if (!parseColorMaps("[poison]\n0x05 0x06\n", empty) || empty.poison.size() != 1)
        {
            LOGE("parseColorMaps rejected a poison-only file");
            return false;
        }
        return true;
    }

    bool testLayerData()
    {
        const std::string path = "/tmp/opencode/t_layers.json";
        {
            std::ofstream f(path);
            f << R"({"tiles":[
                {"index":0,"next":-1},
                {"index":5,"next":9,"speed":3,"type":2,"w":4,"granular":1,"manual":1},
                {"index":9999},
                {}
            ]})";
        }
        static layerdata_t layers[TOTAL_TILE_COUNT];
        if (!loadTileLayer(path, layers, 0))
        {
            LOGE("loadTileLayer failed on valid json");
            std::filesystem::remove(path);
            return false;
        }
        std::filesystem::remove(path);

        // defaults applied to untouched tiles
        if (layers[1].granular != 0 || layers[1].nextTile != 0 ||
            layers[1].animeSpeed != 1 || layers[1].tileType != Background ||
            layers[1].weight != 1 || layers[1].manual != false)
        {
            LOGE("loadTileLayer defaults mismatch");
            return false;
        }
        // "next":-1 maps to DUMMY_NEXT_TILE then 0
        if (layers[0].nextTile != 0)
        {
            LOGE("index 0: nextTile %u; expecting 0", layers[0].nextTile);
            return false;
        }
        // parsed fields, incl. baseIdx offset on next
        if (layers[5].nextTile != 9 || layers[5].animeSpeed != 3 ||
            layers[5].tileType != Solid || layers[5].weight != 4 ||
            layers[5].granular != 1 || layers[5].manual != true)
        {
            LOGE("index 5: parsed tile fields mismatch");
            return false;
        }
        // baseIdx offsets both the array slot and the next tile
        static layerdata_t layers2[TOTAL_TILE_COUNT];
        {
            std::ofstream f(path);
            f << R"({"tiles":[{"index":0,"next":7}]})";
        }
        const bool ok = loadTileLayer(path, layers2, LAYER_TILE_COUNT);
        std::filesystem::remove(path);
        if (!ok || layers2[LAYER_TILE_COUNT].nextTile != 7 + LAYER_TILE_COUNT)
        {
            LOGE("loadTileLayer baseIdx offset mismatch");
            return false;
        }
        if (loadTileLayer("/tmp/opencode/no-such-file.json", layers, 0))
        {
            LOGE("loadTileLayer accepted a missing file");
            return false;
        }
        return true;
    }

    bool testSheetTiles()
    {
        // chMap has 128 entries; getChTile wraps
        for (int i = 0; i < 128; ++i)
        {
            if (getChTile(static_cast<uint8_t>(i)) != getChTile(static_cast<uint8_t>(i + 128)))
            {
                LOGE("getChTile(%d) != getChTile(%d)", i, i + 128);
                return false;
            }
        }
        return true;
    }
}

bool test_tables()
{
    return testNames() && testColorMaps() && testLayerData() && testSheetTiles();
}
