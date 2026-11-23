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
#include <vector>
#include "../../src/shared/FileMem.h"
#include "../../src/logger.h"
#include "../../src/boss.h"
#include "../../src/bossdata.h"
#include "../../src/map.h"
#include "t_boss.h"

bool test_boss()
{
    constexpr const char *INI_DIR = "data/bosses";
    if (!std::filesystem::exists(std::string(INI_DIR) + "/bosses.ini"))
    {
        LOGI("boss: %s not present, skipping", INI_DIR);
        return true;
    }
    if (g_bossCount == 0 && loadBossData(INI_DIR, "data/bosses/data") != 0)
    {
        LOGE("boss: loadBossData failed");
        return false;
    }
    const bossData_t *data = getBossData(BOSS_HARPY);
    if (!data)
    {
        LOGE("boss: no harpy data");
        return false;
    }

    CBoss boss(300, 200, data);
    boss.setHP(1234);
    boss.setSpeed(3);
    boss.setState(CBoss::Attack);
    boss.setAim(AIM_RIGHT);

    CFileMem mem;
    if (!mem.open("tests/out/boss.dat", "rw+"))
    {
        LOGE("boss: can't open boss.dat");
        return false;
    }
    if (!boss.write(mem) || !mem.seek(0))
    {
        LOGE("boss: write failed");
        mem.close();
        return false;
    }

    // constructed with the WRONG boss; read() must adopt the type from the stream
    CBoss clone(0, 0, getBossData(BOSS_MR_DEMON));
    if (!clone.read(mem))
    {
        LOGE("boss: read failed");
        mem.close();
        return false;
    }
    mem.close();

    struct Check
    {
        const char *what;
        long got;
        long want;
    };
    const Check checks[] = {
        {"x", clone.x(), boss.x()},
        {"y", clone.y(), boss.y()},
        {"type", clone.type(), boss.type()},
        {"hp", clone.hp(), boss.hp()},
        {"speed", clone.speed(), boss.speed()},
        {"state", clone.state(), boss.state()},
        {"aim", clone.getAim(), boss.getAim()},
    };
    for (const auto &c : checks)
    {
        if (c.got != c.want)
        {
            LOGE("CBoss.%s got %ld; expecting %ld", c.what, c.got, c.want);
            return false;
        }
    }

    // a stream with an unknown boss type must be rejected
    CFileMem bad;
    if (!bad.open("tests/out/boss_bad.dat", "rw+"))
    {
        LOGE("boss: can't open boss_bad.dat");
        return false;
    }
    const uint8_t bogus_type = 0x01;
    const bool rejected = [&]
    {
        if (bad.write(&bogus_type, sizeof(bogus_type)) != IFILE_OK || !bad.seek(0))
            return false;
        CBoss junk(0, 0, getBossData(BOSS_HARPY));
        return !junk.read(bad);
    }();
    bad.close();
    if (!rejected)
    {
        LOGE("boss: unknown boss type 0x01 was accepted");
        return false;
    }

    // save-field bounds are enforced now; an out-of-range byte must fail the load
    auto readRecord = [&](const uint8_t *rec, size_t len, const char *what)
    {
        CFileMem f;
        if (!f.open("tests/out/boss_field.dat", "rw+"))
        {
            LOGE("boss: can't open boss_field.dat");
            return false;
        }
        const bool ok = f.write(rec, len) == IFILE_OK && f.seek(0);
        CBoss junk(0, 0, getBossData(BOSS_HARPY));
        const bool accepted = ok && junk.read(f);
        f.close();
        if (accepted)
            LOGE("boss: %s was accepted", what);
        return !accepted;
    };
    uint8_t rec[] = {
        data->type, // 0:  boss type
        0, 0,       // 1:  framePtr
        10, 0,      // 3:  x
        20, 0,      // 5:  y
        100, 0,     // 7:  hp
        0x7f,       // 9:  state (>= MAX_STATES)
        1, 0,       // 10: speed
        0,          // 12: aim
        0, 0,       // 13: path size
        0, 0,       // 15: path index
        0, 0        // 17: path timeout
    };
    if (!readRecord(rec, sizeof(rec), "state 0x7f"))
        return false;
    rec[9] = CBoss::Hidden;
    rec[12] = AIM_NONE; // never-moved boss: legal, must still load
    {
        CFileMem f;
        if (!f.open("tests/out/boss_field.dat", "rw+"))
        {
            LOGE("boss: can't open boss_field.dat");
            return false;
        }
        const bool ok = f.write(rec, sizeof(rec)) == IFILE_OK && f.seek(0);
        CBoss good(0, 0, getBossData(BOSS_HARPY));
        const bool accepted = ok && good.read(f);
        f.close();
        if (!accepted)
        {
            LOGE("boss: AIM_NONE record was rejected");
            return false;
        }
    }

    std::filesystem::remove("tests/out/boss.dat");
    std::filesystem::remove("tests/out/boss_bad.dat");
    std::filesystem::remove("tests/out/boss_field.dat");
    return true;
}

/**
 * @brief aims-3 layouts (plan docs/plans/boss-aims3-mirror-plan.md M2):
 *        currentFrame() slot mapping, isMirrored(), legacy aims pinned,
 *        and per-frame hitbox reflection in boss space.
 */
bool test_boss_aims3()
{
    bossData_t d{};
    d.name = "aims3";
    d.type = 0;
    d.hp = 2;
    d.speed = 1;
    d.aims = 3;
    d.mirror = AIM_LEFT;
    d.moving.base = 100;
    d.moving.lenght = 4;

    CBoss boss(12, 12, &d);
    boss.setState(CBoss::Chase); // moving seq, framePtr reset to 0

    struct AimCase
    {
        JoyAim aim;
        int frame;
        bool mirrored;
    };
    const AimCase cases[] = {
        {AIM_UP, 100, false},
        {AIM_DOWN, 104, false},
        {AIM_LEFT, 108, true},  // derived from the shared horizontal block
        {AIM_RIGHT, 108, false}, // provided art
        {AIM_NONE, 100, false}, // never-moved boss normalises to UP
    };
    for (const AimCase &c : cases)
    {
        boss.setAim(c.aim);
        if (boss.currentFrame() != c.frame || boss.isMirrored() != c.mirrored)
        {
            LOGE("boss aims3: aim %.2x -> frame %d mirrored %d; expecting %d/%d",
                 c.aim, boss.currentFrame(), boss.isMirrored(), c.frame, c.mirrored);
            return false;
        }
    }

    // legacy layouts pinned (modulo path): no aim is ever mirrored
    const int legacyAims[] = {1, 2, 4};
    const int legacyWant[3][4] = {
        {100, 100, 100, 100}, // aims 1
        {100, 104, 100, 104}, // aims 2: LEFT->slot0, RIGHT->slot1
        {100, 104, 108, 112}, // aims 4: enum order
    };
    for (int a = 0; a < 3; ++a)
    {
        d.aims = legacyAims[a];
        d.mirror = -1;
        for (int aim = AIM_UP; aim <= AIM_RIGHT; ++aim)
        {
            boss.setAim(static_cast<JoyAim>(aim));
            const int want = legacyWant[a][aim];
            if (boss.currentFrame() != want || boss.isMirrored())
            {
                LOGE("boss legacy aims %d: aim %d -> frame %d mirrored %d; expecting %d/0",
                     d.aims, aim, boss.currentFrame(), boss.isMirrored(), want);
                return false;
            }
        }
    }
    d.aims = 3;

    // mirror right: LEFT is the physical art, RIGHT the flipped copy
    d.mirror = AIM_RIGHT;
    boss.setAim(AIM_RIGHT);
    if (!boss.isMirrored())
    {
        LOGE("boss aims3 mirror right: AIM_RIGHT must be mirrored");
        return false;
    }
    boss.setAim(AIM_LEFT);
    if (boss.isMirrored() || boss.currentFrame() != 108)
    {
        LOGE("boss aims3 mirror right: AIM_LEFT must be the physical block");
        return false;
    }
    d.mirror = AIM_LEFT;

    // --- mirrored per-frame hitbox (testHitbox1) ---
    // boss at half-tile (12,12); main hitbox 2x2 covers world tile (6,6).
    // authored box: x=6 w=2 -> world tile 9 on RIGHT, world tile 2 on LEFT
    d.hitbox = hitbox_t{0, 0, 2, 2, 0};
    CMap map;
    if (!map.resize(16, 16, 0, true))
    {
        LOGE("boss aims3: map resize failed");
        return false;
    }

    static sprite_hitbox_t hbStub;
    const sprite_hitbox_t *savedHb = g_hitboxes;
    const int savedCount = g_hitboxCount;
    boss.setAim(AIM_RIGHT); // LEFT and RIGHT share the physical frame here
    hbStub.spriteID = d.sheet * BossData::SHEET_SPACER + boss.currentFrame();
    hbStub.count = 1;
    hbStub.hitboxes[0] = hitbox_t{6, 0, 2, 2, static_cast<int>(BossData::HitBoxType::SPECIAL1)};
    g_hitboxes = &hbStub;
    g_hitboxCount = 1;

    auto specialCols = [&](JoyAim aim)
    {
        boss.setAim(aim);
        std::vector<int> cols;
        const auto hits = boss.testHitbox1(
            map,
            [](const Pos &, BossData::HitBoxType type)
            { return type == BossData::HitBoxType::SPECIAL1; },
            nullptr);
        for (const HitResult &h : hits)
            cols.push_back(h.pos.x);
        return cols;
    };

    bool ok = true;
    const std::vector<int> right = specialCols(AIM_RIGHT);
    const std::vector<int> left = specialCols(AIM_LEFT);
    if (right != std::vector<int>{9})
    {
        LOGE("boss aims3: RIGHT special box at tile %d; expecting 9",
             right.empty() ? -1 : right[0]);
        ok = false;
    }
    if (left != std::vector<int>{2})
    {
        LOGE("boss aims3: mirrored LEFT special box at tile %d; expecting 2",
             left.empty() ? -1 : left[0]);
        ok = false;
    }

    g_hitboxes = savedHb;
    g_hitboxCount = savedCount;
    return ok;
}
