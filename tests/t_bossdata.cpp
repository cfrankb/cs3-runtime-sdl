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
#include "../src/logger.h"
#include "../src/bossdata.h"
#include "t_bossdata.h"

// Production boss data lives in data/bosses (a symlink to the private
// asset repo), same paths as engine_hw.cpp. Skip when absent.
bool test_bossdata()
{
    constexpr const char *INI_DIR = "data/bosses";
    constexpr const char *HITBOX_DIR = "data/bosses/data";
    if (!std::filesystem::exists(std::string(INI_DIR) + "/bosses.ini"))
    {
        LOGI("bossdata: %s not present, skipping", INI_DIR);
        return true;
    }

    // a bad directory must fail without leaving stale state
    if (loadBossData("data/bosses-nonexistent", HITBOX_DIR) == 0)
    {
        LOGE("bossdata: load of nonexistent dir should fail");
        return false;
    }
    if (g_bossCount != 0)
    {
        LOGE("bossdata: failed load left g_bossCount=%d", g_bossCount);
        return false;
    }

    if (loadBossData(INI_DIR, HITBOX_DIR) != 0)
    {
        LOGE("bossdata: load of %s failed", INI_DIR);
        return false;
    }

    if (g_bossCount != BOSS_COUNT)
    {
        LOGE("bossdata: g_bossCount=%d; expecting %d", g_bossCount, BOSS_COUNT);
        return false;
    }

    const uint8_t types[] = {BOSS_MR_DEMON, BOSS_GHOST, BOSS_HARPY, BOSS_BEHOLDER};
    for (const uint8_t type : types)
    {
        const bossData_t *boss = getBossData(type);
        if (!boss)
        {
            LOGE("bossdata: no entry for type 0x%.2x", type);
            return false;
        }
        if (!boss->name || boss->name[0] == '\0' || boss->hp <= 0)
        {
            LOGE("bossdata: boss 0x%.2x has no name/hp", type);
            return false;
        }
        if (boss->type != type)
        {
            LOGE("bossdata: boss 0x%.2x reports type 0x%.2x", type, boss->type);
            return false;
        }
        if (boss->path > BossData::ASTAR_SMOOTH)
        {
            LOGE("bossdata: boss 0x%.2x has invalid path algo %u", type, boss->path);
            return false;
        }
    }

    // unknown types must not resolve
    if (getBossData(0x01) != nullptr)
    {
        LOGE("bossdata: getBossData(0x01) should be null");
        return false;
    }

    // hitbox accessor must find entries by (sheet, frame)
    if (g_hitboxCount <= 0)
    {
        LOGE("bossdata: no hitbox frames loaded");
        return false;
    }
    const sprite_hitbox_t &first = g_hitboxes[0];
    const int sheet = first.spriteID / BossData::SHEET_SPACER;
    const int frame = first.spriteID % BossData::SHEET_SPACER;
    const sprite_hitbox_t *probe = getHitboxes(sheet, frame);
    if (!probe || probe->spriteID != first.spriteID ||
        probe->count < 1 || probe->count > MAX_HITBOX_PER_FRAME)
    {
        LOGE("bossdata: getHitboxes(%d, %d) mismatch", sheet, frame);
        return false;
    }
    if (getHitboxes(sheet, -1) != nullptr)
    {
        LOGE("bossdata: bogus frame id matched a hitbox set");
        return false;
    }

    // freeBossData must reset the whole runtime state
    freeBossData();
    if (g_bossCount != 0 || g_hitboxCount != 0 || getBossData(BOSS_MR_DEMON) != nullptr)
    {
        LOGE("bossdata: freeBossData left stale state");
        return false;
    }

    return true;
}
