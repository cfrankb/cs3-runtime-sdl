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
#include <string>
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

/**
 * @brief BUG-C5: malformed hitbox JSON and bare INI keywords must be
 *        rejected with warnings, not null-deref / vector OOB; recorded
 *        hitbox counts must never exceed MAX_HITBOX_PER_FRAME.
 *
 * @return true
 * @return false
 */
bool test_bossdata_hostile()
{
    namespace fs = std::filesystem;
    const std::string dir = "tests/out/bd_hostile";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    auto write_file = [&dir](const char *name, const std::string &content)
    {
        std::ofstream out(dir + "/" + name);
        out << content;
    };

    // five boxes in the same cell of one frame -> destination array holds 4
    write_file("hb_five.json",
               R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":[)"
               R"({"x":0,"y":0,"w":8,"h":8,"type":1},)"
               R"({"x":1,"y":1,"w":8,"h":8,"type":2},)"
               R"({"x":2,"y":2,"w":8,"h":8,"type":3},)"
               R"({"x":3,"y":3,"w":8,"h":8,"type":1},)"
               R"({"x":4,"y":4,"w":8,"h":8,"type":2}]})");
    // hitbox entry missing "x" (pre-fix: null deref)
    write_file("hb_missing.json",
               R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":[{"y":0,"w":8,"h":8,"type":1}]})");
    // non-object hitbox entries (pre-fix: null deref via hb.find on Number)
    write_file("hb_scalar.json",
               R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":[7]})");
    // 300-deep JSON array nesting: parser depth cap (64) must reject the
    // file, not overflow the recursive-descent stack
    write_file("hb_deep.json",
               R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":)"
               + std::string(300, '[') + std::string(300, ']') + "}");
    write_file("bosses.ini",
               "[ClampTest]\ntype 3\nhp 5\nmoving 2 hb_five.json\n"
               "[MissingKey]\ntype 4\nhp 5\nmoving 2 hb_missing.json\n"
               "[ScalarEntry]\ntype 5\nhp 5\nmoving 2 hb_scalar.json\n"
               "[BareWords]\nhp\ntype 6\nmoving # consumed by comment\n"
               "aims\n"
               "[DeepNest]\ntype 7\nhp fast\nmoving 2 hb_deep.json\n"
               "[RefTest]\ntype 8\nhp 5\nattack @nothere hb_five.json\nmoving 2\n");

    freeBossData();
    if (loadBossData(dir.c_str(), dir.c_str()) != 0)
    {
        LOGE("bossdata: hostile fixture load should survive with warnings");
        return false;
    }
    if (g_bossCount < 1)
    {
        LOGE("bossdata: hostile fixture produced no bosses");
        return false;
    }

    bool saw_clamped = false;
    for (int i = 0; i < g_hitboxCount; ++i)
    {
        if (g_hitboxes[i].count < 0 || g_hitboxes[i].count > MAX_HITBOX_PER_FRAME)
        {
            LOGE("bossdata: hitbox entry %d count %d exceeds cap", i, g_hitboxes[i].count);
            return false;
        }
        if (g_hitboxes[i].count == MAX_HITBOX_PER_FRAME)
            saw_clamped = true;
    }
    if (!saw_clamped)
    {
        LOGE("bossdata: 5-box frame did not clamp to %d", MAX_HITBOX_PER_FRAME);
        return false;
    }
    // malformed/deep-nested JSON files must have contributed nothing, and
    // the @ref line's hitbox file must be ignored (parse.py parity)
    if (g_hitboxCount != 1)
    {
        LOGE("bossdata: expected 1 hitbox frame, got %d", g_hitboxCount);
        return false;
    }
    // garbage hp ("hp fast") must be flagged and normalized to 0 at load
    {
        bool found = false;
        for (int i = 0; i < g_bossCount; ++i)
        {
            if (std::string(g_bossData[i].name) == "DeepNest")
            {
                found = true;
                if (g_bossData[i].hp != 0)
                {
                    LOGE("bossdata: garbage hp survived as %d", g_bossData[i].hp);
                    return false;
                }
            }
        }
        if (!found)
        {
            LOGE("bossdata: DeepNest fixture boss missing after hostile load");
            return false;
        }
    }
    // garbage/missing aims (BareWords fixture) must be normalized to >= 1
    // at load so CBoss::currentFrame() can never divide by zero
    for (int i = 0; i < g_bossCount; ++i)
    {
        if (g_bossData[i].aims < 1)
        {
            LOGE("bossdata: boss '%s' aims %d not normalized at load",
                 g_bossData[i].name, g_bossData[i].aims);
            return false;
        }
    }

    freeBossData();
    return true;
}
