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

#include <filesystem>

#include "../src/animzdata.h"
#include "../src/attr.h"
#include "../src/boss.h"
#include "../src/bossdata.h"
#include "../src/game.h"
#include "../src/gamesfx.h"
#include "../src/logger.h"
#include "../src/map.h"
#include "../src/sprtypes.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "t_game_ai.h"

// Phases 1-4 of docs/plans/archive/game_ai-test-plan.md: monster/boss AI driven
// through CGame::manageMonsters()/manageBosses()/manageSpawns(). Run LAST in
// tmain: it mutates the CGame singleton (monsters/bosses/player/map).

namespace
{
    constexpr int MAP_SIZE = 12;
    constexpr uint32_t TICKS_ALL = 840; // lcm(1..8): every speeds[] slot passes
    constexpr uint32_t TICKS_NONE = 841; // 29^2: only speeds[0..1] pass

    // mirrors the anonymous enum in game.cpp; the fuse scenario fails if it drifts
    constexpr int BARREL_TTL = 20;

    // mirrors GamePrivate::RESPAWN_DELAY in game_ai.cpp; 840 also passes the
    // (ticks & RESPAWN_SLICE) spawn gate
    constexpr int RESPAWN_DELAY = 20;

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
        g_game->getBosses().clear();
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

    // ---------------- Phase 2: interactions ----------------

    bool testMonsterV3()
    {
        resetWorld();
        const uint16_t tile = tileOf(TYPE_MONSTERV3);
        const TileDef &def = getTileDef(tile);
        if (!def.bullet || !def.rate || !def.range || !def.path)
        {
            LOGE("monsterV3: tile 0x%.2x lacks bullet/rate/range/path fields", tile);
            return false;
        }
        CActor &v3 = spawn(tile, TYPE_MONSTERV3, 5, 5, AIM_DOWN);
        g_game->player().move(5, 8); // distance 3 < range

        g_game->manageMonsters(16); // in range: startPath (follows one step)
        if (g_game->getMonsters().size() != 1 || v3.path() == nullptr ||
            v3.pos() == Pos{5, 5})
        {
            LOGE("monsterV3: startPath did not engage at (%d,%d)", v3.x(), v3.y());
            return false;
        }

        g_game->manageMonsters(24); // active tick, 24 % rate != 0: follow only, no fire
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("monsterV3: fired on a non-rate tick");
            return false;
        }

        g_game->manageMonsters(32); // active and 32 % rate == 0: spawnBullet ahead
        if (g_game->getMonsters().size() != 2)
        {
            LOGE("monsterV3: no bullet spawned on the rate tick (count %zu)",
                 g_game->getMonsters().size());
            return false;
        }
        if (g_game->getMonster(0).type() != TYPE_MONSTERV3)
        {
            LOGE("monsterV3: shooter missing after firing");
            return false;
        }

        // In the real game the player's cell is stamped with their tile
        // (loadLevel + CActor::move); the search must still reach the goal
        // instead of returning an empty path (dragon stuck firing).
        resetWorld();
        CActor &v3b = spawn(tile, TYPE_MONSTERV3, 5, 5, AIM_DOWN);
        g_game->player().move(5, 8); // distance 3 < range
        CGame::getMap().set(5, 8, TILES_ANNIE2);
        g_game->manageMonsters(16); // in range: startPath follows one step
        if (!expectPos("monsterV3: paths onto stamped player tile", v3b, 5, 6))
            return false;
        return true;
    }

    bool testVamPlant()
    {
        // propagates into adjacent swamp tile
        resetWorld();
        const uint16_t plantTile = tileOf(TYPE_VAMPLANT);
        spawn(plantTile, TYPE_VAMPLANT, 5, 5, AIM_DOWN);
        CGame::getMap().set(5, 4, tileOf(TYPE_SWAMP)); // g_dirs scans AIM_UP first
        g_game->manageMonsters(TICKS_ALL);
        if (g_game->getMonsters().size() != 2 ||
            CGame::getMap().at(5, 4) != TILES_VAMPLANT)
        {
            LOGE("vamplant: swamp not seeded (count %zu tile 0x%.2x)",
                 g_game->getMonsters().size(), CGame::getMap().at(5, 4));
            return false;
        }
        const int j = g_game->findMonsterAt(5, 4);
        if (j == CGame::INVALID || g_game->getMonster(j).type() != TYPE_VAMPLANT)
        {
            LOGE("vamplant: seeded plant missing from grid");
            return false;
        }

        // converts an adjacent monster into a plant
        resetWorld();
        spawn(plantTile, TYPE_VAMPLANT, 5, 5, AIM_DOWN);
        CActor &monster = spawn(tileOf(TYPE_MONSTER, 2, AI_STICKY), TYPE_MONSTER, 5, 6, AIM_UP);
        g_game->manageMonsters(TICKS_ALL);
        if (monster.type() != TYPE_VAMPLANT || CGame::getMap().at(5, 6) != TILES_VAMPLANT)
        {
            LOGE("vamplant: monster not converted (type 0x%.2x tile 0x%.2x)",
                 monster.type(), CGame::getMap().at(5, 6));
            return false;
        }
        return true;
    }

    bool testEggHatch()
    {
        resetWorld();
        const uint16_t eggTile = tileOf(TYPE_EGG);
        const uint16_t hatchTile = getTileDef(eggTile).bullet;
        if (!hatchTile)
        {
            LOGE("egg: tile 0x%.2x has no hatchling tile", eggTile);
            return false;
        }
        CActor &egg = spawn(eggTile, TYPE_EGG, 5, 5, AIM_UP);

        for (int i = 0; i < ANIMZ_EGG_HATCH_LEN - 1; ++i)
            g_game->manageMonsters(TICKS_ALL);
        if (egg.type() != TYPE_EGG)
        {
            LOGE("egg: hatched before frame %d", ANIMZ_EGG_HATCH_LEN);
            return false;
        }

        g_game->manageMonsters(TICKS_ALL); // final frame: hatch
        if (g_game->getMonsters().size() != 1 ||
            egg.type() != getTileDef(hatchTile).type ||
            CGame::getMap().at(5, 5) != hatchTile ||
            egg.getPU() != TILES_EGG_CRACKED)
        {
            LOGE("egg: hatch did not swap actor/map (0x%.2x / 0x%.2x)",
                 egg.type(), CGame::getMap().at(5, 5));
            return false;
        }
        return true;
    }

    bool testBullet()
    {
        // advances on open ground
        resetWorld();
        const uint16_t ballTile = tileOf(TYPE_FIREBALL);
        CActor &ball = spawn(ballTile, TYPE_FIREBALL, 5, 5, AIM_RIGHT);
        g_game->manageMonsters(TICKS_ALL);
        if (!expectPos("bullet: advances", ball, 6, 5))
            return false;

        // dies in front of a monster tile; the monster itself survives
        // (blastRadius, not bullets, is what deletes mob monsters)
        resetWorld();
        spawn(ballTile, TYPE_FIREBALL, 5, 5, AIM_RIGHT);
        spawn(tileOf(TYPE_MONSTER, 2, AI_STICKY), TYPE_MONSTER, 6, 5, AIM_UP);
        g_game->manageMonsters(TICKS_ALL);
        if (g_game->getMonsters().size() != 1 ||
            g_game->getMonsters().front().type() != TYPE_MONSTER)
        {
            LOGE("bullet vs monster: survivors %zu", g_game->getMonsters().size());
            return false;
        }

        // blocked by a boulder tile with no actor: dies, tile restored
        resetWorld();
        spawn(ballTile, TYPE_FIREBALL, 5, 5, AIM_RIGHT);
        CGame::getMap().set(6, 5, tileOf(TYPE_BOULDER));
        g_game->manageMonsters(TICKS_ALL);
        if (!g_game->getMonsters().empty() || CGame::getMap().at(5, 5) != TILES_BLANK)
        {
            LOGE("bullet vs boulder: not cleaned up");
            return false;
        }

        // range expiry: algo set and TTL exhausted -> dies even on open ground
        resetWorld();
        CActor &ball4 = spawn(ballTile, TYPE_FIREBALL, 5, 5, AIM_RIGHT);
        ball4.setAlgo(BossData::Path::ASTAR);
        ball4.setTTL(0);
        g_game->manageMonsters(TICKS_ALL);
        if (!g_game->getMonsters().empty() || CGame::getMap().at(6, 5) != TILES_BLANK)
        {
            LOGE("bullet ttl: not cleaned up");
            return false;
        }
        return true;
    }

    bool testBarrelBlast()
    {
        resetWorld();
        CActor &barrel = spawn(tileOf(TYPE_BARREL), TYPE_BARREL, 5, 5, AIM_UP);
        barrel.setTTL(1); // explodes on this next active tick
        const uint16_t monsterTile = tileOf(TYPE_MONSTER, 2, AI_STICKY);
        spawn(monsterTile, TYPE_MONSTER, 5, 3, AIM_UP); // blast offset (0,-2)
        spawn(monsterTile, TYPE_MONSTER, 6, 5, AIM_UP); // blast offset (1,0)
        spawn(monsterTile, TYPE_MONSTER, 8, 5, AIM_UP); // offset (3,0): outside radius 2

        g_game->manageMonsters(TICKS_ALL);
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("barrel blast: survivors %zu; expecting 1", g_game->getMonsters().size());
            return false;
        }
        if (CGame::getMap().at(5, 5) != TILES_BARREL2EX || g_game->getSfx().empty())
        {
            LOGE("barrel blast: tile 0x%.2x, %zu sfx",
                 CGame::getMap().at(5, 5), g_game->getSfx().size());
            return false;
        }
        return true;
    }

    bool testBarrelFuse()
    {
        resetWorld();
        spawn(tileOf(TYPE_BARREL), TYPE_BARREL, 6, 5, AIM_UP);
        spawn(tileOf(TYPE_ICECUBE), TYPE_ICECUBE, 6, 6, AIM_NONE); // static blast victim
        spawn(tileOf(TYPE_FIREBALL), TYPE_FIREBALL, 4, 5, AIM_RIGHT);

        g_game->manageMonsters(TICKS_ALL); // bullet advances to (5,5)
        const int b0 = g_game->findMonsterAt(6, 5);
        if (b0 == CGame::INVALID || g_game->getMonster(b0).getTTL() != CActor::NoTTL)
        {
            LOGE("barrel fuse: armed before the bullet arrived");
            return false;
        }

        g_game->manageMonsters(TICKS_ALL); // bullet dies on the barrel -> fuse lit
        if (g_game->getMonsters().size() != 2)
        {
            LOGE("barrel fuse: bullet not consumed (count %zu)", g_game->getMonsters().size());
            return false;
        }
        const int b1 = g_game->findMonsterAt(6, 5);
        if (b1 == CGame::INVALID || g_game->getMonster(b1).getTTL() != BARREL_TTL)
        {
            LOGE("barrel fuse: ttl %d; expecting %d",
                 b1 == CGame::INVALID ? -999 : g_game->getMonster(b1).getTTL(), BARREL_TTL);
            return false;
        }

        for (int i = 0; i < BARREL_TTL - 1; ++i) // burn down to ttl 1
            g_game->manageMonsters(TICKS_ALL);

        g_game->manageMonsters(TICKS_ALL); // ttl reaches 0: explodes

        if (!g_game->getMonsters().empty() ||
            CGame::getMap().at(6, 5) != TILES_BARREL2EX ||
            CGame::getMap().at(6, 6) != TILES_BLANK)
        {
            LOGE("barrel fuse: explosion cleanup (count %zu tiles 0x%.2x/0x%.2x)",
                 g_game->getMonsters().size(), CGame::getMap().at(6, 5), CGame::getMap().at(6, 6));
            return false;
        }
        return true;
    }

    // ---------------- Phase 3: spawns ----------------

    // Melt a respawn-flagged icecube with a bullet (the TYPE_ICECUBE death
    // path inside handleBullet is what reaches queueRespawn)
    bool queueRespawnViaMelt(const uint8_t attr)
    {
        resetWorld();
        CActor &ice = spawn(tileOf(TYPE_ICECUBE), TYPE_ICECUBE, 6, 5, AIM_NONE);
        ice.setAttr(attr);
        spawn(tileOf(TYPE_FIREBALL), TYPE_FIREBALL, 4, 5, AIM_RIGHT);
        g_game->manageMonsters(TICKS_ALL); // bullet advances to (5,5)
        g_game->manageMonsters(TICKS_ALL); // dies at the icecube: melts + queues
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawn setup: melt did not clear both actors");
            return false;
        }
        return true;
    }

    bool testSpawns()
    {
        // attr ATTR_RESPAWN_MIN+1 arms two respawns
        if (!queueRespawnViaMelt(ATTR_RESPAWN_MIN + 1))
            return false;

        for (int i = 0; i < RESPAWN_DELAY; ++i) // burn the initial delay
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: respawned before the delay elapsed");
            return false;
        }

        g_game->manageSpawns(TICKS_ALL); // delay reached 0: first respawn
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("spawns: no respawn after the delay (count %zu)", g_game->getMonsters().size());
            return false;
        }
        const CActor &born = g_game->getMonsters().front();
        const uint16_t iceTile = tileOf(TYPE_ICECUBE);
        if (born.type() != TYPE_ICECUBE || born.pos() != Pos{6, 5} ||
            CGame::getMap().at(6, 5) != iceTile)
        {
            LOGE("spawns: respawn misplaced (type 0x%.2x at (%d,%d) tile 0x%.2x)",
                 born.type(), born.x(), born.y(), CGame::getMap().at(6, 5));
            return false;
        }

        // pretend the monster wandered off so the cell frees up again
        g_game->getMonsters().clear();
        g_game->rebuildMonsterGrid();
        for (int i = 0; i < RESPAWN_DELAY; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: second respawn came too early");
            return false;
        }
        g_game->manageSpawns(TICKS_ALL);
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("spawns: missing second respawn (count %zu)", g_game->getMonsters().size());
            return false;
        }

        // attr's active count is spent: never respawns again
        g_game->getMonsters().clear();
        g_game->rebuildMonsterGrid();
        for (int i = 0; i < RESPAWN_DELAY * 2 + 1; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: respawned past the attr's active count");
            return false;
        }
        return true;
    }

    bool testSpawnsBlocked()
    {
        if (!queueRespawnViaMelt(ATTR_RESPAWN_MIN))
            return false;

        // an actor standing on the spawn point defers the spawn indefinitely
        spawn(tileOf(TYPE_ICECUBE), TYPE_ICECUBE, 6, 5, AIM_NONE);
        for (int i = 0; i < RESPAWN_DELAY * 2; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("spawns: spawned onto a blocked cell (count %zu)", g_game->getMonsters().size());
            return false;
        }

        // free cell: delay is already 0, so the next slice tick spawns
        g_game->getMonsters().clear();
        g_game->rebuildMonsterGrid();
        g_game->manageSpawns(TICKS_ALL);
        if (g_game->getMonsters().size() != 1 || g_game->findMonsterAt(6, 5) == CGame::INVALID)
        {
            LOGE("spawns: no respawn after the cell unblocked");
            return false;
        }
        return true;
    }

    // ---------------- Phase 4: bosses ----------------

    // real bossdata behind the same skip guard as t_boss.cpp
    const bossData_t *bossFixture()
    {
        constexpr const char *INI_DIR = "data/bosses";
        if (!std::filesystem::exists(std::string(INI_DIR) + "/bosses.ini"))
        {
            LOGI("boss: %s absent, skipping boss scenarios", INI_DIR);
            return nullptr;
        }
        if (g_bossCount == 0 && loadBossData(INI_DIR, "data/bosses/data") != 0)
        {
            LOGE("boss: loadBossData failed");
            return nullptr;
        }
        return g_bossCount > 0 ? g_bossData : nullptr;
    }

    const bossData_t *pickBoss(const bossData_t *all, std::function<bool(const bossData_t &)> pred)
    {
        for (int i = 0; i < g_bossCount; ++i)
            if (pred(all[i]))
                return &all[i];
        return nullptr;
    }

    // manageBosses dispatch: Hidden/Death skip, speed gate, patrol->chase
    // (the plan's TTL-removal item does not exist: CBoss::getTTL is a
    // constant NoTTL and manageBosses never erases bosses)
    bool testBossStates()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data = &all[0];
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(12, 12, data); // world tile (6,6)
        CBoss &boss = bosses.back();
        boss.setSpeed(1);
        g_game->player().move(6, 8); // distance 2
        boss.setState(CBoss::BossState::Hidden);
        g_game->manageBosses(1);
        if (boss.y() != 12)
        {
            LOGE("boss states: hidden boss moved to y %d", boss.y());
            return false;
        }
        boss.setState(CBoss::BossState::Death);
        g_game->manageBosses(1);
        if (boss.y() != 12)
        {
            LOGE("boss states: dead boss moved to y %d", boss.y());
            return false;
        }

        // speed gate: tick 1 fails a speed-4 gate, tick 4 passes and moves
        boss.setState(CBoss::BossState::Chase);
        boss.setSpeed(4);
        g_game->manageBosses(1);
        if (boss.y() != 12)
        {
            LOGE("boss states: boss moved off the speed gate (y %d)", boss.y());
            return false;
        }
        g_game->manageBosses(4);
        if (boss.y() == 12)
        {
            LOGE("boss states: boss did not move when the speed gate passed");
            return false;
        }

        // patrol engages chase within distance_chase
        boss.setState(CBoss::BossState::Patrol);
        boss.setSpeed(1);
        g_game->manageBosses(5);
        if (boss.state() != CBoss::BossState::Chase)
        {
            LOGE("boss states: patrol did not engage chase (state %d)", (int)boss.state());
            return false;
        }
        return true;
    }

    // Maintainer ruling 2026-11-18: barrel blast damages bosses even while
    // Hidden (off screen); blastRadius deliberately applies no state guard
    // and CBoss::sustainDamage's death guard keeps corpse hits as no-ops.
    bool testHiddenBossBlast()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(12, 10, &all[0]); // half-tile coords: world tile (6,5)
        CBoss &boss = bosses.back();
        boss.setState(CBoss::BossState::Hidden);
        const int hp0 = boss.hp();

        CActor &barrel = spawn(tileOf(TYPE_BARREL), TYPE_BARREL, 5, 5, AIM_UP);
        barrel.setTTL(1); // explodes next tick; blast radius 2 covers (6,5)

        g_game->manageMonsters(TICKS_ALL);
        if (boss.hp() >= hp0)
        {
            LOGE("hidden boss blast: hp %d -> %d; expecting blast damage", hp0, boss.hp());
            return false;
        }
        return true;
    }

    // handleBossPath: a chasing boss (path algo or fallback) closes on the player
    bool testBossPath()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data =
            pickBoss(all, [](const bossData_t &d)
                     { return d.path != BossData::Path::NONE && d.distance_pursuit >= 3; });
        if (!data)
        {
            LOGE("boss path: no bossdata with a path algo and pursuit >= 3");
            return false;
        }
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(12, 12, data); // half-tile coords: world tile (6,6)
        CBoss &boss = bosses.back();
        boss.setSpeed(1);
        boss.setState(CBoss::BossState::Chase);
        g_game->player().move(6, 9); // 3 tiles below
        const int16_t y0 = boss.y();
        g_game->manageBosses(1);
        // Maintainer ruling 2026-11-18: exactly one step per tick — the
        // greedy fallback is skipped when the path AI moved the boss.
        // (Un-gated double-move would advance 2 half-tiles.)
        if (boss.y() != y0 + 1)
        {
            LOGE("boss path: chasing boss advanced %d half-tiles in one tick (y %d -> %d); expecting exactly 1",
                 boss.y() - y0, y0, boss.y());
            return false;
        }
        return true;
    }

    // handleBossBullet: chasing boss fires its bullet tile on the rng cadence
    bool testBossBullet()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data =
            pickBoss(all, [](const bossData_t &d)
                     {
                         return d.bullet != TILES_BLANK && d.bullet_rate > 0 && d.hitbox.height <= 6 &&
                                d.distance_attack >= d.hitbox.height + 1 &&
                                d.distance_pursuit >= d.distance_attack;
                     });
        if (!data)
        {
            LOGE("boss bullet: no bossdata fires a bullet within reach of the map centre");
            return false;
        }
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(8, 8, data); // half-tile coords: world tile (4,4)
        CBoss &boss = bosses.back();
        boss.setSpeed(1);
        const int spawnY = 4 + data->hitbox.height; // AIM_DOWN spawn row
        g_game->player().move(4, spawnY + 1);
        // the rng is seeded by the tick; keep the boss pinned so the firing
        // window does not drift while handleBossPath chases the player
        for (uint32_t t = 1; t <= 400 && g_game->getMonsters().empty(); ++t)
        {
            boss.move(Pos{8, 8});
            boss.setState(CBoss::BossState::Chase);
            g_game->manageBosses(t);
        }
        if (g_game->getMonsters().empty())
        {
            LOGE("boss bullet: none fired within 400 ticks");
            return false;
        }
        const CActor &bullet = g_game->getMonsters().front();
        if (bullet.type() != getTileDef(data->bullet).type || bullet.x() != 4 || bullet.y() != spawnY)
        {
            LOGE("boss bullet: wrong actor (type 0x%.2x at (%d,%d), want 0x%.2x at (4,%d))",
                 bullet.type(), bullet.x(), bullet.y(), getTileDef(data->bullet).type, spawnY);
            return false;
        }
        return true;
    }

    // handleBossHitboxContact: player tile under the MAIN hitbox takes damage
    bool testBossHitboxContact()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data = pickBoss(
            all, [](const bossData_t &d)
            { return d.damage > 0 && d.damage < 100 && d.is_goal; });
        if (!data)
        {
            LOGE("boss contact: no goal bossdata with 0 < damage < 100");
            return false;
        }
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(12, 12, data); // MAIN hitbox covers world tile (6,6)
        // pending goal boss keeps checkClosure off after addHealth
        g_game->addHealth(100); // health boots at 0; make the hit observable
        g_game->getMap().set(6, 6, TILES_ANNIE2); // contact is map-tile driven
        const int before = g_game->health();
        g_game->manageBosses(1); // contact runs ahead of the speed gate
        if (g_game->health() >= before)
        {
            LOGE("boss contact: health %d unchanged from %d (primary damage %d)",
                 g_game->health(), before, data->damage);
            return false;
        }
        return true;
    }

    // canMove must gate every world tile the hitbox newly covers, not just
    // the anchor tile's row/column: a half-tile step must never slide the
    // hitbox's far edge into a blocking tile (icecube collision bug)
    bool testBossHalfTileEdge()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data = pickBoss(
            all, [](const bossData_t &d)
            { return d.hitbox.width >= 2 && d.hitbox.width % 2 == 0; });
        if (!data)
        {
            LOGI("boss half-step: no even-width bossdata, skipping");
            return true;
        }
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(4, 4, data); // anchor on an even half-tile coord
        CBoss &boss = bosses.back();
        CMap &map = CGame::getMap();
        if (!boss.canMove(JoyAim::AIM_RIGHT))
        {
            LOGE("boss half-step: free move rejected on a blank map");
            return false;
        }
        // wall off the tile the far edge would newly cover on one step right
        const int edgeTile = (4 + data->hitbox.width) / 2;
        if (edgeTile >= MAP_SIZE)
            return true;
        const uint16_t wall = tileOf(TYPE_WALLS);
        for (int ty = 4 / 2; ty <= (4 + data->hitbox.height - 1) / 2; ++ty)
            map.set(edgeTile, ty, wall);
        if (boss.canMove(JoyAim::AIM_RIGHT))
        {
            LOGE("boss half-step: canMove let the hitbox edge into solid tile %d", edgeTile);
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
        {testMonsterV3, "monsterV3"},
        {testVamPlant, "vamplant"},
        {testEggHatch, "egg hatch"},
        {testBullet, "bullet"},
        {testBarrelBlast, "barrel blast"},
        {testBarrelFuse, "barrel fuse"},
        {testSpawns, "spawns"},
        {testSpawnsBlocked, "spawns blocked"},
        {testBossStates, "boss states"},
        {testHiddenBossBlast, "hidden boss blast"},
        {testBossPath, "boss path"},
        {testBossBullet, "boss bullet"},
        {testBossHitboxContact, "boss hitbox contact"},
        {testBossHalfTileEdge, "boss half-tile edge"},
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
