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
#include "../../src/shared/FileWrap.h"
#include "../../src/shared/FileMem.h"
#include "../../src/logger.h"
#include "../../src/actor.h"
#include "../../src/ai_path.h"
#include "../../src/bossdata.h"
#include "t_actor.h"

bool test_actor()
{
    constexpr const char *OUT_FILE = "tests/out/actor.dat";

    CActor actor(10, 20, 0x31, AIM_LEFT);
    actor.setPU(0x77);
    actor.setAttr(0x40);
    actor.setAlgo(BossData::ASTAR);
    actor.setFrame(7); // must survive round-trip independently of algo
    actor.setTTL(99);
    actor.setOrigin(Pos{1, 2});

    CFileWrap tfile;
    if (!tfile.open(OUT_FILE, "wb"))
    {
        LOGE("can't write %s", OUT_FILE);
        return false;
    }
    if (!actor.write(tfile))
    {
        LOGE("failed to serialize CActor");
        return false;
    }
    tfile.close();

    // a fresh actor without path should round-trip without one
    if (actor.path() != nullptr)
    {
        LOGE("unexpected path on fresh actor");
        return false;
    }

    CFileWrap sfile;
    if (!sfile.open(OUT_FILE, "rb"))
    {
        LOGE("can't read %s", OUT_FILE);
        return false;
    }
    CActor clone(0, 0, 0, AIM_UP);
    if (!clone.read(sfile))
    {
        LOGE("failed to deserialize CActor");
        return false;
    }
    sfile.close();

    struct Check
    {
        const char *what;
        long got;
        long want;
    };
    const Check checks[] = {
        {"x", clone.x(), actor.x()},
        {"y", clone.y(), actor.y()},
        {"type", clone.type(), actor.type()},
        {"aim", clone.getAim(), actor.getAim()},
        {"pu", clone.getPU(), actor.getPU()},
        {"algo", clone.algo(), actor.algo()},
        {"frame", clone.frame(), actor.frame()},
        {"ttl", clone.getTTL(), actor.getTTL()},
        {"attr", clone.attr(), actor.attr()},
        {"origin.x", clone.origin().x, actor.origin().x},
        {"origin.y", clone.origin().y, actor.origin().y},
        {"path defined", clone.path() != nullptr, actor.path() != nullptr},
    };
    for (const auto &c : checks)
    {
        if (c.got != c.want)
        {
            LOGE("CActor.%s got %ld; expecting %ld", c.what, c.got, c.want);
            return false;
        }
    }

    std::filesystem::remove(OUT_FILE);
    return true;
}

bool test_path_algos()
{
    const struct
    {
        uint8_t algo;
        const char *name;
    } valid[] = {
        {BossData::ASTAR, "ASTAR"},
        {BossData::BFS, "BFS"},
        {BossData::LOS, "LOS"},
        {BossData::ASTAR_SMOOTH, "ASTAR_SMOOTH"},
        {BossData::DIJKSTRA, "DIJKSTRA"},
        {BossData::FLOWFIELD, "FLOWFIELD"},
        {BossData::THETASTAR, "THETASTAR"},
        {BossData::RAYCAST, "RAYCAST"},
        {BossData::RAYCAST_BULLET, "RAYCAST_BULLET"},
    };
    for (const auto &v : valid)
    {
        if (CPath::getPathAlgo(v.algo) == nullptr)
        {
            LOGE("getPathAlgo(%s) returned null", v.name);
            return false;
        }
    }
    // invalid selectors must not resolve to an algorithm
    if (CPath::getPathAlgo(BossData::NONE) != nullptr ||
        CPath::getPathAlgo(0xff) != nullptr)
    {
        LOGE("getPathAlgo accepted an invalid algo");
        return false;
    }

    // an actor cannot start a path with an unknown algo
    CActor actor(5, 5, 0x20, AIM_DOWN);
    if (actor.startPath(Pos{10, 10}, BossData::NONE, 10))
    {
        LOGE("startPath accepted BossData::NONE");
        return false;
    }

    // CPath::write/read round-trip through CActor: empty path state
    CFileMem mem;
    if (!mem.open("tests/out/actor_path.dat", "rw+"))
    {
        LOGE("can't open actor_path.dat");
        return false;
    }
    CPath path;
    if (!path.write(mem) || !mem.seek(0) || !path.read(mem))
    {
        LOGE("CPath empty round-trip failed");
        mem.close();
        return false;
    }
    mem.close();

    // hostile save: m_aim is deserialized raw (audit L444); only AIM_UP..
    // AIM_RIGHT or the AIM_NONE placeholder may come off the wire
    CFileMem afm;
    if (!afm.open("", "wb"))
    {
        LOGE("actor aim: CFileMem open failed");
        return false;
    }
    if (!actor.write(afm))
    {
        LOGE("actor aim: serialize failed");
        return false;
    }
    const std::vector<uint8_t> record = afm.buffer();
    afm.close();
    auto reload_with_aim = [&record](uint8_t aim)
    {
        std::vector<uint8_t> forged = record;
        forged[3] = aim; // record layout: x, y, type, aim, ...
        CFileMem fm;
        fm.replace(forged.data(), forged.size());
        if (!fm.open("", "rb"))
            return false;
        CActor probe(0, 0, 0, AIM_UP);
        const bool ok = probe.read(fm);
        fm.close();
        return ok;
    };
    if (reload_with_aim(0x7f))
    {
        LOGE("actor aim: accepted corrupt aim 0x7f");
        return false;
    }
    if (!reload_with_aim(AIM_NONE))
    {
        LOGE("actor aim: rejected legitimate AIM_NONE record");
        return false;
    }

    std::filesystem::remove("tests/out/actor_path.dat");
    return true;
}

// maintainer ruling 2026-11-18: actor positions never wrap; out-of-range
// coordinates are capped to the representable cell range [0, 255]
bool test_actor_move_bounds()
{
    CActor actor(5, 5, 0x20, AIM_DOWN);
    actor.move(9, 3);
    if (actor.x() != 9 || actor.y() != 3)
    {
        LOGE("move: in-range coordinates not stored");
        return false;
    }
    actor.move(-1, 0);
    if (actor.x() != 0 || actor.y() != 0)
    {
        LOGE("move: negative x not capped to 0");
        return false;
    }
    actor.move(0, -2);
    if (actor.x() != 0 || actor.y() != 0)
    {
        LOGE("move: negative y not capped to 0");
        return false;
    }
    actor.move(300, 100);
    if (actor.x() != 255 || actor.y() != 100)
    {
        LOGE("move: oversized x not capped to 255 (got %d)", actor.x());
        return false;
    }
    return true;
}
