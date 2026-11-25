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
#include "../../src/logger.h"
#include "../../src/assetman.h"
#include "../../src/bossdata.h"
#include "../../src/joyaim.h"
#include "../../src/sounds.h"
#include "../../src/tilesdata.h"
#include "../../src/gamesfx.h"
#include "../../src/sfx_private.h"
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

    const uint8_t types[] = {BOSS_MR_DEMON, BOSS_GHOST, BOSS_HARPY, BOSS_BEHOLDER,
                             BOSS_ARACHINE};
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

    // the shipped aims-3 boss: sheet 2, mirror left, and the block bases
    // pin the ini line order (walk, attack, death, damaged) to the
    // obLT records of the sheet png: 5/5/5/4 frames * 3 aims;
    // idle aliases moving via @moving
    const bossData_t *arachne = getBossData(BOSS_ARACHINE);
    if (!arachne || arachne->sheet != 2 || arachne->aims != 3 || arachne->mirror != AIM_LEFT)
    {
        LOGE("bossdata: arachine must load as sheet 2, aims 3, mirror left");
        return false;
    }
    if (arachne->moving.base != 0 || arachne->moving.lenght != 5 ||
        arachne->attack.base != 15 || arachne->death.base != 30 ||
        arachne->hurt.base != 45 ||
        arachne->idle.base != arachne->moving.base ||
        arachne->idle.lenght != arachne->moving.lenght)
    {
        LOGE("bossdata: arachine seq bases wrong: %d/%d/%d/%d/%d",
             arachne->moving.base, arachne->attack.base, arachne->death.base,
             arachne->hurt.base, arachne->idle.base);
        return false;
    }

    // sheet pin (bossdata.cpp.audit open item 3): a `>>>sheet N` line
    // between sections must NOT leak onto the previous boss -- Harpy is
    // flushed when `>>>sheet 1` + [Beholder] are seen, Beholder when
    // `>>>sheet 2` + [Arachine] are seen
    const bossData_t *harpy = getBossData(BOSS_HARPY);
    const bossData_t *beholder = getBossData(BOSS_BEHOLDER);
    if (!harpy || harpy->sheet != 0)
    {
        LOGE("bossdata: harpy must load on sheet 0, got %d", harpy ? harpy->sheet : -1);
        return false;
    }
    if (!beholder || beholder->sheet != 1)
    {
        LOGE("bossdata: beholder must load on sheet 1, got %d", beholder ? beholder->sheet : -1);
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
    // aims-3 sequence 'moving 4' => block of 4*3=12 rawIds; a box in the
    // phantom 4th physical block (rawId 13 = row 3, col 1) must be dropped
    // (plan boss-aims3-mirror §8), the rawId 0 box must survive
    write_file("hb_mirror4.json",
               R"({"frame":{"width":24,"height":32,"cols":4},"hitboxes":[)"
               R"({"x":0,"y":0,"w":8,"h":8,"type":1},)"
               R"({"x":24,"y":96,"w":8,"h":8,"type":2}]})");
    write_file("bosses.ini",
               "[ClampTest]\ntype 3\nhp 5\nmoving 2 hb_five.json\n"
               "[MissingKey]\ntype 4\nhp 5\nmoving 2 hb_missing.json\n"
               "[ScalarEntry]\ntype 5\nhp 5\nmoving 2 hb_scalar.json\n"
               "[BareWords]\nhp\ntype 6\nmoving # consumed by comment\n"
               "aims\n"
               "[DeepNest]\ntype 7\nhp fast\nmoving 2 hb_deep.json\n"
               "[RefTest]\ntype 8\nhp 5\nattack @nothere hb_five.json\nmoving 2\n"
               "[MirrorOK]\ntype 9\nhp 5\naims 3\nmirror left\nmoving 4 hb_mirror4.json\n"
               "healthbar_offset 32, -8\n"
               "[MirrorMissing]\ntype 10\nhp 5\naims 3\nmoving 4\n"
               "[MirrorUnused]\ntype 11\nhp 5\naims 4\nmirror right\nmoving 1\n");

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
    // the @ref line's hitbox file must be ignored (parse.py parity); the
    // MirrorOK file contributes its in-block frame only (§8 drops the rest)
    if (g_hitboxCount != 2)
    {
        LOGE("bossdata: expected 2 hitbox frames, got %d", g_hitboxCount);
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

    // aims 3 + mirror validation (plan docs/plans/boss-aims3-mirror-plan.md)
    {
        auto byName = [](const char *n) -> const bossData_t *
        {
            for (int i = 0; i < g_bossCount; ++i)
            {
                if (std::string(g_bossData[i].name) == n)
                    return &g_bossData[i];
            }
            return nullptr;
        };
        const bossData_t *ok = byName("MirrorOK");
        const bossData_t *missing = byName("MirrorMissing");
        const bossData_t *unused = byName("MirrorUnused");
        if (!ok || !missing || !unused)
        {
            LOGE("bossdata: mirror fixture boss missing after load");
            return false;
        }
        if (ok->aims != 3 || ok->mirror != AIM_LEFT)
        {
            LOGE("bossdata: MirrorOK aims=%d mirror=%d; expecting 3/AIM_LEFT",
                 ok->aims, ok->mirror);
            return false;
        }
        // healthbar_offset "32, -8": comma stays glued to token 1 through
        // tokenize(); negatives must survive (plan boss-healthbar-offset §4)
        if (ok->hpbar_off_x != 32 || ok->hpbar_off_y != -8)
        {
            LOGE("bossdata: MirrorOK healthbar_offset %d,%d; expecting 32,-8",
                 ok->hpbar_off_x, ok->hpbar_off_y);
            return false;
        }
        // absent key keeps the 0 defaults
        if (missing->hpbar_off_x != 0 || missing->hpbar_off_y != 0)
        {
            LOGE("bossdata: MirrorMissing healthbar_offset %d,%d; expecting 0,0",
                 missing->hpbar_off_x, missing->hpbar_off_y);
            return false;
        }
        // aims 3 without mirror: error at load, degraded to aims 1
        if (missing->aims != 1 || missing->mirror != -1)
        {
            LOGE("bossdata: MirrorMissing aims=%d mirror=%d; expecting 1/-1",
                 missing->aims, missing->mirror);
            return false;
        }
        // mirror declared with aims != 3: ignored
        if (unused->aims != 4 || unused->mirror != -1)
        {
            LOGE("bossdata: MirrorUnused aims=%d mirror=%d; expecting 4/-1",
                 unused->aims, unused->mirror);
            return false;
        }
        // sheet tally: MirrorOK block is 4*3=12 frames, MirrorMissing
        // (degraded) is 4*1=4; bases must reflect that exactly
        if (missing->moving.base - ok->moving.base != 12)
        {
            LOGE("bossdata: aims-3 block tallied %d frames; expecting 12",
                 missing->moving.base - ok->moving.base);
            return false;
        }
        if (unused->moving.base - missing->moving.base != 4)
        {
            LOGE("bossdata: degraded aims-3 block tallied %d frames; expecting 4",
                 unused->moving.base - missing->moving.base);
            return false;
        }
        // §8 out-of-block drop: MirrorOK's rawId 0 box survives in its own
        // block; its rawId 13 box (= missing->moving.base + 1) must NOT
        // have bled into MirrorMissing's frame ids
        if (getHitboxes(0, ok->moving.base) == nullptr)
        {
            LOGE("bossdata: MirrorOK in-block hitbox frame was dropped");
            return false;
        }
        if (getHitboxes(0, missing->moving.base + 1) != nullptr)
        {
            LOGE("bossdata: out-of-block hitbox bled into the next sequence");
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
    row.hitboxFile = "TEST_SKILL.json";
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

    // default_box: no JSON, box synthesized on the fx's LAST frame: bottom
    // rect of the table extents (FX_BLUE_LIGHTNING: 32x48 -> -16,16,32x8)
    {
        bossSkillDef_t drow;
        drow.name = "DEFAULT_BOX_TEST";
        drow.kind = SKILL_FX;
        drow.effectId = FX_BLUE_LIGHTNING;
        drow.default_box = true;
        g_skillDefs.push_back(drow);
        loadFxSkills(dir); // dir contents irrelevant: no hitboxFile
        const bossSkillDef_t &d = g_skillDefs.back();
        const int lastFrame = FX_BLUE_LIGHTNING_BASEID + FX_BLUE_LIGHTNING_FRAMES - 1;
        if (d.frames.size() != 1 || d.frames[0].spriteID != lastFrame ||
            d.frames[0].count != 1)
        {
            LOGE("fxskills: default_box frames wrong (n=%zu)", d.frames.size());
            ok = false;
        }
        else
        {
            const hitbox_t &b = d.frames[0].hitboxes[0];
            if (b.x != -16 || b.y != 16 || b.width != 32 || b.height != 8)
            {
                LOGE("fxskills: default_box rect (%d,%d,%d,%d) want (-16,16,32,8)",
                     b.x, b.y, b.width, b.height);
                ok = false;
            }
        }
        if (d.firstHitFrame != lastFrame || d.lastHitFrame != lastFrame)
        {
            LOGE("fxskills: default_box window %d..%d want %d..%d",
                 d.firstHitFrame, d.lastHitFrame, lastFrame, lastFrame);
            ok = false;
        }
    }

    g_skillDefs.resize(saved);
    fs::remove_all(dir);
    return ok;
}

// ini `skill NAME rate=N [tile= algo= sound= ttl=]` lines (plan fx-boss-skills
// §2/§3 + skill-bullet-payload §2/§4): accumulation, 4-slot cap, unknown-token
// skip, per-slot bullet payloads, tile/rate/blank-tile validity rules.
bool test_bossdata_skill_ini()
{
    namespace fs = std::filesystem;
    const std::string dir = "tests/out/bd_skill_ini";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir + "/bosses.ini");
        out << "[Multi]\ntype 1\nhp 5\nmoving 2\n"
            << "skill BULLET rate=10 tile=TILES_FIREBALL algo=Path::ASTAR sound=SOUND_SHOOT3 ttl=20\n"
            << "skill SPECIAL1 rate=30\n"
            << "skill LIGHTNING_STRIKE rate=30\n"
            << "skill METEOR rate=45\n"
            << "skill BULLET rate=99\n" // 5th line: cap, ignored
            << "[Bad]\ntype 2\nhp 5\nmoving 2\n"
            << "skill HOGBUF rate=7\n" // unknown: skipped
            << "skill METEOR rate=45\n"
            << "[None]\ntype 3\nhp 5\nmoving 2\n"
            << "bullet TILES_FIREBALL rate=15\n"
            << "[Two]\ntype 4\nhp 5\nmoving 2\n"
            << "skill BULLET rate=40 tile=TILES_BULLETY1 sound=SOUND_NONE ttl=NoTTL\n"
            << "skill BULLET rate=7 tile=TILES_FIREBALL algo=Path::LOS\n"
            << "[Inval]\ntype 5\nhp 5\nmoving 2\n"
            << "skill BULLET rate=12\n"                       // ruling: no tile=, skipped
            << "skill BULLET rate=5 tile=TILES_BLANK\n"       // ruling: blank tile, skipped
            << "skill BULLET rate=0 tile=TILES_FIREBALL\n";   // ruling: rate=0, skipped
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
        // retired `bullet` keyword: a legacy line must create no slots
        LOGE("skill-ini: retired bullet keyword applied (count %d)",
             n ? n->skillCount : -1);
        ok = false;
    }

    // plan skill-bullet-payload §4: merged keys round-trip into the slot
    if (m && m->skillCount == 4)
    {
        const bulletSpec_t &s = m->skills[0].bullet;
        if (s.tile != TILES_FIREBALL || s.algo != BossData::ASTAR ||
            s.sound != SOUND_SHOOT3 || s.ttl != 20)
        {
            LOGE("skill-ini: [Multi] slot0 payload (tile %d algo %d sound %d ttl %d) wrong",
                 s.tile, s.algo, s.sound, s.ttl);
            ok = false;
        }
    }

    // two BULLET slots carry independent payloads; legacy gate stays inert
    const bossData_t *t = getBossData(4);
    if (!t || t->skillCount != 2)
    {
        LOGE("skill-ini: [Two] skillCount %d want 2", t ? t->skillCount : -1);
        ok = false;
    }
    else
    {
        const bulletSpec_t &a = t->skills[0].bullet;
        const bulletSpec_t &b = t->skills[1].bullet;
        if (a.tile != TILES_BULLETY1 || a.ttl != BossData::NoTTL || a.algo != BossData::NONE ||
            b.tile != TILES_FIREBALL || b.algo != BossData::LOS || b.ttl != BossData::NoTTL)
        {
            LOGE("skill-ini: [Two] payloads not independent");
            ok = false;
        }
    }

    // ruling 2026-11-25: BULLET requires a non-blank tile= and rate>0;
    // invalid entries are skipped entirely
    const bossData_t *i = getBossData(5);
    if (!i || i->skillCount != 0)
    {
        LOGE("skill-ini: [Inval] expected invalid BULLET lines skipped, got count %d",
             i ? i->skillCount : -1);
        ok = false;
    }

    freeBossData();
    fs::remove_all(dir);
    return ok;
}

// real data/fx authoring (plan fx-boss-skills §11 step 6): loader fills the
// two SKILL_FX registry rows with one tile-accurate box per hit frame.
// Skip-guarded: CI has no private data tree.
bool test_bossdata_fx_data()
{
    namespace fs = std::filesystem;
    const std::string fxDir = AssetMan::getPrefix() + "fx"; // configurable like startup
    if (!fs::exists(fxDir + "/LIGHTNING_STRIKE.json"))
    {
        LOGI("fx data: data/fx absent, skipping");
        return true;
    }
    // snapshot the rows the loader may touch; restore at exit
    std::vector<std::pair<size_t, bossSkillDef_t>> saved;
    for (size_t i = 0; i < g_skillDefs.size(); ++i)
    {
        if (g_skillDefs[i].kind == SKILL_FX)
            saved.emplace_back(i, g_skillDefs[i]);
    }
    loadFxSkills(fxDir);

    bool ok = true;
    struct Expect
    {
        const char *name;
        int firstCol;
        int frameCount;
    } wants[] = {
        {"LIGHTNING_STRIKE", 2, 3}, // frames 2..4 of the 6-frame pillar
        {"METEOR", 3, 3},           // impact frames 3..5 of the 8-frame column
    };
    for (const Expect &w : wants)
    {
        const bossSkillDef_t *skill = nullptr;
        for (const auto &s : g_skillDefs)
            if (s.kind == SKILL_FX && s.name && std::strcmp(s.name, w.name) == 0)
                skill = &s;
        if (!skill)
        {
            LOGE("fx data: row %s missing", w.name);
            ok = false;
            continue;
        }
        if (static_cast<int>(skill->frames.size()) != w.frameCount ||
            skill->firstHitFrame != skill->baseFrame + w.firstCol ||
            skill->lastHitFrame != skill->baseFrame + w.firstCol + w.frameCount - 1)
        {
            LOGE("fx data: %s window %d..%d over %zu frames, want %d..%d",
                 w.name, skill->firstHitFrame, skill->lastHitFrame, skill->frames.size(),
                 skill->baseFrame + w.firstCol, skill->baseFrame + w.firstCol + w.frameCount - 1);
            ok = false;
            continue;
        }
        for (const sprite_hitbox_t &f : skill->frames)
        {
            // one 32x48-frame-centered box covering exactly the target 16px tile
            if (f.count != 1 || f.hitboxes[0].x != -8 || f.hitboxes[0].y != 8 ||
                f.hitboxes[0].width != 16 || f.hitboxes[0].height != 16 ||
                f.hitboxes[0].type != BossData::SPECIAL1)
            {
                LOGE("fx data: %s frame %d bad box (n=%d at %d,%d %dx%d t%d)",
                     w.name, f.spriteID, f.count, f.hitboxes[0].x, f.hitboxes[0].y,
                     f.hitboxes[0].width, f.hitboxes[0].height, f.hitboxes[0].type);
                ok = false;
            }
        }
    }
    for (const auto &s : saved)
        g_skillDefs[s.first] = s.second;
    return ok;
}
