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
#include "../../src/shared/FileMem.h"
#include "../../src/logger.h"
#include "../../src/boss.h"
#include "../../src/bossdata.h"
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
