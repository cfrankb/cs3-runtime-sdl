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
#include "game.h"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <utility>
#include "map.h"
#include "logger.h"
#include "ai_path.h"
#include "isprite.h"
#include "filemacros.h"
#include "bossdata.h"
#include "shared/IFile.h"

namespace PathData
{
    const AStar aStar;
    const AStarSmooth aStarSmooth;
    const Dijkstra dijkstra;
    const BFS bFS;
    const LineOfSight lineOfSight;
    const Raycast raycast;
    const RaycastBullet raycastBullet;
    const FlowField flowField;
    const ThetaStar thetaStar;

    constexpr int PATH_TIMEOUT_MAX = 10; // Recompute path every 10 turns
    constexpr size_t MAX_PATH_SIZE = 4096;
    // delta+direction pairs kept structurally in sync; must match the
    // CGame::translate()/JoyAim convention used by sprites and by the
    // direction labelling below
    struct Step
    {
        Pos delta;
        JoyAim dir;
    };
    constexpr Step g_steps[] = {
        {Pos{0, -1}, AIM_UP},   // Up
        {Pos{0, 1}, AIM_DOWN},  // Down
        {Pos{-1, 0}, AIM_LEFT}, // Left
        {Pos{1, 0}, AIM_RIGHT}, // Right
    };
}

using namespace PathData;

namespace std
{
    template <>
    struct hash<Pos>
    {
        // packs the int16_t pair (every Pos value) injectively into 32 bits;
        // the previous sign-extended 64-bit shift worked only by accident
        size_t operator()(const Pos &pos) const
        {
            return (static_cast<uint32_t>(static_cast<uint16_t>(pos.x)) << 16) |
                   static_cast<uint16_t>(pos.y);
        }
    };
}

struct Node
{
    Pos pos;
    int gCost, hCost;
    Node *parent;
    Node(const Pos &p, int g, int h, Node *par) : pos(p), gCost(g), hCost(h), parent(par) {}
    int fCost() const { return gCost + hCost; }
};

// BUG-H17: open-list entries are immutable (fCost, node) snapshots. The
// openList used to hold live Node* whose key CompareNode read at
// comparison time; g-cost relaxations then mutated a Node still parked
// mid-heap, breaching the min-heap invariant so pop() was no longer
// min-fCost. Snapshot keys + the stale-entry skip on pop keep the heap
// untouched. Min-heap via std::greater on the pair (fCost first).
using OpenEntry = std::pair<int, Node *>;
using OpenList = std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>>;

int AStar::manhattanDistance(const Pos &a, const Pos &b) const
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

std::vector<JoyAim> AStar::findPath(ISprite &sprite, const Pos &goalPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor;
    const int mapHei = map.height() * granularFactor;
    std::vector<JoyAim> directions;
    const Pos startPos = sprite.pos();

    // Validate start and goal positions
    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Priority queue for open list
    OpenList openList;
    std::unordered_map<Pos, std::unique_ptr<Node>> nodes;
    std::unordered_map<Pos, bool> closedList;

    // Create start node
    nodes[startPos] = std::make_unique<Node>(startPos, 0, manhattanDistance(startPos, goalPos), nullptr);
    openList.emplace(nodes[startPos]->fCost(), nodes[startPos].get());

    while (!openList.empty())
    {
        const OpenEntry entry = openList.top();
        openList.pop();
        Node *current = entry.second;

        // BUG-H17: skip stale snapshots left behind by g-cost relaxation
        if (entry.first != current->fCost())
            continue;

        // BUG-C9: relaxation re-pushes the same node; skip stale copies
        if (closedList.find(current->pos) != closedList.end())
            continue;

        // Reached goal
        if (current->pos == goalPos)
        {
            // Convert path to directions
            Node *node = current;
            std::vector<Pos> path;
            while (node != nullptr)
            {
                path.push_back(node->pos);
                node = node->parent;
            }
            std::reverse(path.begin(), path.end());

            // Convert positions to directions
            for (size_t i = 1; i < path.size(); ++i)
            {
                int dx = path[i].x - path[i - 1].x;
                int dy = path[i].y - path[i - 1].y;
                if (dx == 1 && dy == 0)
                    directions.push_back(JoyAim::AIM_RIGHT);
                else if (dx == -1 && dy == 0)
                    directions.push_back(JoyAim::AIM_LEFT);
                else if (dx == 0 && dy == 1)
                    directions.push_back(JoyAim::AIM_DOWN);
                else if (dx == 0 && dy == -1)
                    directions.push_back(JoyAim::AIM_UP);
                else
                {
                    LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                         path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                    return {};
                }
            }
            return directions;
        }

        closedList[current->pos] = true;

        // Explore neighbors
        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            const Pos newPos{static_cast<int16_t>(current->pos.x + g_steps[i].delta.x),
                             static_cast<int16_t>(current->pos.y + g_steps[i].delta.y)};

            // Check bounds before moving
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei)
                continue;

            if (closedList.find(newPos) != closedList.end())
                continue;

            // probe canMove from the expanded node so the entered cell is checked
            const Pos originalPos{sprite.pos()};
            sprite.move(current->pos);
            const bool canStep = sprite.canMove(g_steps[i].dir);
            sprite.move(originalPos);
            if (!canStep)
                continue;

            int newGCost = current->gCost + 1;
            int newHCost = manhattanDistance(newPos, goalPos);

            auto it = nodes.find(newPos);
            if (it == nodes.end())
            {
                nodes[newPos] = std::make_unique<Node>(newPos, newGCost, newHCost, current);
                openList.emplace(nodes[newPos]->fCost(), nodes[newPos].get());
            }
            else if (newGCost < it->second->gCost)
            {
                // BUG-C9: relax in place. Reassigning the unique_ptr here
                // freed the Node while openList still held its raw pointer.
                // BUG-H17: the heap only holds fCost snapshots now, so the
                // in-place update cannot corrupt it; push the improved
                // snapshot and let the stale one be skipped on pop.
                it->second->gCost = newGCost;
                it->second->parent = current;
                openList.emplace(it->second->fCost(), it->second.get());
            }
        }
    }
    // No path found
    return directions; // Empty if no path found
}

// ponytail: byte-identical to AStar::findPath with h = 0 (the reference
// searcher). If a second heuristic-free searcher is ever needed, share one
// impl via an hCost lambda instead of adding a third copy.
std::vector<JoyAim> Dijkstra::findPath(ISprite &sprite, const Pos &goalPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor;
    const int mapHei = map.height() * granularFactor;
    std::vector<JoyAim> directions;
    const Pos startPos = sprite.pos();

    // Validate start and goal positions
    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Priority queue for open list
    OpenList openList;
    std::unordered_map<Pos, std::unique_ptr<Node>> nodes;
    std::unordered_map<Pos, bool> closedList;

    // Create start node (hCost = 0: uniform-cost search)
    nodes[startPos] = std::make_unique<Node>(startPos, 0, 0, nullptr);
    openList.emplace(nodes[startPos]->fCost(), nodes[startPos].get());

    while (!openList.empty())
    {
        const OpenEntry entry = openList.top();
        openList.pop();
        Node *current = entry.second;

        // BUG-H17: skip stale snapshots left behind by g-cost relaxation
        if (entry.first != current->fCost())
            continue;

        // BUG-C9: relaxation re-pushes the same node; skip stale copies
        if (closedList.find(current->pos) != closedList.end())
            continue;

        // Reached goal
        if (current->pos == goalPos)
        {
            // Convert path to directions
            Node *node = current;
            std::vector<Pos> path;
            while (node != nullptr)
            {
                path.push_back(node->pos);
                node = node->parent;
            }
            std::reverse(path.begin(), path.end());

            // Convert positions to directions
            for (size_t i = 1; i < path.size(); ++i)
            {
                int dx = path[i].x - path[i - 1].x;
                int dy = path[i].y - path[i - 1].y;
                if (dx == 1 && dy == 0)
                    directions.push_back(JoyAim::AIM_RIGHT);
                else if (dx == -1 && dy == 0)
                    directions.push_back(JoyAim::AIM_LEFT);
                else if (dx == 0 && dy == 1)
                    directions.push_back(JoyAim::AIM_DOWN);
                else if (dx == 0 && dy == -1)
                    directions.push_back(JoyAim::AIM_UP);
                else
                {
                    LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                         path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                    return {};
                }
            }
            return directions;
        }

        closedList[current->pos] = true;

        // Explore neighbors
        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            const Pos newPos{static_cast<int16_t>(current->pos.x + g_steps[i].delta.x),
                             static_cast<int16_t>(current->pos.y + g_steps[i].delta.y)};

            // Check bounds before moving
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei)
                continue;

            if (closedList.find(newPos) != closedList.end())
                continue;

            // probe canMove from the expanded node so the entered cell is checked
            const Pos originalPos{sprite.pos()};
            sprite.move(current->pos);
            const bool canStep = sprite.canMove(g_steps[i].dir);
            sprite.move(originalPos);
            if (!canStep)
                continue;

            int newGCost = current->gCost + 1;

            auto it = nodes.find(newPos);
            if (it == nodes.end())
            {
                nodes[newPos] = std::make_unique<Node>(newPos, newGCost, 0, current);
                openList.emplace(nodes[newPos]->fCost(), nodes[newPos].get());
            }
            else if (newGCost < it->second->gCost)
            {
                // BUG-C9: relax in place. Reassigning the unique_ptr here
                // freed the Node while openList still held its raw pointer.
                // BUG-H17: the heap only holds fCost snapshots now, so the
                // in-place update cannot corrupt it; push the improved
                // snapshot and let the stale one be skipped on pop.
                it->second->gCost = newGCost;
                it->second->parent = current;
                openList.emplace(it->second->fCost(), it->second.get());
            }
        }
    }
    // No path found
    return directions; // Empty if no path found
}

std::vector<JoyAim> BFS::findPath(ISprite &sprite, const Pos &goalPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor;
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos();

    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    std::queue<Pos> queue;
    std::unordered_map<Pos, Pos> parent;
    std::unordered_map<Pos, bool> visited;
    queue.push(startPos);
    visited[startPos] = true;

    while (!queue.empty())
    {
        Pos current = queue.front();
        queue.pop();

        if (current == goalPos)
        {
            std::vector<JoyAim> directions;
            Pos node = current;
            std::vector<Pos> path;
            while (node != startPos)
            {
                path.push_back(node);
                node = parent[node];
            }
            path.push_back(startPos);
            std::reverse(path.begin(), path.end());

            for (size_t i = 1; i < path.size(); ++i)
            {
                const int dx = path[i].x - path[i - 1].x;
                const int dy = path[i].y - path[i - 1].y;
                if (dx == 1 && dy == 0)
                    directions.push_back(JoyAim::AIM_RIGHT);
                else if (dx == -1 && dy == 0)
                    directions.push_back(JoyAim::AIM_LEFT);
                else if (dx == 0 && dy == 1)
                    directions.push_back(JoyAim::AIM_DOWN);
                else if (dx == 0 && dy == -1)
                    directions.push_back(JoyAim::AIM_UP);
                else
                {
                    LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                         path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                    return {};
                }
            }
            return directions;
        }

        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            Pos newPos = {static_cast<int16_t>(current.x + g_steps[i].delta.x), static_cast<int16_t>(current.y + g_steps[i].delta.y)};
            // find(), not operator[]: operator[] would default-insert false
            // keys for every rejected neighbor (only true is ever inserted)
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei ||
                visited.find(newPos) != visited.end())
                continue;

            // probe canMove from the expanded node so the entered cell is checked
            const Pos originalPos = sprite.pos();
            sprite.move(current);
            const bool canStep = sprite.canMove(g_steps[i].dir);
            sprite.move(originalPos);
            if (canStep)
            {
                queue.push(newPos);
                visited[newPos] = true;
                parent[newPos] = current;
            }
        }
    }
    return {};
}

std::vector<JoyAim> LineOfSight::findPath(ISprite &sprite, const Pos &playerPos) const
{
    int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor; // Half-tile bounds
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos(); // Half-tile coordinates
    const Pos goalPos = playerPos;

    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Generate 4-directional path and check LOS simultaneously
    std::vector<JoyAim> directions;
    int x = startPos.x;
    int y = startPos.y;
    const int dx = goalPos.x - startPos.x;
    const int dy = goalPos.y - startPos.y;
    int stepsX = std::abs(dx);
    int stepsY = std::abs(dy);
    int stepX = dx >= 0 ? 1 : -1;
    int stepY = dy >= 0 ? 1 : -1;
    const Pos originalPos = sprite.pos();

    while (x != goalPos.x || y != goalPos.y)
    {
        bool moved = false;
        // Prioritize x movement if x distance is larger or equal, or y is done
        if (x != goalPos.x && (stepsX >= stepsY || y == goalPos.y))
        {
            Pos next = {static_cast<int16_t>(x + stepX), static_cast<int16_t>(y)};
            if (next.x >= 0 && next.x < mapLen && next.y >= 0 && next.y < mapHei)
            {
                // probe canMove from the current cell so the entered cell is checked
                sprite.move(Pos{static_cast<int16_t>(x), static_cast<int16_t>(y)});
                JoyAim aim = (stepX > 0 ? AIM_RIGHT : AIM_LEFT);
                if (sprite.canMove(aim))
                {
                    directions.push_back(aim);
                    x += stepX;
                    stepsX--;
                    moved = true;
                }
                sprite.move(originalPos);
            }
        }
        // Then try y movement
        if (!moved && y != goalPos.y)
        {
            Pos next = {static_cast<int16_t>(x), static_cast<int16_t>(y + stepY)};
            if (next.x >= 0 && next.x < mapLen && next.y >= 0 && next.y < mapHei)
            {
                // probe canMove from the current cell so the entered cell is checked
                sprite.move(Pos{static_cast<int16_t>(x), static_cast<int16_t>(y)});
                JoyAim aim = (stepY > 0 ? AIM_DOWN : AIM_UP);
                if (sprite.canMove(aim))
                {
                    directions.push_back(aim);
                    y += stepY;
                    stepsY--;
                    moved = true;
                }
                sprite.move(originalPos);
            }
        }
        if (!moved)
            return {};
    }

    return directions;
}

std::vector<JoyAim> Raycast::findPath(ISprite &sprite, const Pos &playerPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor; // Half-tile bounds
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos(); // Half-tile coordinates

    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        playerPos.x < 0 || playerPos.x >= mapLen || playerPos.y < 0 || playerPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, playerPos.x, playerPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Generalized Bresenham ray: exactly dx + dy unit steps, staircase
    // interleaved. Both endpoints are in bounds and every step moves one
    // coordinate toward its goal value, so all cells stay inside the
    // start/goal bounding box (no per-step bounds check needed). No
    // walkability probe on purpose: the ray keeps its final step into a
    // blocking cell so CPath::followPath reports Blocked and handleBullet
    // detonates the bullet at the wall; a probe-trimmed partial ray would
    // exhaust to NoValidPath and leave a NoTTL bullet parked forever.
    std::vector<JoyAim> directions;
    const int dx = std::abs(playerPos.x - startPos.x);
    const int dy = std::abs(playerPos.y - startPos.y);
    const int stepX = playerPos.x >= startPos.x ? 1 : -1;
    const int stepY = playerPos.y >= startPos.y ? 1 : -1;
    int x = startPos.x;
    int y = startPos.y;
    int err = dx - dy;
    while (x != playerPos.x || y != playerPos.y)
    {
        const int e2 = 2 * err;
        if (e2 > -dy) // x advance (never taken when dx == 0)
        {
            err -= dy;
            x += stepX;
            directions.push_back(stepX > 0 ? AIM_RIGHT : AIM_LEFT);
        }
        if (e2 < dx) // y advance (never taken when dy == 0)
        {
            err += dx;
            y += stepY;
            directions.push_back(stepY > 0 ? AIM_DOWN : AIM_UP);
        }
    }
    return directions;
}

// ponytail: RaycastBullet is Raycast's twin with a different step order;
// the two coincide for axis-aligned and exact 45-degree rays. If the
// geometric-ray family grows a third member, share one loop via a
// tie-order strategy instead of a third copy.
std::vector<JoyAim> RaycastBullet::findPath(ISprite &sprite, const Pos &playerPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor; // Half-tile bounds
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos(); // Half-tile coordinates

    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        playerPos.x < 0 || playerPos.x >= mapLen || playerPos.y < 0 || playerPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, playerPos.x, playerPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Amanatides & Woo grid traversal (fast voxel DDA) in integer
    // rationals: the next vertical grid crossing sits at line-parameter
    // (i+1)/dx and the next horizontal one at (j+1)/dy; compare them by
    // cross-multiplication, no floats. Unlike Raycast's balanced
    // staircase this emits axis crossings in line-parameter order, so the
    // visited cell sequence is exactly the supercover of the segment (the
    // cells the line actually passes through) -- the physically-correct
    // collision sequence for a bullet flying the sight ray. Exact-corner
    // ties emit the x step first (the canonical x-first convention shared
    // with AStarSmooth's staircase oracle), so both corner-adjacent cells
    // are visited and a bullet never leaks through a diagonal wall seam.
    // Products are bounded by (half-tile extent)^2; int64_t removes any
    // overflow doubt.
    // No walkability probe on purpose, same contract as Raycast: the ray
    // keeps its final step into a blocking cell so CPath::followPath
    // reports Blocked and handleBullet detonates the bullet at the wall;
    // a probe-trimmed partial ray would exhaust to NoValidPath and leave a
    // NoTTL bullet parked forever.
    std::vector<JoyAim> directions;
    const int dx = std::abs(playerPos.x - startPos.x);
    const int dy = std::abs(playerPos.y - startPos.y);
    const int stepX = playerPos.x >= startPos.x ? 1 : -1;
    const int stepY = playerPos.y >= startPos.y ? 1 : -1;
    int i = 0; // vertical crossings consumed
    int j = 0; // horizontal crossings consumed
    while (i < dx || j < dy)
    {
        if (j >= dy || (i < dx && static_cast<int64_t>(i + 1) * dy <= static_cast<int64_t>(j + 1) * dx))
        {
            ++i;
            directions.push_back(stepX > 0 ? AIM_RIGHT : AIM_LEFT);
        }
        else
        {
            ++j;
            directions.push_back(stepY > 0 ? AIM_DOWN : AIM_UP);
        }
    }
    return directions;
}

// ponytail: the field is rebuilt per call; findPath is const on a shared
// PathData singleton, so there is no legitimate place to cache it. If a
// swarm feature ever needs one field for N agents with the same goal, give
// that feature an explicit FlowFieldBuilder keyed on (goal, footprint, map
// epoch) -- do not bolt mutable state onto this instance.
std::vector<JoyAim> FlowField::findPath(ISprite &sprite, const Pos &goalPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor;
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos();

    // Validate start and goal positions
    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Integration field, indexed y * mapLen + x: g_steps index of the
    // downhill step per cell, -1 unreached, TOTAL_AIMS sentinel at the goal
    std::vector<int8_t> flow(static_cast<size_t>(mapLen) * mapHei, -1);
    flow[static_cast<size_t>(goalPos.y) * mapLen + goalPos.x] = static_cast<int8_t>(TOTAL_AIMS);
    std::queue<Pos> queue;
    queue.push(goalPos);

    // Reverse BFS: admit the forward edge prev -> cur with the same
    // predicate the forward searches apply when expanding prev, so reverse
    // reachability from the goal is exactly forward reachability to it.
    // First-touch wins (BFS order => dist(prev) = dist(cur) + 1 is
    // minimal), so there is no relaxation pass and no Node/OpenList to
    // corrupt (BUG-C9/BUG-H17 machinery is structurally absent).
    const Pos originalPos{sprite.pos()};
    while (!queue.empty())
    {
        const Pos cur = queue.front();
        queue.pop();
        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            const Pos prev{static_cast<int16_t>(cur.x - g_steps[i].delta.x),
                           static_cast<int16_t>(cur.y - g_steps[i].delta.y)};
            if (prev.x < 0 || prev.x >= mapLen || prev.y < 0 || prev.y >= mapHei)
                continue;
            int8_t &cell = flow[static_cast<size_t>(prev.y) * mapLen + prev.x];
            if (cell != -1)
                continue;
            // probe canMove from the predecessor so the entered cell is checked
            sprite.move(prev);
            if (sprite.canMove(g_steps[i].dir))
            {
                cell = static_cast<int8_t>(i);
                queue.push(prev);
            }
        }
    }
    sprite.move(originalPos);

    // Gradient descent from the start. Every stored step strictly decreases
    // the BFS distance, so the descent terminates without a step cap.
    std::vector<JoyAim> directions;
    Pos cellPos = startPos;
    while (!(cellPos == goalPos))
    {
        const int8_t i = flow[static_cast<size_t>(cellPos.y) * mapLen + cellPos.x];
        if (i < 0)
        {
            LOGE("FlowField: no path from (%d,%d) to (%d,%d) on line %d",
                 startPos.x, startPos.y, goalPos.x, goalPos.y, __LINE__);
            return {};
        }
        directions.push_back(g_steps[i].dir);
        cellPos = {static_cast<int16_t>(cellPos.x + g_steps[i].delta.x),
                   static_cast<int16_t>(cellPos.y + g_steps[i].delta.y)};
    }
    return directions;
}

/////////////////////////////////////////////////////////////////////

int AStarSmooth::manhattanDistance(const Pos &a, const Pos &b) const
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

namespace
{
    // aim for the unit step between consecutive A* path cells, resolved
    // through the shared g_steps table (mirrors the searches' labelling)
    JoyAim unitAim(const Pos &from, const Pos &to)
    {
        for (const auto &step : PathData::g_steps)
        {
            const Pos next{static_cast<int16_t>(from.x + step.delta.x),
                           static_cast<int16_t>(from.y + step.delta.y)};
            if (next == to)
                return step.dir;
        }
        return AIM_NONE;
    }

    // deterministic x-first staircase probe; fills `steps` and returns true
    // only if every step's canMove probe succeeds (shared AStarSmooth/ThetaStar)
    bool staircaseSteps(ISprite &sprite, const Pos &from, const Pos &to,
                        std::vector<JoyAim> &steps)
    {
        steps.clear();
        const int granularFactor = sprite.getGranularFactor();
        const CMap &map = CGame::getMap();
        const int mapLen = map.width() * granularFactor; // Half-tile bounds
        const int mapHei = map.height() * granularFactor;
        const Pos originalPos = sprite.pos();
        Pos cur = from;
        bool ok = true;
        // deterministic x-first staircase: full horizontal leg, then vertical;
        // unlike the greedy LineOfSight router there is no axis fallback, so a
        // clear walk means the segment genuinely sees through the wall gaps of
        // this exact route, not some detour
        for (int leg = 0; leg < 2 && ok; ++leg)
        {
            while ((leg == 0) ? (cur.x != to.x) : (cur.y != to.y))
            {
                const int16_t step = (leg == 0)
                                         ? static_cast<int16_t>(to.x > cur.x ? 1 : -1)
                                         : static_cast<int16_t>(to.y > cur.y ? 1 : -1);
                const JoyAim aim = (leg == 0)
                                       ? (step > 0 ? AIM_RIGHT : AIM_LEFT)
                                       : (step > 0 ? AIM_DOWN : AIM_UP);
                const Pos next{static_cast<int16_t>(cur.x + (leg == 0 ? step : 0)),
                               static_cast<int16_t>(cur.y + (leg == 1 ? step : 0))};
                if (next.x < 0 || next.x >= mapLen || next.y < 0 || next.y >= mapHei)
                {
                    ok = false;
                    break;
                }
                // probe canMove from the cell being left so the entered cell is checked
                sprite.move(cur);
                const bool canStep = sprite.canMove(aim);
                if (!canStep)
                {
                    ok = false;
                    break;
                }
                steps.push_back(aim);
                cur = next;
            }
        }
        sprite.move(originalPos);
        if (!ok)
            steps.clear();
        return ok;
    }
}

bool AStarSmooth::segmentSteps(ISprite &sprite, const Pos &from, const Pos &to,
                               std::vector<JoyAim> &steps) const
{
    return staircaseSteps(sprite, from, to, steps);
}

std::vector<JoyAim> AStarSmooth::smoothPath(const std::vector<Pos> &path, ISprite &sprite) const
{
    if (path.size() < 2)
        return {};
    std::vector<JoyAim> directions;
    for (size_t i = 1; i < path.size();)
    {
        // extend the anchor greedily; segmentSteps is the single probe
        // walk (no second execution to emit) and the last accepted run is
        // the staircase for path[j-1]
        std::vector<JoyAim> best;
        size_t j = i + 1;
        std::vector<JoyAim> steps;
        while (j < path.size() && segmentSteps(sprite, path[i - 1], path[j], steps))
        {
            best = std::move(steps);
            ++j;
        }
        if (best.empty())
        {
            // fail-soft: path[i-1]->path[i] is an edge the search itself
            // validated with the same canMove probes, so the oracle can
            // only refuse it on a sprite-model inconsistency; walk the
            // single unit step instead of killing the whole path
            const JoyAim aim = unitAim(path[i - 1], path[i]);
            if (aim == AIM_NONE)
            {
                LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                     path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                return {};
            }
            LOGW("oracle rejected search-validated step (%d,%d)->(%d,%d)",
                 path[i - 1].x, path[i - 1].y, path[i].x, path[i].y);
            directions.push_back(aim);
            i = j; // == i + 1
        }
        else
        {
            directions.insert(directions.end(), best.begin(), best.end());
            i = j;
        }
    }
    return directions;
}

std::vector<JoyAim> AStarSmooth::findPath(ISprite &sprite, const Pos &playerPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor; // Half-tile bounds
    const int mapHei = map.height() * granularFactor;
    const Pos startPos = sprite.pos(); // Half-tile coordinates
    const Pos goalPos = playerPos;

    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    OpenList openList;
    std::unordered_map<Pos, std::unique_ptr<Node>> nodes;
    std::unordered_map<Pos, bool> closedList;

    nodes[startPos] = std::make_unique<Node>(startPos, 0, manhattanDistance(startPos, goalPos), nullptr);
    openList.emplace(nodes[startPos]->fCost(), nodes[startPos].get());

    while (!openList.empty())
    {
        const OpenEntry entry = openList.top();
        openList.pop();
        Node *current = entry.second;

        // BUG-H17: skip stale snapshots left behind by g-cost relaxation
        if (entry.first != current->fCost())
            continue;

        // BUG-C9: relaxation re-pushes the same node; skip stale copies
        if (closedList.find(current->pos) != closedList.end())
            continue;

        if (current->pos == goalPos)
        {
            std::vector<Pos> path;
            Node *node = current;
            while (node != nullptr)
            {
                path.push_back(node->pos);
                node = node->parent;
            }
            std::reverse(path.begin(), path.end());
            return smoothPath(path, sprite); // Apply smoothing
        }

        closedList[current->pos] = true;

        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            const Pos newPos = {static_cast<int16_t>(current->pos.x + g_steps[i].delta.x), static_cast<int16_t>(current->pos.y + g_steps[i].delta.y)};
            // find(), not operator[]: BUG-C9's pop skip treats map membership
            // as closed, so no false-valued probes may enter closedList
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei ||
                closedList.find(newPos) != closedList.end())
                continue;

            // probe canMove from the expanded node so the entered cell is checked
            const Pos originalPos = sprite.pos();
            sprite.move(current->pos);
            const bool canStep = sprite.canMove(g_steps[i].dir);
            sprite.move(originalPos);
            if (!canStep)
                continue;

            int newGCost = current->gCost + 1;
            // find() first: operator[] here default-inserted a *null* entry for
            // every probed neighbor and re-hashed on each access
            auto it = nodes.find(newPos);
            if (it == nodes.end())
            {
                nodes[newPos] = std::make_unique<Node>(newPos, newGCost, manhattanDistance(newPos, goalPos), current);
                openList.emplace(nodes[newPos]->fCost(), nodes[newPos].get());
            }
            else if (newGCost < it->second->gCost)
            {
                // BUG-C9: relax in place (see AStar); replacing the unique_ptr
                // would free the Node still referenced by openList.
                // BUG-H17: snapshot keys make the in-place update safe.
                it->second->gCost = newGCost;
                it->second->parent = current;
                openList.emplace(it->second->fCost(), it->second.get());
            }
        }
    }
    return {};
}

int ThetaStar::manhattanDistance(const Pos &a, const Pos &b) const
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

// ponytail: Manhattan-metric Theta*, not Euclidean any-angle Theta*: the
// JoyAim wire format only expresses unit cardinal steps, so dist() here is
// Manhattan and gCost stays equal to the emitted step count. That keeps the
// length exactly A*-optimal (unit edges are always relaxed too) while the
// grandparent relaxation merges collinear runs during the search instead of
// in AStarSmooth's after-pass. Upgrade path: Euclidean any-angle Theta*,
// blocked until JoyAim can express diagonals.
// ponytail: node loop is the fourth copy (AStar, AStarSmooth, Dijkstra,
// here); a shared loop needs a relaxation callback since the successor
// block here differs by design. Extract when a fifth searcher or a common
// change forces it.
std::vector<JoyAim> ThetaStar::findPath(ISprite &sprite, const Pos &goalPos) const
{
    const int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    const int mapLen = map.width() * granularFactor;
    const int mapHei = map.height() * granularFactor;
    std::vector<JoyAim> directions;
    const Pos startPos = sprite.pos();

    // Validate start and goal positions
    if (startPos.x < 0 || startPos.x >= mapLen || startPos.y < 0 || startPos.y >= mapHei ||
        goalPos.x < 0 || goalPos.x >= mapLen || goalPos.y < 0 || goalPos.y >= mapHei)
    {
        LOGE("Invalid start (%d,%d) or goal (%d,%d) for map bounds (%d,%d) on line %d",
             startPos.x, startPos.y, goalPos.x, goalPos.y, mapLen, mapHei, __LINE__);
        return {};
    }

    // Priority queue for open list
    OpenList openList;
    std::unordered_map<Pos, std::unique_ptr<Node>> nodes;
    std::unordered_map<Pos, bool> closedList;

    // Create start node
    nodes[startPos] = std::make_unique<Node>(startPos, 0, manhattanDistance(startPos, goalPos), nullptr);
    openList.emplace(nodes[startPos]->fCost(), nodes[startPos].get());

    while (!openList.empty())
    {
        const OpenEntry entry = openList.top();
        openList.pop();
        Node *current = entry.second;

        // BUG-H17: skip stale snapshots left behind by g-cost relaxation
        if (entry.first != current->fCost())
            continue;

        // BUG-C9: relaxation re-pushes the same node; skip stale copies
        if (closedList.find(current->pos) != closedList.end())
            continue;

        // Reached goal
        if (current->pos == goalPos)
        {
            // Reconstruct: parent-chain points may be several cells apart
            std::vector<Pos> path;
            Node *node = current;
            while (node != nullptr)
            {
                path.push_back(node->pos);
                node = node->parent;
            }
            std::reverse(path.begin(), path.end());

            // Replay each segment; the map is static and the oracle is
            // deterministic, so it must accept what the relaxation probed
            std::vector<JoyAim> seg;
            for (size_t i = 1; i < path.size(); ++i)
            {
                const int dx = path[i].x - path[i - 1].x;
                const int dy = path[i].y - path[i - 1].y;
                if (abs(dx) + abs(dy) == 1)
                {
                    const JoyAim aim = unitAim(path[i - 1], path[i]);
                    if (aim == AIM_NONE)
                    {
                        LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                             path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                        return {};
                    }
                    directions.push_back(aim);
                    continue;
                }
                // unlike AStarSmooth the points are not adjacent, so no
                // fail-soft single step here: a refusal means the search
                // validated an inconsistent segment
                if (!staircaseSteps(sprite, path[i - 1], path[i], seg))
                {
                    LOGE("Invalid path transition from (%d,%d) to (%d,%d) on line %d",
                         path[i - 1].x, path[i - 1].y, path[i].x, path[i].y, __LINE__);
                    return {};
                }
                directions.insert(directions.end(), seg.begin(), seg.end());
            }
            return directions;
        }

        closedList[current->pos] = true;

        // Explore neighbors
        for (size_t i = 0; i < std::size(g_steps); ++i)
        {
            const Pos newPos{static_cast<int16_t>(current->pos.x + g_steps[i].delta.x),
                             static_cast<int16_t>(current->pos.y + g_steps[i].delta.y)};

            // Check bounds before moving
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei)
                continue;

            if (closedList.find(newPos) != closedList.end())
                continue;

            // probe canMove from the expanded node so the entered cell is checked
            const Pos originalPos{sprite.pos()};
            sprite.move(current->pos);
            const bool canStep = sprite.canMove(g_steps[i].dir);
            sprite.move(originalPos);
            if (!canStep)
                continue;

            // Theta* relaxation: prefer the grandparent (anchor) when a
            // clear staircase segment reaches newPos through it
            int newGCost = current->gCost + 1;
            Node *newParent = current;
            Node *anchor = current->parent; // may be null (start expansion)
            if (anchor != nullptr)
            {
                const int viaAnchor = anchor->gCost + manhattanDistance(anchor->pos, newPos);
                std::vector<JoyAim> seg;
                // cheap bound first: only probe the oracle when the grandparent could
                // tie or beat the unit edge (tie goes to the straighter parent)
                if (viaAnchor <= newGCost && staircaseSteps(sprite, anchor->pos, newPos, seg))
                {
                    newGCost = viaAnchor;
                    newParent = anchor;
                }
            }

            int newHCost = manhattanDistance(newPos, goalPos);

            auto it = nodes.find(newPos);
            if (it == nodes.end())
            {
                nodes[newPos] = std::make_unique<Node>(newPos, newGCost, newHCost, newParent);
                openList.emplace(nodes[newPos]->fCost(), nodes[newPos].get());
            }
            else if (newGCost < it->second->gCost)
            {
                // BUG-C9: relax in place (see AStar); replacing the unique_ptr
                // would free the Node still referenced by openList.
                // BUG-H17: snapshot keys make the in-place update safe.
                it->second->gCost = newGCost;
                it->second->parent = newParent;
                openList.emplace(it->second->fCost(), it->second.get());
            }
        }
    }
    // No path found
    return directions; // Empty if no path found
}

////////////////////////////////////////////////
CPath::Result CPath::followPath(ISprite &sprite, const Pos &playerPos, const IPath &astar)
{
    // Maintainer ruling 2026-11-18: standing on the goal needs no search;
    // skip the (expensive, always-empty) recompute instead of re-running it.
    if (sprite.pos() == playerPos)
        return Result::NoValidPath;

    // Check if path is invalid or timed out
    if (m_pathIndex >= m_cachedDirections.size() || m_pathTimeout == 0)
    {
        m_cachedDirections = astar.findPath(sprite, playerPos);
        m_pathIndex = 0;
        if (!m_pathTimeout)
            m_pathTimeout = PATH_TIMEOUT_MAX;
        if (m_cachedDirections.empty())
            return Result::NoValidPath; // No valid path
    }

    // Try the next direction
    const JoyAim aim = m_cachedDirections[m_pathIndex];
    sprite.setAim(aim);
    if (sprite.canMove(aim))
    {
        if (sprite.isBoss())
        {
            sprite.move(aim);
        }
        else
        {
            // unchecked downcast was UB for any other ISprite; fail safe
            CActor *actor = dynamic_cast<CActor *>(&sprite);
            if (!actor)
            {
                LOGE("CPath::followPath: non-boss sprite %p is not a CActor",
                     static_cast<void *>(&sprite));
                return Result::Blocked;
            }
            // A pathing CActor must never occupy the player's cell: the
            // CActor::move tile swap would displace the player's stamped
            // map identity. MONSTERV3 canMove() admits that cell only so
            // the A* goal is reachable.
            if (actor->isPlayerThere(aim))
                return Result::NoValidPath;
            CGame::getGame()->shadowActorMove(*actor, aim);
        }
        ++m_pathIndex;
        --m_pathTimeout;
        return Result::MoveSuccesful;
    }

    // Move failed, invalidate cache and recompute next turn
    m_cachedDirections.clear();
    m_pathIndex = 0;
    m_pathTimeout = 0;
    return Result::Blocked;
}

bool CPath::read(IFile &sfile)
{
    auto readfile = [&sfile](auto ptr, auto size) -> bool
    {
        return sfile.read(ptr, size) == IFILE_OK;
    };

    // all three gated fields are size_t after the 0x0011 uint16-local reads
    auto checkBound = [](const char *name, size_t value, size_t max) -> bool
    {
        if (value > max)
        {
            LOGE("%s: %zu outside expected -- expected <= %zu", name, value, max);
            return false;
        }
        return true;
    };

    constexpr size_t DATA_SIZE = 2;
    //////////////////////////////////////
    // Read Path (saved)
    m_cachedDirections.clear();
    uint16_t u16 = 0;
    _R(&u16, DATA_SIZE);
    size_t pathSize = u16;
    if (!checkBound("pathSize", pathSize, MAX_PATH_SIZE))
        return false;
    for (size_t i = 0; i < pathSize; ++i)
    {
        JoyAim dir;
        _R(&dir, sizeof(dir));
        // cached directions only ever hold AIM_UP..AIM_RIGHT (see g_steps);
        // AIM_NONE/0xff and other garbage never legitimately reach the wire
        if (dir >= TOTAL_AIMS)
        {
            LOGE("CPath::read: corrupt direction %.2x", dir);
            return false;
        }
        m_cachedDirections.emplace_back(dir);
    }
    m_pathIndex = 0;
    _R(&u16, DATA_SIZE);
    m_pathIndex = u16;
    if (!checkBound("m_pathIndex", m_pathIndex, MAX_PATH_SIZE))
        return false;
    m_pathTimeout = 0;
    _R(&u16, DATA_SIZE);
    m_pathTimeout = u16;
    if (!checkBound("m_pathTimeout", m_pathTimeout, MAX_PATH_SIZE))
        return false;

    return true;
}

bool CPath::write(IFile &tfile)
{
    auto writefile = [&tfile](auto ptr, auto size)
    {
        return tfile.write(ptr, size) == IFILE_OK;
    };

    constexpr size_t DATA_SIZE = 2;
    // values are bounded by MAX_PATH_SIZE / PATH_TIMEOUT_MAX, so the
    // uint16 wire fields cannot truncate
    const uint16_t pathSize = static_cast<uint16_t>(m_cachedDirections.size());
    _W(&pathSize, DATA_SIZE);
    for (const auto &dir : m_cachedDirections)
    {
        _W(&dir, sizeof(dir));
    }
    const uint16_t pathIndex = static_cast<uint16_t>(m_pathIndex);
    const uint16_t pathTimeout = static_cast<uint16_t>(m_pathTimeout);
    _W(&pathIndex, DATA_SIZE);
    _W(&pathTimeout, DATA_SIZE);

    return true;
}

CPath::CPath()
{
    m_pathIndex = 0;
    m_pathTimeout = 0;
}

const IPath *CPath::getPathAlgo(const uint8_t algo)
{
    if (algo == BossData::ASTAR)
    {
        return &PathData::aStar;
    }
    else if (algo == BossData::LOS)
    {
        return &PathData::lineOfSight;
    }
    else if (algo == BossData::BFS)
    {
        return &PathData::bFS;
    }
    else if (algo == BossData::ASTAR_SMOOTH)
    {
        return &PathData::aStarSmooth;
    }
    else if (algo == BossData::DIJKSTRA)
    {
        return &PathData::dijkstra;
    }
    else if (algo == BossData::FLOWFIELD)
    {
        return &PathData::flowField;
    }
    else if (algo == BossData::THETASTAR)
    {
        return &PathData::thetaStar;
    }
    else if (algo == BossData::RAYCAST)
    {
        return &PathData::raycast;
    }
    else if (algo == BossData::RAYCAST_BULLET)
    {
        return &PathData::raycastBullet;
    }
    else
    {
        LOGE("unsupported ai algo: %u", algo);
        return nullptr;
    }
}

void CPath::setTimeout(int timeout)
{
    // m_pathTimeout is size_t: a negative argument would wrap to a huge
    // value and disable path expiry
    m_pathTimeout = timeout < 0 ? 0 : static_cast<size_t>(timeout);
}