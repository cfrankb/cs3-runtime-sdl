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

#include "../../src/animzdata.h"
#include "../../src/assetman.h"
#include "../../src/attr.h"
#include "../../src/boss.h"
#include "../../src/bossdata.h"
#include "../../src/game.h"
#include "../../src/gamestats.h"
#include "../../src/gamesfx.h"
#include "../../src/logger.h"
#include "../../src/map.h"
#include "../../src/sprtypes.h"
#include "../../src/tilesdata.h"
#include "../../src/tilesdefs.h"
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
    constexpr int RESPAWN_DELAY = 30;

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
        g_game->manageMonsters(24); // (5,7), adjacent from here on
        g_game->manageMonsters(32);
        g_game->manageMonsters(40);
        // must halt adjacent: never step onto the player's stamped cell
        if (!expectPos("monsterV3: halts before player cell", v3b, 5, 7) ||
            CGame::getMap().at(5, 8) != TILES_ANNIE2)
        {
            LOGE("monsterV3: overran player cell from (%d,%d)", v3b.x(), v3b.y());
            return false;
        }
        // chokepoint: shadowActorMove itself refuses the stamped player cell
        // (also covers the out-of-range wander route, findNextDir target)
        if (g_game->shadowActorMove(v3b, AIM_DOWN) ||
            !expectPos("monsterV3: shadowActorMove refuses player cell", v3b, 5, 7) ||
            CGame::getMap().at(5, 8) != TILES_ANNIE2)
        {
            LOGE("monsterV3: shadowActorMove stamped over the player");
            return false;
        }
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

    bool testBulletLOSTrack()
    {
        // harpy-style bullet: LOS algo, must home in on the player's cell.
        // The player's tile is stamped on the map at their cell; like the
        // MONSTERV3 goal fix, canMove must admit it or LineOfSight rejects
        // the whole path and the bolt stands still until its TTL expires.
        // On contact it must detonate (damage the player), not idle.
        resetWorld();
        g_game->addHealth(100); // health boots at 0; make the hit observable
        // blank test map has no diamonds/goal boss -> level "completed" ->
        // the addHealth above latched S_CLOSURE, which no-ops damage
        g_game->stats().set(S_CLOSURE, 0);
        const int base = g_game->health();
        g_game->player().move(8, 5);
        CGame::getMap().set(8, 5, TILES_ANNIE2); // player stamp
        const uint16_t boltTile = tileOf(TYPE_LIGHTNING_BOLT);
        CActor &bolt = spawn(boltTile, TYPE_LIGHTNING_BOLT, 5, 5, AIM_RIGHT);
        bolt.startPath(g_game->player().pos(), BossData::LOS, 20);

        g_game->manageMonsters(4); // speed-4 gate: 1st active tick -> (7,5)
        if (!expectPos("bullet LOS: homes in, stops at player edge", bolt, 7, 5))
            return false;

        // next active tick: contact detonation, while TTL is still 18
        g_game->manageMonsters(8);
        if (!g_game->getMonsters().empty())
        {
            LOGE("bullet LOS: not detonated on player contact (ttl still running)");
            return false;
        }
        if (g_game->health() >= base)
        {
            LOGE("bullet LOS: player health %d unchanged from %d", g_game->health(), base);
            return false;
        }
        if (CGame::getMap().at(8, 5) != TILES_ANNIE2)
        {
            LOGE("bullet LOS: stamped over the player tile (0x%.2x)", CGame::getMap().at(8, 5));
            return false;
        }

        // straight-shot bolt (no path algo) must likewise burst against the
        // player instead of stamping over the cell or idling
        spawn(boltTile, TYPE_LIGHTNING_BOLT, 5, 5, AIM_RIGHT);
        for (uint32_t t = 1; t <= 5; ++t)
            g_game->manageMonsters(t * 4);
        if (!g_game->getMonsters().empty())
        {
            LOGE("bullet straight: not detonated on player contact");
            return false;
        }
        if (CGame::getMap().at(8, 5) != TILES_ANNIE2)
        {
            LOGE("bullet straight: stamped over the player tile (0x%.2x)", CGame::getMap().at(8, 5));
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
        // (a departing actor restores the tile under it to its PU)
        g_game->getMonsters().clear();
        CGame::getMap().set(6, 5, TILES_BLANK);
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
        CGame::getMap().set(6, 5, TILES_BLANK);
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

        // free cell: the delay held during the block still has to elapse
        g_game->getMonsters().clear();
        g_game->rebuildMonsterGrid();
        for (int i = 0; i < RESPAWN_DELAY; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: respawn came too quickly after the block cleared");
            return false;
        }
        g_game->manageSpawns(TICKS_ALL);
        if (g_game->getMonsters().size() != 1 || g_game->findMonsterAt(6, 5) == CGame::INVALID)
        {
            LOGE("spawns: no respawn after the cell unblocked");
            return false;
        }
        return true;
    }

    bool testSpawnsPlayerBlocked()
    {
        if (!queueRespawnViaMelt(ATTR_RESPAWN_MIN))
            return false;

        // a player standing on the spawn point defers the spawn indefinitely
        g_game->player().move(6, 5);
        for (int i = 0; i < RESPAWN_DELAY * 2; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: spawned on top of the player (count %zu)", g_game->getMonsters().size());
            return false;
        }

        // player leaves: the delay held during the block still has to elapse
        g_game->player().move(8, 8);
        for (int i = 0; i < RESPAWN_DELAY; ++i)
            g_game->manageSpawns(TICKS_ALL);
        if (!g_game->getMonsters().empty())
        {
            LOGE("spawns: respawn came too quickly after the player left");
            return false;
        }
        g_game->manageSpawns(TICKS_ALL);
        if (g_game->findMonsterAt(6, 5) == CGame::INVALID)
        {
            LOGE("spawns: no respawn after the player left");
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

    // first declared BULLET slot with a usable payload (plan
    // skill-bullet-payload: bullets are skill slots now), nullptr if none
    static const bossSkill_t *bulletSlot(const bossData_t &d)
    {
        for (uint8_t i = 0; i < d.skillCount; ++i)
            if (g_skillDefs[d.skills[i].skill].kind == SKILL_BULLET &&
                d.skills[i].bullet.tile != TILES_BLANK && d.skills[i].rate)
                return &d.skills[i];
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

    // handleBossBullet: chasing boss fires its BULLET slot payload on the rng
    // cadence (plan skill-bullet-payload retire: no legacy `bullet` gate)
    bool testBossBullet()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *data =
            pickBoss(all, [](const bossData_t &d)
                     {
                         return bulletSlot(d) != nullptr && d.hitbox.height <= 6 &&
                                d.distance_attack >= d.hitbox.height + 1 &&
                                d.distance_pursuit >= d.distance_attack;
                     });
        if (!data)
        {
            LOGE("boss bullet: no bossdata declares a BULLET slot within reach of the map "
                 "centre (migrate data/bosses/bosses.ini to `skill BULLET` lines?)");
            return false;
        }
        const uint8_t wantType = getTileDef(bulletSlot(*data)->bullet.tile).type;
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
        if (bullet.type() != wantType || bullet.x() != 4 || bullet.y() != spawnY)
        {
            LOGE("boss bullet: wrong actor (type 0x%.2x at (%d,%d), want 0x%.2x at (4,%d))",
                 bullet.type(), bullet.x(), bullet.y(), wantType, spawnY);
            return false;
        }
        return true;
    }

    // §7 cast trigger: declared slots cast on the rng gate. FX slot spawns
    // the skill fx at the player tile (+telegraph iff telegraphFx >= 0),
    // rate 0 never casts; BULLET slot routes through handleBossBullet;
    // SPECIAL1 slot rate-gates the contact hit-test (no slot = unchanged).
    bool testBossSkillCast()
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return true;
        const bossData_t *base = pickBoss(
            all, [](const bossData_t &d)
            {
                return d.is_goal && bulletSlot(d) != nullptr &&
                       d.hitbox.height <= 6 && d.distance_pursuit >= 6;
            });
        if (!base)
        {
            LOGE("skill cast: no goal bossdata in fixture");
            return false;
        }
        int idxBullet = -1;
        int idxFx = -1;
        int idxSpecial1 = -1;
        for (size_t i = 0; i < g_skillDefs.size(); ++i)
        {
            if (!g_skillDefs[i].name)
                continue;
            const std::string n(g_skillDefs[i].name);
            if (n == "BULLET")
                idxBullet = static_cast<int>(i);
            else if (g_skillDefs[i].kind == SKILL_FX && idxFx < 0)
                idxFx = static_cast<int>(i); // first fx row (LIGHTNING_STRIKE)
            else if (g_skillDefs[i].kind == SKILL_SPECIAL1)
                idxSpecial1 = static_cast<int>(i);
        }
        if (idxBullet < 0 || idxFx < 0 || idxSpecial1 < 0)
        {
            LOGE("skill cast: registry rows missing");
            return false;
        }

        // --- FX slot: rate=1 casts every tick at the player ---
        bossData_t data = *base;
        data.skillCount = 1;
        data.skills[0].skill = static_cast<uint8_t>(idxFx);
        data.skills[0].rate = 1;
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(8, 8, &data); // world tile (4,4)
        CBoss &boss = bosses.back();
        boss.setSpeed(1);
        boss.setState(CBoss::BossState::Chase);
        g_game->player().move(4, 7);
        g_game->getFx().clear(); // resetWorld keeps fx alive
        g_game->manageBosses(1);
        if (g_game->pendingFx().size() != 1)
        {
            LOGE("skill cast: fx slot scheduled %zu, want 1", g_game->pendingFx().size());
            return false;
        }
        for (int i = 0; i < g_skillDefs[idxFx].telegraphTtl; ++i)
            g_game->tickFx(); // hint window elapses -> strike spawns
        const int wantFx = g_skillDefs[idxFx].effectId;
        int fxCount = 0;
        for (const fx_t &fx : g_game->getFx())
            fxCount += fx.effectID == wantFx;
        if (fxCount != 1)
        {
            LOGE("skill cast: fx slot spawned %d instances of %d, want 1", fxCount, wantFx);
            return false;
        }

        // telegraphFx >= 0 adds a second instance; reset keeps the table clean
        const int savedTele = g_skillDefs[idxFx].telegraphFx;
        g_skillDefs[idxFx].telegraphFx = g_skillDefs[idxFx].effectId; // placeholder art
        resetWorld();
        bosses.emplace_back(8, 8, &data);
        bosses.back().setSpeed(1);
        bosses.back().setState(CBoss::BossState::Chase);
        g_game->player().move(4, 7);
        g_game->getFx().clear(); // resetWorld keeps fx alive
        g_game->manageBosses(1);
        g_skillDefs[idxFx].telegraphFx = savedTele;
        for (int i = 0; i < g_skillDefs[idxFx].telegraphTtl; ++i)
            g_game->tickFx();
        int teleCount = 0;
        for (const fx_t &fx : g_game->getFx())
            teleCount += fx.effectID == wantFx;
        if (teleCount != 2)
        {
            LOGE("skill cast: telegraph row produced %d instances, want 2", teleCount);
            return false;
        }

        // --- rate 0: declared, never cast ---
        data.skills[0].rate = 0;
        resetWorld();
        bosses.emplace_back(8, 8, &data);
        bosses.back().setSpeed(1);
        bosses.back().setState(CBoss::BossState::Chase);
        g_game->player().move(4, 7);
        g_game->getFx().clear(); // resetWorld keeps fx alive
        g_game->manageBosses(1);
        if (!g_game->getFx().empty() || !g_game->pendingFx().empty())
        {
            LOGE("skill cast: rate=0 fx slot cast anyway");
            return false;
        }
        data.skills[0].rate = 1;

        // --- BULLET slot: casts through handleBossBullet. ---
        bossData_t bdata = *base;
        bdata.distance_attack = 1;
        bdata.skillCount = 1;
        // slots carry their own payload (plan skill-bullet-payload §3);
        // reuse the fixture boss's own BULLET slot payload
        bdata.skills[0] = *bulletSlot(*base);
        bdata.skills[0].skill = static_cast<uint8_t>(idxBullet);
        bdata.skills[0].rate = 1;
        const int by = 6;
        const int spawnY = by + bdata.hitbox.height; // AIM_DOWN spawn row
        resetWorld();
        bosses.emplace_back(12, 12, &bdata); // world tile (6,6)
        bosses.back().setSpeed(1);
        bosses.back().setState(CBoss::BossState::Chase);
        g_game->player().move(6, spawnY + 1);
        g_game->manageBosses(1);
        if (g_game->getMonsters().size() != 1)
        {
            LOGE("skill cast: BULLET slot fired %zu bullets, want 1",
                 g_game->getMonsters().size());
            return false;
        }
        // control: no slot, same setup -> no firing path, nothing fires
        bdata.skillCount = 0;
        resetWorld();
        bosses.emplace_back(12, 12, &bdata);
        bosses.back().setSpeed(1);
        bosses.back().setState(CBoss::BossState::Chase);
        g_game->player().move(6, spawnY + 1);
        g_game->manageBosses(1);
        if (!g_game->getMonsters().empty())
        {
            LOGE("skill cast: slot-less boss fired outside the legacy gate");
            return false;
        }

        // --- §5 pin (plan skill-bullet-payload): two BULLET slots cast
        // INDEPENDENT payloads. Both slots aim at the same tile, and
        // spawnBullet collides on the shared spawn tile, so a tick yields at
        // most one bullet; the pin is that BOTH payload tiles appear. ---
        bdata.skillCount = 2;
        bdata.skills[1] = bdata.skills[0];
        bdata.skills[1].bullet.tile = TILES_FIREBALL_SM;
        const uint8_t typeA = getTileDef(bdata.skills[0].bullet.tile).type;
        const uint8_t typeB = getTileDef(TILES_FIREBALL_SM).type;
        resetWorld();
        bosses.emplace_back(12, 12, &bdata);
        bosses.back().setSpeed(1);
        g_game->player().move(6, spawnY + 1);
        bool seenA = false;
        bool seenB = false;
        for (uint32_t t = 1; t <= 200 && !(seenA && seenB); ++t)
        {
            bosses.back().move(Pos{12, 12});
            bosses.back().setState(CBoss::BossState::Chase);
            g_game->manageBosses(t);
            for (const CActor &m : g_game->getMonsters())
            {
                seenA = seenA || m.type() == typeA;
                seenB = seenB || m.type() == typeB;
            }
            g_game->getMonsters().clear();
            g_game->rebuildMonsterGrid(); // resetWorld's seam: flush cleared actors
        }
        if (!seenA || !seenB)
        {
            LOGE("skill cast: BULLET slot payloads seen A=%d B=%d within 200 ticks",
                 seenA, seenB);
            return false;
        }

        // --- SPECIAL1 contact gate ---
        if (idxSpecial1 < 0)
        {
            LOGE("skill cast: SPECIAL1 row missing");
            return false;
        }
        const sprite_hitbox_t *savedHb = g_hitboxes;
        const int savedCount = g_hitboxCount;
        static sprite_hitbox_t hbTable; // referenced while the boss is alive
        const int s1Damage = 20;

        struct Case
        {
            uint8_t count;
            uint8_t rate;
            bool hurt;
        } cases[] = {
            {1, 1, true},   // rate=1: lands every tick
            {1, 0, false},  // rate=0: declared, never lands
            {0, 0, true},   // no slot: unthrottled legacy contact
        };
        for (const Case &c : cases)
        {
            bossData_t sdata = *base;
            sdata.damage = 0; // MAIN contact must not contribute
            sdata.damage_special1 = s1Damage;
            sdata.speed_anime = 3; // ticks=1: no animate() -> stable frame
            sdata.skillCount = c.count;
            if (c.count)
            {
                sdata.skills[0].skill = static_cast<uint8_t>(idxSpecial1);
                sdata.skills[0].rate = c.rate;
            }
            resetWorld();
            bosses.emplace_back(12, 12, &sdata); // MAIN covers world tile (6,6)
            CBoss &sboss = bosses.back();
            sboss.setState(CBoss::BossState::Attack); // nobody moves; frame key below must match this state
            hbTable.spriteID = sdata.sheet * BossData::SHEET_SPACER + sboss.currentFrame();
            hbTable.count = 1;
            hbTable.hitboxes[0] = hitbox_t{sdata.hitbox.x, sdata.hitbox.y,
                                           sdata.hitbox.width, sdata.hitbox.height,
                                           BossData::SPECIAL1};
            g_hitboxes = &hbTable;
            g_hitboxCount = 1;
            g_game->addHealth(100);
            g_game->getMap().set(6, 6, TILES_ANNIE2); // contact is map-tile driven
            const int before = g_game->health();
            g_game->manageBosses(1);
            const int after = g_game->health();
            if (c.hurt && after != before - s1Damage)
            {
                LOGE("skill cast: SPECIAL1 rate=%d dealt %d, want %d",
                     c.rate, before - after, s1Damage);
                g_hitboxes = savedHb;
                g_hitboxCount = savedCount;
                return false;
            }
            if (!c.hurt && after < before)
            {
                LOGE("skill cast: SPECIAL1 rate=%d dealt damage anyway", c.rate);
                g_hitboxes = savedHb;
                g_hitboxCount = savedCount;
                return false;
            }
        }
        g_hitboxes = savedHb;
        g_hitboxCount = savedCount;
        return true;
    }

    // end-to-end on real data (plan fx-boss-skills): ini skill lines +
    // data/fx hitboxes loaded -> cast trigger -> tickFx collision drops the
    // registry damage. MAIN/contact damage zeroed and per-frame boxes hidden
    // so the fx skill is the only damage source. Skip-guarded on data/.
    bool testBossSkillEndToEnd()
    {
        const bossData_t *all = bossFixture();
        if (!all || !std::filesystem::exists(AssetMan::getPrefix() + "fx/LIGHTNING_STRIKE.json"))
            return true;
        loadFxSkills(AssetMan::getPrefix() + "fx");
        const bossData_t *decl = pickBoss(
            all, [](const bossData_t &d)
            {
                for (uint8_t i = 0; i < d.skillCount; ++i)
                    if (g_skillDefs[d.skills[i].skill].kind == SKILL_FX)
                        return true;
                return false;
            });
        if (!decl)
        {
            LOGE("skill e2e: no boss in bosses.ini declares a SKILL_FX slot");
            return false;
        }
        const sprite_hitbox_t *savedHb = g_hitboxes;
        const int savedCount = g_hitboxCount;
        g_hitboxes = nullptr;
        g_hitboxCount = 0; // no authored per-frame boxes: MAIN contact only

        bossData_t data = *decl;
        data.damage = 0; // MAIN contact cannot hurt: fx skills are the only source
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(8, 8, &data); // world tile (4,4)
        bosses.back().setState(CBoss::BossState::Chase);
        g_game->player().move(4, 12); // 8 tiles: chase range, out of contact
        g_game->addHealth(100);
        const int base = g_game->health();
        bool fxSeen = false;
        int drop = 0;
        for (uint32_t t = 1; t <= 600; ++t)
        {
            g_game->manageBosses(t);
            for (const fx_t &fx : g_game->getFx())
                fxSeen = fxSeen || skillForFx(fx.effectID) != nullptr;
            g_game->tickFx();
            if (g_game->health() < base)
            {
                drop = base - g_game->health();
                break;
            }
        }
        g_hitboxes = savedHb;
        g_hitboxCount = savedCount;
        const int dmgA = g_skillDefs[2].damage; // LIGHTNING_STRIKE
        const int dmgB = g_skillDefs[3].damage; // METEOR
        if (!fxSeen || (drop != dmgA && drop != dmgB))
        {
            LOGE("skill e2e: fxSeen=%d first drop=%d want %d or %d", fxSeen, drop, dmgA, dmgB);
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
        {testBulletLOSTrack, "bullet LOS track"},
        {testBarrelBlast, "barrel blast"},
        {testBarrelFuse, "barrel fuse"},
        {testSpawns, "spawns"},
        {testSpawnsBlocked, "spawns blocked"},
        {testSpawnsPlayerBlocked, "spawns blocked by player"},
        {testBossStates, "boss states"},
        {testHiddenBossBlast, "hidden boss blast"},
        {testBossPath, "boss path"},
        {testBossBullet, "boss bullet"},
        {testBossHitboxContact, "boss hitbox contact"},
        {testBossSkillCast, "boss skill cast"},
        {testBossSkillEndToEnd, "boss skill end-to-end"},
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
