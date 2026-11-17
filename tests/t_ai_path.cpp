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

#include "../src/logger.h"
#include "../src/game.h"
#include "../src/map.h"
#include "../src/ai_path.h"
#include "../src/isprite.h"
#include "../src/actor.h"
#include "../src/bossdata.h"
#include "../src/shared/FileMem.h"
#include "t_ai_path.h"

namespace
{
    constexpr uint16_t BLOCKED = 0xff;

    // minimal sprite whose solidity is "map tile != BLOCKED";
    // aim->delta table mirrors CGame::translate() / fixed PathData::g_deltas
    class TestSprite : public ISprite
    {
        int16_t m_x = 0;
        int16_t m_y = 0;
        JoyAim m_aim = AIM_UP;

        static Pos delta(const JoyAim aim)
        {
            // matches CGame::translate()
            switch (aim)
            {
            case AIM_UP: return {0, -1};
            case AIM_DOWN: return {0, 1};
            case AIM_LEFT: return {-1, 0};
            default: return {1, 0}; // AIM_RIGHT
            }
        }

    public:
        int16_t x() const override { return m_x; }
        int16_t y() const override { return m_y; }
        uint8_t type() const override { return 0; }
        bool canMove(const JoyAim aim) const override
        {
            const Pos d = delta(aim);
            const int nx = m_x + d.x;
            const int ny = m_y + d.y;
            const CMap &map = CGame::getMap();
            if (nx < 0 || ny < 0 || nx >= map.width() || ny >= map.height())
                return false;
            return map.at(nx, ny) != BLOCKED;
        }
        void move(const JoyAim aim) override
        {
            const Pos d = delta(aim);
            m_x = static_cast<int16_t>(m_x + d.x);
            m_y = static_cast<int16_t>(m_y + d.y);
            m_aim = aim;
        }
        int distance(const CActor &) const override { return 0; }
        void move(const int16_t x, const int16_t y) override
        {
            m_x = x;
            m_y = y;
        }
        void move(const Pos pos) override { move(pos.x, pos.y); }
        int16_t getGranularFactor() const override { return 1; }
        const Pos pos() const override { return Pos{m_x, m_y}; }
        void setAim(const JoyAim aim) override { m_aim = aim; }
        JoyAim getAim() const override { return m_aim; }
        bool isBoss() const override { return false; }
        int getTTL() const override { return BossData::NoTTL; }
    };

    // replay the returned aims in geometry space (findPath labels steps by
    // their actual displacement); fails if a blocked cell is entered
    Pos replay(const Pos &start, const std::vector<JoyAim> &moves)
    {
        Pos p = start;
        for (const auto aim : moves)
        {
            switch (aim)
            {
            case AIM_UP: --p.y; break;
            case AIM_DOWN: ++p.y; break;
            case AIM_LEFT: --p.x; break;
            default: ++p.x; break; // AIM_RIGHT
            }
        }
        return p;
    }

    bool checkPath(const char *algo, TestSprite &sprite, const IPath &path,
                   const Pos &start, const Pos &goal, size_t expectedLen)
    {
        sprite.move(start);
        const auto moves = path.findPath(sprite, goal);
        if (moves.size() != expectedLen)
        {
            LOGE("%s: path size %zu; expecting %zu", algo, moves.size(), expectedLen);
            return false;
        }
        const Pos end = replay(start, moves);
        if (!(end == goal))
        {
            LOGE("%s: path ends at (%d, %d); expecting (%d, %d)",
                 algo, end.x, end.y, goal.x, goal.y);
            return false;
        }
        return true;
    }

    void fillMap(const int size)
    {
        CMap &map = CGame::getMap();
        map.clear();
        map.resize(size, size, 0, true);
    }

    void wallColumn(const int x, const int y0, const int y1)
    {
        CMap &map = CGame::getMap();
        for (int y = y0; y <= y1; ++y)
            map.set(x, y, BLOCKED);
    }
}

bool test_pathfinding()
{
    CGame::getGame(); // ensure the singleton backing getMap() exists
    fillMap(10);

    TestSprite sprite;
    const AStar aStar;
    const AStarSmooth aStarSmooth;
    const BFS bfs;
    const LineOfSight los;

    // open grid: optimal = manhattan (3)
    const Pos start{1, 1};
    const Pos goal{4, 1};
    if (!checkPath("AStar", sprite, aStar, start, goal, 3) ||
        !checkPath("AStarSmooth", sprite, aStarSmooth, start, goal, 3) ||
        !checkPath("BFS", sprite, bfs, start, goal, 3))
        return false;

    // line of sight along a row: 4 straight steps
    if (!checkPath("LOS", sprite, los, Pos{2, 2}, Pos{6, 2}, 4))
        return false;

    // wall with a gap: AStar/BFS must route around it (10 steps optimal)
    wallColumn(1, 0, 2);
    if (!checkPath("AStar", sprite, aStar, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("BFS", sprite, bfs, Pos{0, 0}, Pos{4, 0}, 10))
        return false;

    // BUG-C9: map whose heap tie-breaks force a g-cost relaxation (a node
    // already in openList is reached cheaper); pre-fix freed the old Node
    // there while openList still held its raw pointer (heap-use-after-free
    // under ASan). Layout from a search-identical simulation of the pre-fix
    // loop: only this start/goal/wall set fires the branch.
    {
        CMap &map = CGame::getMap();
        map.clear();
        map.resize(8, 6, 0, true);
        constexpr int RELAX_WALLS[][2] = {{1, 0}, {1, 1}, {0, 3}, {3, 3}, {6, 3}, {0, 4}, {7, 5}};
        for (const auto &w : RELAX_WALLS)
            map.set(w[0], w[1], BLOCKED);
        if (!checkPath("AStar", sprite, aStar, Pos{2, 5}, Pos{7, 0}, 10) ||
            !checkPath("BFS", sprite, bfs, Pos{2, 5}, Pos{7, 0}, 10))
            return false;
    }

    // restore the gap-wall map for the sealed-wall cases below
    fillMap(10);
    wallColumn(1, 0, 2);

    // fully sealed wall: no path
    wallColumn(1, 3, 9);
    sprite.move(Pos{0, 0});
    if (!aStar.findPath(sprite, Pos{4, 0}).empty() ||
        !bfs.findPath(sprite, Pos{4, 0}).empty())
    {
        LOGE("sealed wall: expected no path");
        return false;
    }

    // out-of-bounds goal must return empty, not crash
    sprite.move(Pos{0, 0});
    if (!aStar.findPath(sprite, Pos{99, 99}).empty() ||
        !bfs.findPath(sprite, Pos{-1, 0}).empty() ||
        !los.findPath(sprite, Pos{0, 99}).empty())
    {
        LOGE("out-of-bounds goal returned a path");
        return false;
    }

    fillMap(1); // leave a trivial map for any later test
    return true;
}

// CPath::read gates hostile save records: checkBound results reject
// pathSize/pathIndex/pathTimeout over MAX_PATH_SIZE and out-of-range
// directions; a CPath::write round-trip stays accepted.
bool test_path_load_validation()
{
    // wire layout: pathSize(2 LE) + pathSize x dir(1) + pathIndex(2) + pathTimeout(2)
    auto load = [](const std::vector<uint8_t> &bytes)
    {
        CFileMem fm;
        fm.replace(bytes.data(), bytes.size());
        if (!fm.open("", "rb"))
        {
            LOGE("path load: CFileMem open failed");
            return false;
        }
        CPath path;
        const bool ok = path.read(fm);
        fm.close();
        return ok;
    };

    // valid record: one cached AIM_DOWN step, index 0, timeout 5
    if (!load({1, 0, AIM_DOWN, 0, 0, 5, 0}))
    {
        LOGE("path load: rejected a valid record");
        return false;
    }
    // pathSize 4225 > MAX_PATH_SIZE (4096)
    if (load({0x81, 0x10}))
    {
        LOGE("path load: accepted oversized pathSize");
        return false;
    }
    // pathIndex over the cap
    if (load({1, 0, AIM_DOWN, 0x81, 0x10, 0, 0}))
    {
        LOGE("path load: accepted oversized pathIndex");
        return false;
    }
    // pathTimeout over the cap
    if (load({1, 0, AIM_DOWN, 0, 0, 0x81, 0x10}))
    {
        LOGE("path load: accepted oversized pathTimeout");
        return false;
    }
    // out-of-range direction
    if (load({1, 0, 0x7f, 0, 0, 0, 0}))
    {
        LOGE("path load: accepted corrupt direction byte");
        return false;
    }
    // truncated record (claims a direction it does not carry)
    if (load({1, 0}))
    {
        LOGE("path load: accepted truncated record");
        return false;
    }

    // end-to-end: whatever CPath::write produces must reload cleanly
    CFileMem fm;
    if (!fm.open("", "wb"))
    {
        LOGE("path write: CFileMem open failed");
        return false;
    }
    CPath src;
    if (!src.write(fm))
    {
        LOGE("path write: CPath::write failed");
        return false;
    }
    const std::vector<uint8_t> bytes = fm.buffer();
    fm.close();
    if (!load(bytes))
    {
        LOGE("path load: rejected a freshly written record");
        return false;
    }
    return true;
}
