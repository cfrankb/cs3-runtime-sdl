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

#include "../../src/logger.h"
#include "../../src/game.h"
#include "../../src/map.h"
#include "../../src/ai_path.h"
#include "../../src/isprite.h"
#include "../../src/actor.h"
#include "../../src/bossdata.h"
#include "../../src/shared/FileMem.h"
#include <random>
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

    // followPath's non-boss branch downcasts to CActor (shadowActorMove);
    // isBoss() == true takes the plain sprite.move(aim) path instead
    class BossSprite : public TestSprite
    {
    public:
        bool isBoss() const override { return true; }
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

    // number of direction changes in an aim sequence
    size_t turnCount(const std::vector<JoyAim> &moves)
    {
        size_t turns = 0;
        for (size_t i = 1; i < moves.size(); ++i)
            if (moves[i] != moves[i - 1])
                ++turns;
        return turns;
    }
}

bool test_pathfinding()
{
    CGame::getGame(); // ensure the singleton backing getMap() exists
    fillMap(10);

    TestSprite sprite;
    const AStar aStar;
    const AStarSmooth aStarSmooth;
    const Dijkstra dijkstra;
    const BFS bfs;
    const LineOfSight los;
    const Raycast raycast;
    const RaycastBullet raycastBullet;
    const FlowField flowField;
    const ThetaStar thetaStar;
    const AStarBudget aStarBudget;
    const Greedy greedy;
    const Flee flee;
    const AStarSafe aStarSafe;

    // open grid: optimal = manhattan (3)
    const Pos start{1, 1};
    const Pos goal{4, 1};
    if (!checkPath("AStar", sprite, aStar, start, goal, 3) ||
        !checkPath("AStarSmooth", sprite, aStarSmooth, start, goal, 3) ||
        !checkPath("AStarBudget", sprite, aStarBudget, start, goal, 3) ||
        !checkPath("Greedy", sprite, greedy, start, goal, 3) ||
        !checkPath("Dijkstra", sprite, dijkstra, start, goal, 3) ||
        !checkPath("FlowField", sprite, flowField, start, goal, 3) ||
        !checkPath("ThetaStar", sprite, thetaStar, start, goal, 3) ||
        !checkPath("BFS", sprite, bfs, start, goal, 3))
        return false;

    // line of sight along a row: 4 straight steps
    if (!checkPath("LOS", sprite, los, Pos{2, 2}, Pos{6, 2}, 4))
        return false;

    // raycast: pure Bresenham geometry. Row run is straight, the perfect
    // diagonal is the exact R/D staircase (aim-sequence pin)
    if (!checkPath("Raycast", sprite, raycast, Pos{2, 2}, Pos{6, 2}, 4) ||
        !checkPath("Raycast", sprite, raycast, Pos{1, 1}, Pos{4, 4}, 6))
        return false;
    sprite.move(Pos{1, 1});
    {
        const std::vector<JoyAim> want{AIM_RIGHT, AIM_DOWN, AIM_RIGHT,
                                       AIM_DOWN, AIM_RIGHT, AIM_DOWN};
        if (raycast.findPath(sprite, Pos{4, 4}) != want)
        {
            LOGE("Raycast diagonal: unexpected aim sequence");
            return false;
        }
    }

    // RaycastBullet (Amanatides & Woo DDA): exact 45-degree rays and axis
    // runs match Raycast; steeper slopes differ from the Bresenham
    // staircase in step ORDER (the bullet's collision-cell sequence).
    // Discriminator (0,0)->(1,3): the center-to-center line is still in
    // column 0 at row 1, so DDA gives D D R D while Raycast's balanced
    // staircase detours through the never-touched cell (1,1) with D R D D.
    if (!checkPath("RaycastBullet", sprite, raycastBullet, Pos{2, 2}, Pos{6, 2}, 4) ||
        !checkPath("RaycastBullet", sprite, raycastBullet, Pos{1, 1}, Pos{4, 4}, 6))
        return false;
    {
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> want{AIM_DOWN, AIM_DOWN, AIM_RIGHT, AIM_DOWN};
        if (raycastBullet.findPath(sprite, Pos{1, 3}) != want)
        {
            LOGE("RaycastBullet discriminator: (0,0)->(1,3) is not the DDA order D D R D");
            return false;
        }
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> stair{AIM_DOWN, AIM_RIGHT, AIM_DOWN, AIM_DOWN};
        if (raycast.findPath(sprite, Pos{1, 3}) != stair)
        {
            LOGE("Raycast discriminator: (0,0)->(1,3) lost its Bresenham staircase order");
            return false;
        }
        // exact-corner ties take x first (supercover: both corner-adjacent
        // cells visited, a bullet never leaks through a diagonal wall seam)
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> diag{AIM_RIGHT, AIM_DOWN, AIM_RIGHT,
                                       AIM_DOWN, AIM_RIGHT, AIM_DOWN};
        if (raycastBullet.findPath(sprite, Pos{3, 3}) != diag)
        {
            LOGE("RaycastBullet corner diagonal: unexpected aim sequence");
            return false;
        }
        // sign coverage: dx=3 dy=2 up-left slope (0x55 ladder), mirror of
        // the down-right (3,2) case
        sprite.move(Pos{3, 3});
        const std::vector<JoyAim> upLeft{AIM_LEFT, AIM_DOWN, AIM_LEFT,
                                         AIM_LEFT, AIM_DOWN};
        if (raycastBullet.findPath(sprite, Pos{0, 5}) != upLeft)
        {
            LOGE("RaycastBullet (3,3)->(0,5): unexpected aim sequence");
            return false;
        }
    }

    // wall with a gap: AStar/BFS must route around it (10 steps optimal)
    wallColumn(1, 0, 2);
    if (!checkPath("AStar", sprite, aStar, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("AStarBudget", sprite, aStarBudget, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("BFS", sprite, bfs, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("Dijkstra", sprite, dijkstra, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("FlowField", sprite, flowField, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("ThetaStar", sprite, thetaStar, Pos{0, 0}, Pos{4, 0}, 10) ||
        !checkPath("AStarSmooth", sprite, aStarSmooth, Pos{0, 0}, Pos{4, 0}, 10))
        return false;

    // detour shape: the smoothed path must not turn more often than A*'s
    sprite.move(Pos{0, 0});
    const auto astarDetour = aStar.findPath(sprite, Pos{4, 0});
    sprite.move(Pos{0, 0});
    const auto smoothDetour = aStarSmooth.findPath(sprite, Pos{4, 0});
    if (turnCount(smoothDetour) > turnCount(astarDetour))
    {
        LOGE("AStarSmooth detour shape: %zu turns vs A* %zu",
             turnCount(smoothDetour), turnCount(astarDetour));
        return false;
    }

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

    // BUG-H17: with snapshot heap keys, A* pop order stays min-fCost, so
    // its path length must always equal the FIFO (order-immune) BFS one.
    // Seeded random maps keep the check deterministic.
    {
        std::mt19937 rng(20261126);
        CMap &map = CGame::getMap();
        for (int trial = 0; trial < 20; ++trial)
        {
            map.clear();
            map.resize(12, 12, 0, true);
            for (int w = 0; w < 20; ++w)
                map.set(static_cast<int>(rng() % 12), static_cast<int>(rng() % 12), BLOCKED);
            const Pos s{static_cast<int16_t>(rng() % 12), static_cast<int16_t>(rng() % 12)};
            const Pos g{static_cast<int16_t>(rng() % 12), static_cast<int16_t>(rng() % 12)};
            if (map.at(s.x, s.y) == BLOCKED || map.at(g.x, g.y) == BLOCKED)
                continue; // start/goal on a wall: both searches agree trivially
            sprite.move(s);
            const auto a = aStar.findPath(sprite, g);
            sprite.move(s);
            const auto b = bfs.findPath(sprite, g);
            if (a.size() != b.size())
            {
                LOGE("BUG-H17 trial %d: A* %zu steps vs BFS %zu ((%d,%d)->(%d,%d))",
                     trial, a.size(), b.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            // Dijkstra equivalence: the h=0 copy must agree with A* on
            // reachability and optimal length on every map
            sprite.move(s);
            const auto d = dijkstra.findPath(sprite, g);
            if (d.size() != a.size() || d.empty() != a.empty())
            {
                LOGE("Dijkstra fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                     trial, d.size(), a.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            // FlowField equivalence: the reverse traversal keeps the same
            // edge predicate, so it must agree on reachability and length
            sprite.move(s);
            const auto ff = flowField.findPath(sprite, g);
            if (ff.size() != a.size() || ff.empty() != a.empty())
            {
                LOGE("FlowField fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                     trial, ff.size(), a.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            // ThetaStar equivalence: Manhattan gCosts stay equal to the
            // emitted step count, so length parity with A* is exact
            sprite.move(s);
            const auto ts = thetaStar.findPath(sprite, g);
            if (ts.size() != a.size() || ts.empty() != a.empty())
            {
                LOGE("ThetaStar fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                     trial, ts.size(), a.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            // AStarBudget equivalence: budget 256 >= the 144 cells of a
            // 12x12 map, so the cap never fires and the search is optimal:
            // same length as A*/BFS and it must actually reach the goal.
            // (Aim-list identity is NOT assertable here: A* breaks fCost
            // heap ties on Node* pointer, so even two A* runs can emit
            // different equal-length lists; exact aims are pinned by the
            // deterministic corridor case below.) Where A* found none, the
            // capped algo deliberately returns the partial best-node path
            // (the phase-1 fallback), so nothing to compare.
            sprite.move(s);
            const auto ab = aStarBudget.findPath(sprite, g);
            if (!a.empty() && (ab.size() != a.size() || !(replay(s, ab) == g)))
            {
                LOGE("AStarBudget fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                     trial, ab.size(), a.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            // Greedy (GBFS) fuzz: declared NON-optimal, so no length
            // parity -- but budget 256 > the 144 cells means every
            // reachable goal is popped, so where BFS found a path greedy
            // must find one that replays onto the goal. A budget-3 clone
            // must never emit more than 3 steps (partial-path bound).
            sprite.move(s);
            const auto gr = greedy.findPath(sprite, g);
            if (!b.empty() && (gr.empty() || !(replay(s, gr) == g)))
            {
                LOGE("Greedy fuzz trial %d: %zu steps, replay to (%d,%d), goal (%d,%d) ((%d,%d)->(%d,%d))",
                     trial, gr.size(), replay(s, gr).x, replay(s, gr).y, g.x, g.y, s.x, s.y, g.x, g.y);
                return false;
            }
            sprite.move(s);
            const Greedy tiny(3);
            const auto gr3 = tiny.findPath(sprite, g);
            if (gr3.size() > 3)
            {
                LOGE("Greedy budget-3 fuzz trial %d: %zu steps exceed budget", trial, gr3.size());
                return false;
            }
            // RaycastBullet parity: the DDA ray has no wall semantics, so
            // it must always emit exactly the Raycast step count (dx+dy)
            sprite.move(s);
            const auto rb = raycastBullet.findPath(sprite, g);
            sprite.move(s);
            const auto rc = raycast.findPath(sprite, g);
            if (rb.size() != rc.size() || rb.empty() != rc.empty())
            {
                LOGE("RaycastBullet fuzz trial %d: %zu steps vs Raycast %zu ((%d,%d)->(%d,%d))",
                     trial, rb.size(), rc.size(), s.x, s.y, g.x, g.y);
                return false;
            }
        }
    }

    // fix-astar-smooth: S-bend, opposing columns x=3 rows 0..6 and x=8
    // rows 5..11 on a 12-grid force crossings at y>=7 and y<=4; optimal
    // is 21 steps for both algos (11 horizontal + 5 down + 3 up + 2 up)
    {
        fillMap(12);
        CMap &map = CGame::getMap();
        for (int y = 0; y <= 6; ++y)
            map.set(3, y, BLOCKED);
        for (int y = 5; y <= 11; ++y)
            map.set(8, y, BLOCKED);
        if (!checkPath("AStar", sprite, aStar, Pos{0, 2}, Pos{11, 2}, 21) ||
            !checkPath("AStarSmooth", sprite, aStarSmooth, Pos{0, 2}, Pos{11, 2}, 21))
            return false;
    }

    // staircase oracle, exact shape: with column x=3 rows 0..6 walled,
    // every optimal (2,1)->(4,5) path is D6 R2 U2 (crossing only at row 7,
    // up-legs only in the x=4 column); the x-first staircase oracle must
    // reproduce exactly that aim sequence
    {
        fillMap(12);
        wallColumn(3, 0, 6);
        sprite.move(Pos{2, 1});
        const auto moves = aStarSmooth.findPath(sprite, Pos{4, 5});
        const std::vector<JoyAim> want{AIM_DOWN, AIM_DOWN, AIM_DOWN, AIM_DOWN,
                                       AIM_DOWN, AIM_DOWN, AIM_RIGHT, AIM_RIGHT,
                                       AIM_UP, AIM_UP};
        if (moves != want)
        {
            LOGE("staircase oracle: got %zu steps; expecting exact D6 R2 U2",
                 moves.size());
            return false;
        }
    }

    // seeded fuzz: AStarSmooth length parity with A* and every replayed
    // cell walkable (catches an oracle accepting a routed detour)
    {
        std::mt19937 rng(0x5A05);
        CMap &map = CGame::getMap();
        for (int trial = 0; trial < 50; ++trial)
        {
            map.clear();
            map.resize(12, 12, 0, true);
            for (int w = 0; w < 20; ++w)
                map.set(static_cast<int>(rng() % 12), static_cast<int>(rng() % 12), BLOCKED);
            const Pos s{static_cast<int16_t>(rng() % 12), static_cast<int16_t>(rng() % 12)};
            const Pos g{static_cast<int16_t>(rng() % 12), static_cast<int16_t>(rng() % 12)};
            if (map.at(s.x, s.y) == BLOCKED || map.at(g.x, g.y) == BLOCKED)
                continue;
            sprite.move(s);
            const auto smoothMoves = aStarSmooth.findPath(sprite, g);
            sprite.move(s);
            const auto astarMoves = aStar.findPath(sprite, g);
            if (smoothMoves.size() != astarMoves.size())
            {
                LOGE("AStarSmooth fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                     trial, smoothMoves.size(), astarMoves.size(), s.x, s.y, g.x, g.y);
                return false;
            }
            Pos p = s;
            for (const auto aim : smoothMoves)
            {
                switch (aim)
                {
                case AIM_UP: --p.y; break;
                case AIM_DOWN: ++p.y; break;
                case AIM_LEFT: --p.x; break;
                default: ++p.x; break; // AIM_RIGHT
                }
                if (p.x < 0 || p.y < 0 || p.x >= 12 || p.y >= 12 || map.at(p.x, p.y) == BLOCKED)
                {
                    LOGE("AStarSmooth fuzz trial %d: path enters blocked/out-of-bounds (%d,%d)",
                         trial, p.x, p.y);
                    return false;
                }
            }
            if (!(p == g))
            {
                LOGE("AStarSmooth fuzz trial %d: ends at (%d,%d), goal (%d,%d)",
                     trial, p.x, p.y, g.x, g.y);
                return false;
            }
        }
    }

    // ThetaStar straightness: the in-loop grandparent relaxation merges
    // collinear runs, so on a clear LOS row it must not turn more often
    // than AStarSmooth's after-pass (shape check, not an exact aim pin)
    {
        fillMap(10);
        sprite.move(Pos{2, 2});
        const auto tsRow = thetaStar.findPath(sprite, Pos{6, 2});
        sprite.move(Pos{2, 2});
        const auto smoothRow = aStarSmooth.findPath(sprite, Pos{6, 2});
        if (turnCount(tsRow) > turnCount(smoothRow))
        {
            LOGE("ThetaStar LOS row shape: %zu turns vs AStarSmooth %zu",
                 turnCount(tsRow), turnCount(smoothRow));
            return false;
        }
    }

    // restore the gap-wall map for the sealed-wall cases below
    fillMap(10);
    wallColumn(1, 0, 2);

    // fully sealed wall: no path
    wallColumn(1, 3, 9);
    sprite.move(Pos{0, 0});
    if (!aStar.findPath(sprite, Pos{4, 0}).empty() ||
        !bfs.findPath(sprite, Pos{4, 0}).empty() ||
        !dijkstra.findPath(sprite, Pos{4, 0}).empty() ||
        !thetaStar.findPath(sprite, Pos{4, 0}).empty() ||
        !greedy.findPath(sprite, Pos{4, 0}).empty() ||
        !aStarSafe.findPath(sprite, Pos{4, 0}).empty() ||
        !flowField.findPath(sprite, Pos{4, 0}).empty())
    {
        LOGE("sealed wall: expected no path");
        return false;
    }

    // Raycast design pin (bullet semantics): the ray is NOT probe-trimmed,
    // it keeps its full geometric length through walls; CPath::followPath's
    // runtime canMove() then reports Blocked and handleBullet detonates the
    // bullet at the wall. A future "fix" adding a probe here parks NoTTL
    // bullets at walls instead (see Raycast::findPath comment).
    {
        sprite.move(Pos{0, 0});
        const auto ray = raycast.findPath(sprite, Pos{4, 0});
        const std::vector<JoyAim> want{AIM_RIGHT, AIM_RIGHT, AIM_RIGHT, AIM_RIGHT};
        if (ray != want)
        {
            LOGE("sealed wall: raycast must stay geometric (4 RIGHT steps), got %zu steps",
                 ray.size());
            return false;
        }
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> diag{AIM_RIGHT, AIM_DOWN, AIM_RIGHT,
                                       AIM_DOWN, AIM_RIGHT, AIM_DOWN};
        if (raycastBullet.findPath(sprite, Pos{3, 3}) != diag)
        {
            LOGE("sealed wall: raycastBullet must stay geometric (full 6-step diagonal)");
            return false;
        }
    }

    // AStarBudget (plan phase 1): one-wide corridor on row y=0 (row y=1
    // fully walled), goal (9,1) sealed in the walled row. A* finds nothing;
    // the capped search converges at (9,0), the reachable cell closest to
    // the goal. The corridor never ties on pop order, so pins are exact.
    {
        fillMap(10);
        CMap &map = CGame::getMap();
        for (int x = 0; x < 10; ++x)
            map.set(x, 1, BLOCKED);
        sprite.move(Pos{0, 0});
        if (!aStar.findPath(sprite, Pos{9, 1}).empty())
        {
            LOGE("AStarBudget corridor: A* expected empty for the sealed-row goal");
            return false;
        }
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> full(9, AIM_RIGHT);
        if (aStarBudget.findPath(sprite, Pos{9, 1}) != full)
        {
            LOGE("AStarBudget corridor: expected 9 RIGHT to (9,0)");
            return false;
        }
        // budget cut: 3 expansions walk exactly 3 steps toward the goal
        const AStarBudget tight(3);
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> cut(3, AIM_RIGHT);
        if (tight.findPath(sprite, Pos{9, 1}) != cut)
        {
            LOGE("AStarBudget budget-3: expected 3 RIGHT");
            return false;
        }
        // Greedy (plan phase 2): the corridor forces the same walk and its
        // keys never tie, so the same pins apply
        sprite.move(Pos{0, 0});
        if (greedy.findPath(sprite, Pos{9, 1}) != full)
        {
            LOGE("Greedy corridor: expected 9 RIGHT to (9,0)");
            return false;
        }
        const Greedy greedyTight(3);
        sprite.move(Pos{0, 0});
        if (greedyTight.findPath(sprite, Pos{9, 1}) != cut)
        {
            LOGE("Greedy budget-3: expected 3 RIGHT");
            return false;
        }
        // boxed-in start: (1,0) walled too, start is its own best node -> empty
        map.set(1, 0, BLOCKED);
        map.set(0, 1, BLOCKED);
        sprite.move(Pos{0, 0});
        if (!aStarBudget.findPath(sprite, Pos{9, 1}).empty() ||
            !greedy.findPath(sprite, Pos{9, 1}).empty())
        {
            LOGE("boxed-in start: AStarBudget/Greedy expected empty");
            return false;
        }
    }

    // AStarSafe (plan phase 4): entry cost = 1 + kOpenSpaceWeight per
    // refused canMove probe out of the entered cell (cramped cells and
    // dead ends are penalised, never forbidden).
    {
        // wall-hug choice: two equal 6-step routes around the column
        // {(4,3),(4,4)}; every route pays at least 1 (entering (4,2) right
        // beside the wall top is the cheapest crossing) and the x=6 descent
        // is the only one that pays exactly that, so it is the unique
        // optimum on a tie-free map -> exact aim pin.
        fillMap(10);
        CMap &safeMap = CGame::getMap();
        safeMap.set(4, 3, BLOCKED);
        safeMap.set(4, 4, BLOCKED);
        sprite.move(Pos{3, 2});
        const std::vector<JoyAim> hug{AIM_RIGHT, AIM_RIGHT, AIM_RIGHT,
                                      AIM_DOWN, AIM_DOWN, AIM_DOWN};
        if (aStarSafe.findPath(sprite, Pos{6, 5}) != hug)
        {
            LOGE("AStarSafe wall-hug: expected RRR DDD (open descent, penalty 1)");
            return false;
        }

        // corridor forced: entering (x,0) pays 2 (map edge + wall row),
        // the penalty biases but never refuses: the sealed-row goal still
        // converges at (9,0) and a budget-3 cut walks exactly 3 steps --
        // fallback keyed on hCost as phase 1 (f = 10 + 2x is strictly
        // increasing along the corridor, so the pop order is forced)
        fillMap(10);
        for (int x = 0; x < 10; ++x)
            safeMap.set(x, 1, BLOCKED);
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> safeFull(9, AIM_RIGHT);
        if (aStarSafe.findPath(sprite, Pos{9, 1}) != safeFull)
        {
            LOGE("AStarSafe corridor: expected 9 RIGHT to (9,0)");
            return false;
        }
        const AStarSafe safeTight(3);
        sprite.move(Pos{0, 0});
        const std::vector<JoyAim> safeCut(3, AIM_RIGHT);
        if (safeTight.findPath(sprite, Pos{9, 1}) != safeCut)
        {
            LOGE("AStarSafe budget-3: expected 3 RIGHT");
            return false;
        }

        // free oracle: wall-free maps with interior start/goal admit a
        // zero-penalty monotone route, so the optimal STEP count equals
        // A*'s (penalties can only re-rank equal-cost routes, never raise
        // this bound). Aim identity is NOT assertable (Node* heap ties,
        // see the BUG-H17 note); budget 256 > the 144 cells never fires.
        {
            std::mt19937 rng(20261127);
            CMap &map = CGame::getMap();
            for (int trial = 0; trial < 20; ++trial)
            {
                map.clear();
                map.resize(12, 12, 0, true);
                const Pos s{static_cast<int16_t>(1 + rng() % 10),
                            static_cast<int16_t>(1 + rng() % 10)};
                const Pos g{static_cast<int16_t>(1 + rng() % 10),
                            static_cast<int16_t>(1 + rng() % 10)};
                sprite.move(s);
                const auto a = aStar.findPath(sprite, g);
                sprite.move(s);
                const auto safe = aStarSafe.findPath(sprite, g);
                if (safe.size() != a.size() || !(replay(s, safe) == g))
                {
                    LOGE("AStarSafe free fuzz trial %d: %zu steps vs A* %zu ((%d,%d)->(%d,%d))",
                         trial, safe.size(), a.size(), s.x, s.y, g.x, g.y);
                    return false;
                }
            }
        }
    }

    // out-of-bounds goal must return empty, not crash
    sprite.move(Pos{0, 0});
    if (!aStar.findPath(sprite, Pos{99, 99}).empty() ||
        !aStarBudget.findPath(sprite, Pos{99, 99}).empty() ||
        !greedy.findPath(sprite, Pos{99, 99}).empty() ||
        !flee.findPath(sprite, Pos{99, 99}).empty() ||
        !aStarSafe.findPath(sprite, Pos{99, 99}).empty() ||
        !bfs.findPath(sprite, Pos{-1, 0}).empty() ||
        !dijkstra.findPath(sprite, Pos{99, 99}).empty() ||
        !thetaStar.findPath(sprite, Pos{99, 99}).empty() ||
        !flowField.findPath(sprite, Pos{99, 99}).empty() ||
        !los.findPath(sprite, Pos{0, 99}).empty() ||
        !raycast.findPath(sprite, Pos{99, 99}).empty() ||
        !raycastBullet.findPath(sprite, Pos{99, 99}).empty())
    {
        LOGE("out-of-bounds goal returned a path");
        return false;
    }

    // Raycast start == goal: empty aim list (nothing to fly)
    sprite.move(Pos{5, 5});
    if (!raycast.findPath(sprite, Pos{5, 5}).empty() ||
        !raycastBullet.findPath(sprite, Pos{5, 5}).empty())
    {
        LOGE("Raycast start==goal returned a path");
        return false;
    }

    // registry: DIJKSTRA and FLOWFIELD must resolve to the const instances
    if (CPath::getPathAlgo(BossData::DIJKSTRA) == nullptr)
    {
        LOGE("getPathAlgo(DIJKSTRA) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::FLOWFIELD) == nullptr)
    {
        LOGE("getPathAlgo(FLOWFIELD) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::THETASTAR) == nullptr)
    {
        LOGE("getPathAlgo(THETASTAR) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::RAYCAST) == nullptr)
    {
        LOGE("getPathAlgo(RAYCAST) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::RAYCAST_BULLET) == nullptr)
    {
        LOGE("getPathAlgo(RAYCAST_BULLET) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::ASTAR_BUDGET) == nullptr)
    {
        LOGE("getPathAlgo(ASTAR_BUDGET) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::GBFS) == nullptr)
    {
        LOGE("getPathAlgo(GBFS) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::FLEE) == nullptr)
    {
        LOGE("getPathAlgo(FLEE) returned null");
        return false;
    }
    if (CPath::getPathAlgo(BossData::ASTAR_SAFE) == nullptr)
    {
        LOGE("getPathAlgo(ASTAR_SAFE) returned null");
        return false;
    }

    // maintainer ruling 2026-11-18: start == goal skips the search and
    // reports NoValidPath (no full-grid re-exploration while touching)
    {
        CPath cp;
        sprite.move(Pos{5, 5});
        if (cp.followPath(sprite, Pos{5, 5}, aStar) != CPath::NoValidPath)
        {
            LOGE("start==goal: expected NoValidPath without search");
            return false;
        }
    }

    // Flee (plan phase 3)
    {
        // flees() declaration: only the flee searcher opts out of the
        // on-goal short-circuit
        if (!flee.flees() || aStar.flees() || greedy.flees())
        {
            LOGE("flees(): only Flee may declare true");
            return false;
        }

        // open 10x10, sprite (1,1), threat (8,8): maximize Manhattan
        // distance to the threat over cells within FLEE_RADIUS; the corner
        // (0,0) is uniquely farthest (16) and its parent chain is forced
        // by BFS's UP-first step order -> UP LEFT
        fillMap(10);
        sprite.move(Pos{1, 1});
        const std::vector<JoyAim> wantFlee{AIM_UP, AIM_LEFT};
        if (flee.findPath(sprite, Pos{8, 8}) != wantFlee)
        {
            LOGE("Flee open map: expected UP LEFT to (0,0)");
            return false;
        }

        // wall-sealed pocket: row y=1 fully walled, sprite (0,0) standing
        // on the threat; reachable set is row y=0 only, farthest is (9,0)
        {
            fillMap(10);
            CMap &map = CGame::getMap();
            for (int x = 0; x < 10; ++x)
                map.set(x, 1, BLOCKED);
            sprite.move(Pos{0, 0});
            const std::vector<JoyAim> corridor(9, AIM_RIGHT);
            if (flee.findPath(sprite, Pos{0, 0}) != corridor)
            {
                LOGE("Flee pocket: expected 9 RIGHT away from (0,0)");
                return false;
            }
            // boxed-in sprite: answer is the start -> empty aim list
            map.set(1, 0, BLOCKED);
            if (!flee.findPath(sprite, Pos{0, 0}).empty())
            {
                LOGE("Flee boxed-in: expected empty");
                return false;
            }
        }

        // driver concession: sprite standing ON the threat is exactly when
        // a flee must search; the on-goal short-circuit must not fire.
        // (isBoss()==true takes the plain move() branch; the aStar
        // short-circuit regression is covered by the case above)
        fillMap(10);
        CPath cp;
        BossSprite boss;
        boss.move(Pos{5, 5});
        if (cp.followPath(boss, Pos{5, 5}, flee) != CPath::MoveSuccesful ||
            boss.pos() == Pos{5, 5})
        {
            LOGE("Flee adjacency: expected MoveSuccesful away from the threat");
            return false;
        }
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
