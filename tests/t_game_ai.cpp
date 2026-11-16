/*
    cs3-runtime-sdl
    Copyright (C) 2026 Francois Blanchette

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

#include "../src/attr.h"
#include "../src/game.h"
#include "../src/gamesfx.h"
#include "../src/logger.h"
#include "../src/map.h"
#include "../src/sprtypes.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "t_game_ai.h"

// Phase 1 of docs/plans/game_ai-test-plan.md: dispatcher + simple movers
// driven through CGame::manageMonsters(). Run LAST in tmain: it mutates the
// CGame singleton (monsters/player/map).

namespace
{
    constexpr int MAP_SIZE = 12;
    constexpr uint32_t TICKS_ALL = 840; // lcm(1..8): every speeds[] slot passes
    constexpr uint32_t TICKS_NONE = 841; // 29^2: only speeds[0..1] pass

    CGame *g_game = nullptr;

    uint16_t tileOf(const uint8_t type, const uint8_t minSpeed = 0, const uint16_t aiMustBeClear = 0)
    {
        for (int i = 0; i < TILES_TOTAL_COUNT; ++i)
        {
            const TileDef &def = getTileDef(i);
            if (def.type == type && def.speed >= minSpeed && !(def.ai & aiMustBeClear))
                return static_cast<uint16_t>(i);
        }
        LOGE("no tile with type 0x%.2x speed>=%u", type, minSpeed);
        return TILES_BLANK; // type mismatch below will fail the test
    }

    // fresh blank map, empty actor lists, player parked out of harm's way
    void resetWorld()
    {
        g_game = CGame::getGame();
        CMap &map = CGame::getMap();
        map.clear();
        map.resize(MAP_SIZE, MAP_SIZE, TILES_BLANK, true);
        g_game->getMonsters().clear();
        g_game->getMonsters().reserve(16); // keep CActor& refs stable across spawn()
        g_game->getSfx().clear();
        g_game->resetStats();
        g_game->player().move(MAP_SIZE - 1, MAP_SIZE - 1);
        g_game->rebuildMonsterGrid(); // flush stale entries of cleared actors
    }

    // plant `tile` in the map under a matching actor; keeps grid/map/vector in sync
    CActor &spawn(const uint16_t tile, const uint8_t type, const int x, const int y, const JoyAim aim)
    {
        if (tile != TILES_BLANK && getTileDef(tile).type != type)
            LOGE("tile 0x%.2x type 0x%.2x; expecting 0x%.2x", tile, getTileDef(tile).type, type);
        CGame::getMap().set(x, y, tile);
        auto &monsters = g_game->getMonsters();
        monsters.emplace_back(CActor(Pos{static_cast<int16_t>(x), static_cast<int16_t>(y)}, type, aim));
        g_game->rebuildMonsterGrid();
        return monsters.back();
    }

    bool expectPos(const char *what, const CActor &actor, const int x, const int y)
    {
        if (actor.pos() != Pos{static_cast<int16_t>(x), static_cast<int16_t>(y)})
        {
            LOGE("%s: actor at (%d,%d); expecting (%d,%d)",
                 what, actor.x(), actor.y(), x, y);
            return false;
        }
        return true;
    }

    bool testIdleAttr()
    {
        resetWorld();
        const uint16_t tile = tileOf(TYPE_MONSTER);
        CActor &monster = spawn(tile, TYPE_MONSTER, 5, 5, AIM_UP);
        CMap &map = CGame::getMap();
        map.setAttr(5, 5, ATTR_IDLE_MIN + 1); // wake up when player within 2

        g_game->manageMonsters(TICKS_ALL); // player still 8 tiles away
        if (!expectPos("idle: stays put", monster, 5, 5))
            return false;
        if (map.getAttr(5, 5) != ATTR_IDLE_MIN + 1)
        {
            LOGE("idle: attr cleared while player far away");
            return false;
        }

        g_game->player().move(5, 7); // distance 2: wakes, but idles this tick
        g_game->manageMonsters(TICKS_ALL);
        if (map.getAttr(5, 5) != 0)
        {
            LOGE("idle: attr not cleared with player in range");
            return false;
        }
        if (!expectPos("idle: idle tick stays put", monster, 5, 5))
            return false;

        g_game->manageMonsters(TICKS_ALL); // awake: wanders
        if (monster.pos() == Pos{5, 5})
        {
            LOGE("idle: monster never woke up");
            return false;
        }
        return true;
    }

    bool testSpeedGating()
    {
        resetWorld();
        const uint16_t tile = tileOf(TYPE_MONSTER, 2, AI_STICKY);
        CActor &monster = spawn(tile, TYPE_MONSTER, 5, 5, AIM_UP);

        g_game->manageMonsters(TICKS_NONE); // speeds[def.speed] is false
        if (!expectPos("speed gate: frozen tick", monster, 5, 5))
            return false;

        g_game->manageMonsters(TICKS_ALL);
        if (monster.pos() == Pos{5, 5})
        {
            LOGE("speed gate: monster stuck on an active tick");
            return false;
        }
        return true;
    }

    bool testMonsterContact()
    {
        resetWorld();
        const uint16_t tile = tileOf(TYPE_MONSTER, 2, AI_STICKY);
        CActor &monster = spawn(tile, TYPE_MONSTER, 5, 5, AIM_RIGHT);
        CGame::getMap().set(6, 5, TILES_ANNIE2); // player tile right next to it

        g_game->manageMonsters(TICKS_ALL); // damage + contact path, cannot enter player tile
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("contact: monster count %zu; expecting 1", g_game->getMonsters().size());
            return false;
        }
        if (monster.pos() == Pos{6, 5})
        {
            LOGE("contact: monster entered the player tile");
            return false;
        }
        return true;
    }

    bool testDrone()
    {
        resetWorld();
        const uint16_t tile = tileOf(TYPE_DRONE);
        CActor &drone = spawn(tile, TYPE_DRONE, 5, 5, AIM_RIGHT);

        g_game->manageMonsters(TICKS_ALL); // straight ahead is open
        if (!expectPos("drone: advances", drone, 6, 5))
            return false;

        CGame::getMap().set(7, 5, TILES_STOP);
        g_game->manageMonsters(TICKS_ALL); // blocked: flips aim, stays
        if (!expectPos("drone: blocked stays", drone, 6, 5) || drone.getAim() != AIM_LEFT)
        {
            LOGE("drone: aim %d at (%d,%d); expecting AIM_LEFT at (6,5)",
                 drone.getAim(), drone.x(), drone.y());
            return false;
        }

        g_game->manageMonsters(TICKS_ALL); // retreats the other way
        return expectPos("drone: reverses", drone, 5, 5);
    }

    bool testCrusher()
    {
        resetWorld();
        // vertical crusher: internal speed = (0xd0 & 3) + SPEED_VERYFAST = 2
        CActor &crusher = spawn(TILES_BLANK, ATTR_CRUSHERV_MIN, 5, 5, AIM_UP);

        g_game->manageMonsters(TICKS_NONE); // odd tick: speeds[2] false
        if (!expectPos("crusher: frozen tick", crusher, 5, 5))
            return false;

        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("crusher: advances", crusher, 5, 4))
            return false;

        CGame::getMap().set(5, 3, TILES_STOP);
        g_game->manageMonsters(TICKS_ALL); // blocked: flips to AIM_DOWN
        if (!expectPos("crusher: blocked stays", crusher, 5, 4) || crusher.getAim() != AIM_DOWN)
        {
            LOGE("crusher: aim %d at (%d,%d); expecting AIM_DOWN at (5,4)",
                 crusher.getAim(), crusher.x(), crusher.y());
            return false;
        }

        g_game->manageMonsters(TICKS_ALL);
        return expectPos("crusher: reverses", crusher, 5, 5);
    }

    bool testIceCube()
    {
        // falls one tile per active tick
        resetWorld();
        const uint16_t iceTile = tileOf(TYPE_ICECUBE);
        CActor &ice = spawn(iceTile, TYPE_ICECUBE, 5, 5, AIM_DOWN);
        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("icecube: falls", ice, 5, 6))
            return false;

        // pushes a boulder ahead of it (isPushable + pushChain)
        resetWorld();
        CActor &ice2 = spawn(iceTile, TYPE_ICECUBE, 5, 5, AIM_DOWN);
        const uint16_t boulderTile = tileOf(TYPE_BOULDER);
        CActor &boulder = spawn(boulderTile, TYPE_BOULDER, 5, 6, AIM_NONE);
        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("icecube: pushes icecube", ice2, 5, 6) ||
            !expectPos("icecube: pushes boulder", boulder, 5, 7))
            return false;

        // push chain blocked by an unpushable monster: everything stops
        resetWorld();
        CActor &ice3 = spawn(iceTile, TYPE_ICECUBE, 5, 5, AIM_DOWN);
        CActor &boulder3 = spawn(boulderTile, TYPE_BOULDER, 5, 6, AIM_NONE);
        spawn(tileOf(TYPE_MONSTER, 2, AI_STICKY), TYPE_MONSTER, 5, 7, AIM_UP);
        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("icecube: blocked stays", ice3, 5, 5) ||
            !expectPos("icecube: blocked boulder stays", boulder3, 5, 6) ||
            ice3.getAim() != AIM_NONE)
        {
            LOGE("icecube: blocked push at (%d,%d) aim %d", ice3.x(), ice3.y(), ice3.getAim());
            return false;
        }

        // bottom edge: aim cleared, no move
        resetWorld();
        CActor &ice4 = spawn(iceTile, TYPE_ICECUBE, 5, MAP_SIZE - 1, AIM_DOWN);
        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("icecube: edge stays", ice4, 5, MAP_SIZE - 1) || ice4.getAim() != AIM_NONE)
        {
            LOGE("icecube: edge aim %d", ice4.getAim());
            return false;
        }

        // isPushable table predicate
        if (!CGame::isPushable(TYPE_BOULDER) || !CGame::isPushable(TYPE_ICECUBE) ||
            CGame::isPushable(TYPE_MONSTER) || CGame::isPushable(TYPE_PLAYER))
        {
            LOGE("isPushable: wrong table result");
            return false;
        }
        return true;
    }
}

bool test_game_ai()
{
    struct
    {
        bool (*fct)();
        const char *name;
    } scenarios[] = {
        {testIdleAttr, "idle attr"},
        {testSpeedGating, "speed gating"},
        {testMonsterContact, "monster contact"},
        {testDrone, "drone"},
        {testCrusher, "crusher"},
        {testIceCube, "icecube"},
    };

    int failed = 0;
    for (const auto &s : scenarios)
    {
        if (!s.fct())
        {
            LOGE("game_ai scenario failed: %s", s.name);
            ++failed;
        }
    }

    // leave minimal singleton state for anything that still runs after us
    resetWorld();
    g_game->getMonsters().clear();
    g_game->rebuildMonsterGrid();
    CGame::getMap().resize(1, 1, TILES_BLANK, true);
    return failed == 0;
}
