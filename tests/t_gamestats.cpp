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
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include "t_gamestats.h"
#include "../src/gamestats.h"
#include "../src/skills.h"
#include "thelper.h"
#include <filesystem>

constexpr int Skill = SKILL_HARD;
constexpr int IdleTime = 3000;
constexpr int Sugar = 5;

bool testStats(CGameStats &stats, const char *context)
{
    if (stats.get(S_SKILL) != Skill)
    {
        LOGE("[%s] stats.get(S_SKILL) -- Got %d; expecting %d\n", context, stats.get(S_SKILL), Skill);
        return false;
    }

    if (stats.get(S_IDLE_TIME) != IdleTime)
    {
        LOGE("[%s] stats.get(S_IDLE_TIME) -- Got %d; expecting %d\n", context, stats.get(S_IDLE_TIME), IdleTime);
        return false;
    }

    if (stats.get(S_SUGAR) != Sugar)
    {
        LOGE("[%s] stats.get(S_SUGAR) -- Got %d; expecting %d\n", context, stats.get(S_SUGAR), Sugar);
        return false;
    }

    return true;
}

bool test_gamestats()
{
    CGameStats stats;
    stats.set(S_SKILL, Skill);
    stats.set(S_IDLE_TIME, IdleTime);
    stats.set(S_SUGAR, Sugar);

    if (!testStats(stats, "initial object"))
        return false;

    const char *outFile1 = "tests/out/stats1.dat";
    const char *outFile2 = "tests/out/stats2.dat";

    // FileWrap Write
    CFileWrap fwfile;
    if (!fwfile.open(outFile1, "wb"))
    {
        LOGE("cannot write %s\n", outFile1);
        return false;
    }
    stats.write(fwfile);
    fwfile.close();

    // FILE* write
    FILE *file = fopen(outFile2, "wb");
    if (!file)
    {
        LOGE("cannot write %s\n", outFile2);
        return false;
    }
    stats.write(file);
    fclose(file);

    // correlate both files
    if (getFileSize(outFile1) != getFileSize(outFile2))
    {
        LOGE("file %s size %ld, file %s size %ld; mismatch writers\n",
             outFile1, getFileSize(outFile1), outFile2, getFileSize(outFile2));
        return false;
    }

    CGameStats stats2{stats};
    if (!testStats(stats2, "constructor"))
        return false;

    CGameStats stats3 = stats;
    if (!testStats(stats3, "copy constructor"))
        return false;

    // FileWrap READ
    CGameStats stats4;
    if (!fwfile.open(outFile1, "rb"))
    {
        LOGE("cannot read %s\n", outFile1);
        return false;
    }
    stats4.read(fwfile);
    fwfile.close();
    if (!testStats(stats4, "read fileWrap"))
    {
        return false;
    }

    // FILE* read
    CGameStats stats5;
    file = fopen(outFile2, "rb");
    if (!file)
    {
        LOGE("cannot read %s\n", outFile2);
        return false;
    }
    stats5.read(file);
    fclose(file);
    if (!testStats(stats5, "read FILE*"))
        return false;

    // inc and dec
    int skill = stats.inc(S_SKILL);
    if (skill != Skill + 1)
    {
        LOGE("stats.inc(S_SKILL) returned %d; expecting %d\n", skill, Skill + 1);
        return false;
    }

    skill = stats.get(S_SKILL);
    if (skill != Skill + 1)
    {
        LOGE("stats.get(S_SKILL) returned %d; expecting %d\n", skill, Skill + 1);
        return false;
    }

    skill = stats.dec(S_SKILL);
    if (skill != Skill)
    {
        LOGE("stats.dec(S_SKILL) returned %d; expecting %d\n", skill, Skill);
        return false;
    }

    skill = stats.get(S_SKILL);
    if (skill != Skill)
    {
        LOGE("stats.get(S_SKILL) returned %d; expecting %d\n", skill, Skill);
        return false;
    }

    std::filesystem::remove(outFile1);
    std::filesystem::remove(outFile2);
    return true;
}