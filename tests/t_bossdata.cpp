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
#include <fstream>
#include <string>
#include "../src/logger.h"
#include "../src/bossdata.h"
#include "../src/gamesfx.h"
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

// data/fx skill hitbox loader (plan fx-boss-skills §3): §0b derivation with
// center-anchor px conversion, sparse frame records, derived hit-window bounds.
bool test_bossdata_fxskills()
{
    namespace fs = std::filesystem;
    const std::string dir = "tests/out/fx_skills";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir + "/TEST_SKILL.json");
        out << R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":[)"
            << R"({"x":0,"y":0,"w":8,"h":8,"type":0},)"   // marker, dropped
            << R"({"x":48,"y":32,"w":16,"h":24,"type":2},)" // col2 row1 -> raw 6
            << R"({"x":60,"y":40,"w":8,"h":8,"type":2},)"   // same frame
            << R"({"x":72,"y":0,"w":4,"h":4,"type":1}]})";  // col3 row0 -> raw 3
    }

    const size_t saved = g_skillDefs.size();
    bossSkillDef_t row;
    row.name = "TEST_SKILL";
    row.kind = SKILL_FX;
    row.effectId = 4044;
    row.baseFrame = 10;
    g_skillDefs.push_back(row);

    loadFxSkills("nonexistent/fx/dir"); // missing dir: warnings only
    if (!g_skillDefs.back().frames.empty())
    {
        LOGE("fxskills: missing dir filled %zu frames", g_skillDefs.back().frames.size());
        g_skillDefs.resize(saved);
        return false;
    }

    loadFxSkills(dir);
    const bossSkillDef_t &skill = g_skillDefs.back();
    bool ok = true;
    // sparse: only the two frames with boxes get records, first-seen order
    if (skill.frames.size() != 2 || skill.frames[0].spriteID != 16 || skill.frames[1].spriteID != 13)
    {
        LOGE("fxskills: frame keying wrong (n=%zu)", skill.frames.size());
        ok = false;
    }
    else
    {
        const sprite_hitbox_t &f16 = skill.frames[0];
        if (f16.count != 2 || f16.hitboxes[0].x != -12 || f16.hitboxes[0].y != -16 ||
            f16.hitboxes[0].width != 16 || f16.hitboxes[0].height != 24 ||
            f16.hitboxes[0].type != 2 || f16.hitboxes[1].x != 0 || f16.hitboxes[1].y != -8)
        {
            LOGE("fxskills: frame 16 boxes wrong (n=%d b0=(%d,%d,%d,%d,%d))", f16.count,
                 f16.hitboxes[0].x, f16.hitboxes[0].y, f16.hitboxes[0].width,
                 f16.hitboxes[0].height, f16.hitboxes[0].type);
            ok = false;
        }
        const sprite_hitbox_t &f13 = skill.frames[1];
        if (f13.count != 1 || f13.hitboxes[0].x != -12 || f13.hitboxes[0].y != -16 ||
            f13.hitboxes[0].width != 4 || f13.hitboxes[0].type != 1)
        {
            LOGE("fxskills: frame 13 box wrong (n=%d)", f13.count);
            ok = false;
        }
    }
    if (skill.firstHitFrame != 13 || skill.lastHitFrame != 16)
    {
        LOGE("fxskills: window %d..%d want 13..16", skill.firstHitFrame, skill.lastHitFrame);
        ok = false;
    }
    // hand-authored rows with no JSON keep empty frames (never damage)
    if (skillForFx(FX_BLUE_LIGHTNING) && !skillForFx(FX_BLUE_LIGHTNING)->frames.empty())
    {
        LOGE("fxskills: LIGHTNING_STRIKE filled without a JSON");
        ok = false;
    }

    g_skillDefs.resize(saved);
    fs::remove_all(dir);
    return ok;
}

// ini `skill NAME rate=N` lines (plan fx-boss-skills §2/§3): accumulation,
// 4-slot cap, unknown-token skip, legacy bullet fields untouched.
bool test_bossdata_skill_ini()
{
    namespace fs = std::filesystem;
    const std::string dir = "tests/out/bd_skill_ini";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir + "/bosses.ini");
        out << "[Multi]\ntype 1\nhp 5\nmoving 2\n"
            << "skill BULLET rate=10\n"
            << "skill SPECIAL1 rate=30\n"
            << "skill LIGHTNING_STRIKE rate=30\n"
            << "skill METEOR rate=45\n"
            << "skill BULLET rate=99\n" // 5th line: cap, ignored
            << "[Bad]\ntype 2\nhp 5\nmoving 2\n"
            << "skill HOGBUF rate=7\n" // unknown: skipped
            << "skill METEOR rate=45\n"
            << "[None]\ntype 3\nhp 5\nmoving 2\n"
            << "bullet TILES_FIREBALL rate=15\n";
    }

    freeBossData();
    if (loadBossData(dir, dir) != 0)
    {
        LOGE("skill-ini: fixture load failed");
        return false;
    }

    auto idxOf = [](const char *name) -> int
    {
        for (size_t i = 0; i < g_skillDefs.size(); ++i)
            if (g_skillDefs[i].name && std::strcmp(g_skillDefs[i].name, name) == 0)
                return static_cast<int>(i);
        return -1;
    };

    bool ok = true;
    const bossData_t *m = getBossData(1);
    if (!m || m->skillCount != 4)
    {
        LOGE("skill-ini: [Multi] skillCount %d want 4", m ? m->skillCount : -1);
        ok = false;
    }
    else
    {
        const char *want[4] = {"BULLET", "SPECIAL1", "LIGHTNING_STRIKE", "METEOR"};
        const uint8_t rate[4] = {10, 30, 30, 45};
        for (int i = 0; i < 4; ++i)
        {
            if (m->skills[i].skill != idxOf(want[i]) || m->skills[i].rate != rate[i])
            {
                LOGE("skill-ini: [Multi] slot %d = (%d, %d) want (%d, %d)", i,
                     m->skills[i].skill, m->skills[i].rate, idxOf(want[i]), rate[i]);
                ok = false;
            }
        }
    }
    const bossData_t *b = getBossData(2);
    if (!b || b->skillCount != 1 || b->skills[0].skill != idxOf("METEOR"))
    {
        LOGE("skill-ini: [Bad] expected only METEOR slot, got count %d",
             b ? b->skillCount : -1);
        ok = false;
    }
    const bossData_t *n = getBossData(3);
    if (!n || n->skillCount != 0)
    {
        LOGE("skill-ini: [None] skillCount %d want 0", n ? n->skillCount : -1);
        ok = false;
    }
    if (n && (n->bullet_rate != 15 || n->bullet == 0))
    {
        LOGE("skill-ini: legacy bullet fields broken (rate %d)", n->bullet_rate);
        ok = false;
    }

    freeBossData();
    fs::remove_all(dir);
    return ok;
}
