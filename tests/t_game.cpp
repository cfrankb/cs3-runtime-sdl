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
#include <filesystem>

#include "../src/boss.h"
#include "../src/bossdata.h"
#include "../src/events.h"
#include "../src/game.h"
#include "../src/gamestats.h"
#include "../src/gamesfx.h"
#include "../src/joyaim.h"
#include "../src/layer.h"
#include "../src/layerdata.h"
#include "../src/logger.h"
#include "../src/map.h"
#include "../src/maparch.h"
#include "../src/sprtypes.h"
#include "../src/statedata.h"
#include "../src/states.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "t_game.h"

// CGame rule tests for game.cpp: save signature validation, key inventory,
// sfx purge, scanPos/granular probing and level closure branches.
// Mutates the CGame singleton; leaves map/g_layerdata/keys restored.

namespace
{
    constexpr int MAP_SIZE = 12;

    // mirrors the file-local GAME_SIGNATURE/ENGINE_VERSION in game.cpp;
    // a save-format bump must update this (and the format docs) together
    constexpr char EXPECTED_SIGNATURE[5] = {'C', 'S', '3', 'b'};
    constexpr uint32_t EXPECTED_VERSION = (0x0200 << 16) + 0x000f;

    // mirrors the CLOSURE_TIMER constant in game.cpp
    constexpr int CLOSURE_TIMER = 7;

    CGame *g_game = nullptr;

    void resetWorld()
    {
        g_game = CGame::getGame();
        CMap &map = CGame::getMap();
        map.clear();
        map.resize(MAP_SIZE, MAP_SIZE, TILES_BLANK, true);
        g_game->getMonsters().clear();
        g_game->getBosses().clear();
        g_game->getSfx().clear();
        g_game->resetStats();
        g_game->clearEvents();
        g_game->player().move(MAP_SIZE - 1, MAP_SIZE - 1);
        g_game->rebuildMonsterGrid();
    }

    uint16_t tileOf(const uint8_t type)
    {
        for (int i = 0; i < TILES_TOTAL_COUNT; ++i)
        {
            if (getTileDef(i).type == type)
                return static_cast<uint16_t>(i);
        }
        LOGE("no tile with type 0x%.2x", type);
        return TILES_BLANK;
    }

    bool isClosed()
    {
        return g_game->stats().get(S_CLOSURE) != 0;
    }

    const bossData_t *bossFixture()
    {
        constexpr const char *INI_DIR = "data/bosses";
        if (!std::filesystem::exists(std::string(INI_DIR) + "/bosses.ini"))
        {
            LOGI("boss: %s absent, skipping closure scenarios", INI_DIR);
            return nullptr;
        }
        if (g_bossCount == 0 && loadBossData(INI_DIR, "data/bosses/data") != 0)
        {
            LOGE("boss: loadBossData failed");
            return nullptr;
        }
        return g_bossCount > 0 ? g_bossData : nullptr;
    }

    // Fresh world kept INCOMPLETE (pending goal boss) with the player alive.
    // addHealth() ends with checkClosure(), so a completed level here would
    // close before the scenario could observe the un-closed state.
    bool setupAlive(CBoss **bossOut)
    {
        const bossData_t *all = bossFixture();
        if (!all)
            return false;
        resetWorld();
        auto &bosses = g_game->getBosses();
        bosses.emplace_back(2, 2, &all[0]);
        CBoss &boss = bosses.back();
        if (!boss.isGoal())
        {
            LOGE("fixture boss 0 is not a goal boss; pick a different entry");
            return false;
        }
        g_game->stats().set(S_SHIELD, 0);
        g_game->addHealth(100);
        if (g_game->health() <= 0 || isClosed())
        {
            LOGE("setupAlive: health %d closed %d", g_game->health(), isClosed());
            return false;
        }
        *bossOut = &boss;
        return true;
    }

    bool testValidateSignature()
    {
        if (!CGame::validateSignature(EXPECTED_SIGNATURE, EXPECTED_VERSION))
        {
            LOGE("validateSignature: valid signature+version rejected");
            return false;
        }
        constexpr char badSig[5] = {'c', 's', '3', 'b'};
        if (CGame::validateSignature(badSig, EXPECTED_VERSION))
        {
            LOGE("validateSignature: bogus signature accepted");
            return false;
        }
        if (CGame::validateSignature(EXPECTED_SIGNATURE, EXPECTED_VERSION + 1))
        {
            LOGE("validateSignature: mismatched version accepted");
            return false;
        }
        return true;
    }

    bool testAddKey()
    {
        resetWorld();
        auto &keys = CGame::keys();
        memset(&keys, 0, sizeof(keys));
        constexpr uint8_t ids[CGame::MAX_KEYS] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
        for (const auto id : ids)
            g_game->addKey(id);
        for (uint32_t i = 0; i < CGame::MAX_KEYS; ++i)
        {
            if (!CGame::hasKey(ids[i]) || keys.tiles[i] != ids[i] ||
                keys.indicators[i] != CGame::MAX_KEY_STATE)
            {
                LOGE("addKey: slot %u tile 0x%.2x indicator %u",
                     i, keys.tiles[i], keys.indicators[i]);
                return false;
            }
        }
        g_game->addKey(ids[0]); // duplicate: inventory unchanged
        for (uint32_t i = 0; i < CGame::MAX_KEYS; ++i)
        {
            if (keys.tiles[i] != ids[i])
            {
                LOGE("addKey: duplicate 0x%.2x stored again at slot %u", ids[0], i);
                return false;
            }
        }
        g_game->addKey(0x7f); // full inventory: rejected, indicators stay in range
        if (CGame::hasKey(0x7f))
        {
            LOGE("addKey: 7th key accepted into a full inventory");
            return false;
        }
        for (uint32_t i = 0; i < CGame::MAX_KEYS; ++i)
        {
            if (keys.indicators[i] != CGame::MAX_KEY_STATE || keys.tiles[i] != ids[i])
            {
                LOGE("addKey: overflow clobbered slot %u (tile 0x%.2x indicator %u)",
                     i, keys.tiles[i], keys.indicators[i]);
                return false;
            }
        }
        memset(&keys, 0, sizeof(keys));
        return true;
    }

    bool testPurgeSfx()
    {
        resetWorld();
        auto &sfx = g_game->getSfx();
        sfx.push_back({1, 1, SFX_SPARKLE, 1});
        sfx.push_back({2, 2, SFX_SPARKLE, 2});
        sfx.push_back({3, 3, SFX_SPARKLE, 3});
        g_game->purgeSfx();
        if (sfx.size() != 2 || sfx[0].timeout != 1 || sfx[1].timeout != 2)
        {
            LOGE("purgeSfx: first purge left %zu effects", sfx.size());
            return false;
        }
        g_game->purgeSfx();
        if (sfx.size() != 1 || sfx[0].x != 3)
        {
            LOGE("purgeSfx: second purge left %zu effects", sfx.size());
            return false;
        }
        g_game->purgeSfx();
        if (!sfx.empty())
        {
            LOGE("purgeSfx: final purge left %zu effects", sfx.size());
            return false;
        }
        return true;
    }

    // scratch layer-tile ids in g_layerdata (unused by the animator by default)
    constexpr uint16_t ID_GRAIN = 0x43;

    struct LayerDataPatch
    {
        uint16_t idx;
        layerdata_t saved;
    };

    void patchLayerData(LayerDataPatch *patches, const uint16_t *ids, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            patches[i] = {ids[i], g_layerdata[ids[i]]};
            memset(&g_layerdata[ids[i]], 0, sizeof(layerdata_t));
        }
    }

    void restoreLayerData(const LayerDataPatch *patches, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            g_layerdata[patches[i].idx] = patches[i].saved;
    }

    bool expectScan(const char *what, const Pos &pos, const uint8_t aim,
                    const bool water, const bool solid, const bool deadly)
    {
        const scan_t s = g_game->scanPos(pos, aim);
        if (s.isWater != water || s.isSolid != solid || s.isDeadly != deadly)
        {
            LOGE("scanPos %s at (%d,%d): water %d solid %d deadly %d; "
                 "expecting water %d solid %d deadly %d",
                 what, pos.x, pos.y, s.isWater, s.isSolid, s.isDeadly,
                 water, solid, deadly);
            return false;
        }
        return true;
    }

    bool testScanPos()
    {
        resetWorld();
        CMap &map = CGame::getMap();
        const uint16_t swamp = tileOf(TYPE_SWAMP);
        if (getTileDef(swamp).type != TYPE_SWAMP)
            return false;

        constexpr uint16_t ids[] = {0, 0x40, 0x41, 0x42, ID_GRAIN}; // 0: empty layer tile
        LayerDataPatch patches[5];
        patchLayerData(patches, ids, 5);

        map.set(2, 2, swamp); // main layer only
        constexpr uint16_t idDeadly = 0x40;
        constexpr uint16_t idSolid = 0x41;
        constexpr uint16_t idWater = 0x42;
        g_layerdata[idDeadly].tileType = Deadly;
        g_layerdata[idSolid].tileType = Solid;
        g_layerdata[idWater].tileType = Water;

        CLayer *fx = map.addLayer(CLayer::LAYER_DECOR, "probe", 1);
        fx->set(3, 3, idDeadly);
        fx->set(4, 4, idSolid);
        fx->set(5, 5, idWater);
        fx->set(7, 7, idDeadly);
        map.set(7, 7, swamp);

        bool ok = expectScan("swamp", {2, 2}, AIM_UP, true, false, false) &&
                  expectScan("deadly", {3, 3}, AIM_UP, false, false, true) &&
                  expectScan("solid", {4, 4}, AIM_UP, false, true, false) &&
                  expectScan("water", {5, 5}, AIM_UP, true, false, false) &&
                  expectScan("blank", {1, 1}, AIM_UP, false, false, false) &&
                  expectScan("swamp+deadly", {7, 7}, AIM_UP, true, false, true);

        // granular quadrant gate: solid only when the two facing quadrants are set
        fx->set(6, 6, ID_GRAIN);
        struct
        {
            uint8_t aim;
            uint8_t pair;
        } cases[] = {
            {AIM_UP, GranualarDL | GranualarDR},
            {AIM_DOWN, GranualarUL | GranualarUR},
            {AIM_LEFT, GranualarUR | GranualarDR},
            {AIM_RIGHT, GranualarUL | GranualarDL}};
        for (const auto &c : cases)
        {
            g_layerdata[ID_GRAIN].granular = c.pair;
            ok = expectScan("granular pair", {6, 6}, c.aim, false, true, false) && ok;
            g_layerdata[ID_GRAIN].granular = GranualarUL;
            ok = expectScan("granular half", {6, 6}, c.aim, false, false, false) && ok;
        }

        map.popLayer();
        restoreLayerData(patches, 5);
        return ok;
    }

    bool testGranularSolid()
    {
        resetWorld();
        CMap &map = CGame::getMap();
        constexpr uint16_t ids[] = {0, ID_GRAIN};
        LayerDataPatch patches[2];
        patchLayerData(patches, ids, 2);
        g_layerdata[ID_GRAIN].granular = GranualarUL | GranualarUR | GranualarDL | GranualarDR;

        bool ok = true;
        struct
        {
            uint8_t aim;
            uint8_t pair;
        } cases[] = {
            {AIM_UP, GranualarUL | GranualarUR},
            {AIM_DOWN, GranualarDL | GranualarDR},
            {AIM_LEFT, GranualarUL | GranualarDL},
            {AIM_RIGHT, GranualarUR | GranualarDR}};

        CLayer *fx = map.addLayer(CLayer::LAYER_DECOR, "grain", 1);
        fx->set(4, 4, ID_GRAIN);
        for (const auto &c : cases)
        {
            if (!g_game->isGranularSolidFromPos({4, 4}, c.aim))
            {
                LOGE("granularSolid: aim %u not solid with all quadrants set", c.aim);
                ok = false;
            }
            g_layerdata[ID_GRAIN].granular = c.pair & (c.pair - 1); // drop one bit
            if (g_game->isGranularSolidFromPos({4, 4}, c.aim))
            {
                LOGE("granularSolid: aim %u solid with pair 0x%.2x broken", c.aim, c.pair);
                ok = false;
            }
            g_layerdata[ID_GRAIN].granular = GranualarUL | GranualarUR | GranualarDL | GranualarDR;
        }
        map.popLayer();

        // main layer (baseID 0) is never probed for granularity
        map.set(6, 6, ID_GRAIN);
        if (g_game->isGranularSolidFromPos({6, 6}, AIM_UP))
        {
            LOGE("granularSolid: main layer tile reported granular-solid");
            ok = false;
        }

        restoreLayerData(patches, 2);
        return ok;
    }

    // a hit with 2 shields must still cost health; casting the float damage
    // factor to int truncated it to 0, which made shields total immunity
    bool testShieldDamage()
    {
        CBoss *boss = nullptr;
        if (!setupAlive(&boss))
            return true; // fixture absent: skip
        g_game->stats().set(S_SHIELD, 2);
        const int before = g_game->health();
        g_game->addHealth(-10);
        const int after = g_game->health();
        if (after >= before)
        {
            LOGE("shield damage: health %d -> %d with 2 shields", before, after);
            return false;
        }
        return true;
    }

    bool testClosureDeath()
    {
        CBoss *boss = nullptr;
        if (!setupAlive(&boss))
            return true; // fixture absent: skip
        g_game->addHealth(-1000);
        if (g_game->health() != 0)
        {
            LOGE("closure/death: health %d after lethal hit", g_game->health());
            return false;
        }
        if (!isClosed() || g_game->closusureTimer() != CLOSURE_TIMER)
        {
            LOGE("closure/death: closure %d timer %d", isClosed(), g_game->closusureTimer());
            return false;
        }
        return true;
    }

    bool testClosureLevelComplete()
    {
        CBoss *boss = nullptr;
        if (!setupAlive(&boss))
            return true;
        if (isClosed() || g_game->isLevelCompleted())
        {
            LOGE("closure/pending: level closed/completed with a pending goal boss");
            return false;
        }
        boss->setState(CBoss::BossState::Hidden); // goal boss down: level completed
        g_game->checkClosure();
        if (!isClosed() || g_game->closusureTimer() != CLOSURE_TIMER)
        {
            LOGE("closure/complete: closure %d timer %d", isClosed(), g_game->closusureTimer());
            return false;
        }
        if (g_game->getEvent() != EVENT_NONE)
        {
            LOGE("closure/complete: unexpected event without an exit");
            return false;
        }
        return true;
    }

    bool testClosureExitGate()
    {
        CBoss *boss = nullptr;
        if (!setupAlive(&boss))
            return true;
        boss->setState(CBoss::BossState::Hidden);
        CMap &map = CGame::getMap();
        constexpr int exitX = 3;
        constexpr int exitY = 4;
        map.states().setU(POS_EXIT, CMap::toKey(exitX, exitY));

        g_game->checkClosure(); // player not at the exit yet
        if (isClosed())
        {
            LOGE("closure/exit: closed while the player is away from the exit");
            return false;
        }
        if (g_game->getEvent() != EVENT_EXIT_OPENED)
        {
            LOGE("closure/exit: EVENT_EXIT_OPENED not queued");
            return false;
        }
        if (map.get(exitX, exitY) != TILES_DOORS_LEAF || g_game->stats().get(S_REVEAL_EXIT) != 1)
        {
            LOGE("closure/exit: exit tile 0x%.2x reveal %d",
                 map.get(exitX, exitY), g_game->stats().get(S_REVEAL_EXIT));
            return false;
        }
        g_game->checkClosure(); // already revealed, player still away
        if (isClosed())
        {
            LOGE("closure/exit: closed on the repeat check away from the exit");
            return false;
        }
        g_game->player().move(exitX, exitY);
        g_game->checkClosure();
        if (!isClosed())
        {
            LOGE("closure/exit: not closed once the player reached the exit");
            return false;
        }
        return true;
    }

    bool testClosureChute()
    {
        CBoss *boss = nullptr;
        if (!setupAlive(&boss))
            return true;
        g_game->player().move(5, 6);
        g_game->stats().set(S_CHUTE, 1);
        g_game->checkClosure();
        if (!isClosed())
        {
            LOGE("closure/chute: S_CHUTE did not trigger closure");
            return false;
        }
        return true;
    }
// BUG-C7: loadLevel must reject a null archive or an out-of-range level
// instead of dereferencing at()'s nullptr. Mutates and restores the
// singleton's arch/level/mode; no archive is set anywhere else in cs3-tests.
bool testLevelBounds()
{
    CGame *game = CGame::getGame();
    const int savedLevel = game->level();
    const CGame::GameMode savedMode = game->mode();
    bool ok = true;

    game->setMapArch(nullptr);
    game->setLevel(0);
    if (game->loadLevel(CGame::MODE_IDLE))
    {
        LOGE("loadLevel accepted a null archive");
        ok = false;
    }

    CMapArch arch;
    arch.add(std::make_unique<CMap>(MAP_SIZE, MAP_SIZE));
    game->setMapArch(&arch);
    game->setLevel(1);
    if (game->loadLevel(CGame::MODE_IDLE))
    {
        LOGE("loadLevel accepted level 1 of a 1-map archive");
        ok = false;
    }
    game->setLevel(-1);
    if (game->loadLevel(CGame::MODE_IDLE))
    {
        LOGE("loadLevel accepted negative level");
        ok = false;
    }

    game->setMapArch(nullptr);
    game->setLevel(savedLevel);
    game->setMode(savedMode);
    return ok;
}
} // namespace

bool test_game()
{
    return testValidateSignature() &&
           testAddKey() &&
           testPurgeSfx() &&
           testScanPos() &&
           testGranularSolid() &&
           testClosureDeath() &&
           testShieldDamage() &&
           testClosureLevelComplete() &&
           testClosureExitGate() &&
           testClosureChute() &&
           testLevelBounds();
}
