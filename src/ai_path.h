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

#pragma once
#include <vector>
#include "sprtypes.h"
#include "rect.h"
#include "joyaim.h"
#include "map.h"

class ISprite;
class IFile;

class IPath
{
public:
    virtual std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const = 0;
    // True for searchers whose goal is "farthest from playerPos", not
    // "standing on it". Only Flee overrides; all shipped algos inherit
    // false and are bit-identical.
    virtual bool flees() const { return false; }
};

// A* Pathfinding class
class AStar : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
};

// A* Pathfinding class
class AStarSmooth : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
    std::vector<JoyAim> smoothPath(const std::vector<Pos> &path, ISprite &sprite) const;
    // probes the deterministic x-first staircase between two in-bounds
    // cells; fills `steps` and returns true only if every step's canMove
    // probe succeeds (no axis fallback, no routing around anything)
    bool segmentSteps(ISprite &sprite, const Pos &from, const Pos &to,
                      std::vector<JoyAim> &steps) const;
};

// Dijkstra (uniform-cost) Pathfinding class: A* with h = 0
class Dijkstra : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// BFS Pathfinding class
class BFS : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// Line-of-Sight Pathfinding class
class LineOfSight : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// Raycast "pathfinding" class: Bresenham straight-line ray from sprite to
// goal (4-dir staircase), no walkability probe and no axis fallback: the
// bullet attack that flies the sight ray and detonates at the first wall
// (CPath::followPath's canMove() probe reports Blocked there).
class Raycast : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// Raycast-bullet Pathfinding class: Amanatides & Woo DDA traversal of the
// segment sprite to goal (the exact cells the line passes through, x-first
// corner ties), no walkability probe and no axis fallback (same runtime-
// resolved-ray contract as Raycast, see cpp comment). Differs from
// Raycast only in step order on non-1:1/non-axis slopes.
class RaycastBullet : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// Flow Field Pathfinding class: reverse BFS from the goal, then gradient descent
class FlowField : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
};

// Theta* Pathfinding class: A* whose relaxation prefers line-of-sight
// grandparents (Manhattan costs; unit-step 4-dir output, see cpp comment)
class ThetaStar : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
};

// Capped A* Pathfinding class: AStar's node loop with a hard expansion
// budget; on exhaustion it returns the path to the popped node closest to
// the goal (partial success, see cpp comment) instead of empty. A boxed-in
// start still reconstructs to {} so "empty = no path" holds.
class AStarBudget : public IPath
{
public:
    explicit AStarBudget(int budget = 256);
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
    int m_budget; // const instance uses the default; tests inject tight budgets
};

// Greedy best-first (GBFS) Pathfinding class: AStarBudget's node loop with
// the open-list key reduced to (hCost, gCost) pairs. NON-optimal by
// construction (the budget and partial-path fallback match AStarBudget).
class Greedy : public IPath
{
public:
    explicit Greedy(int budget = 256);
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
    int m_budget;
};

// Flee Pathfinding class: bounded BFS (FLEE_RADIUS) around the sprite;
// answer = the reachable cell maximizing Manhattan distance from the
// threat position. flees() is true, so CPath::followPath skips its
// on-goal short-circuit for it (see cpp comment).
class Flee : public IPath
{
public:
    std::vector<JoyAim> findPath(ISprite &sprite, const Pos &playerPos) const override;
    bool flees() const override { return true; }

private:
    int manhattanDistance(const Pos &a, const Pos &b) const;
};

class CPath
{
public:
    CPath();
    ~CPath() {};

    enum Result
    {
        Blocked, // Blocked
        MoveSuccesful,
        NoValidPath,
        NotConfigured
    };

    Result followPath(ISprite &sprite, const Pos &playerPos, const IPath &astar);
    bool read(IFile &file);
    bool write(IFile &file);
    void setTimeout(int timeout);
    static const IPath *getPathAlgo(const uint8_t algo);

private:
    // Path caching
    std::vector<JoyAim> m_cachedDirections;
    size_t m_pathIndex;
    size_t m_pathTimeout;
};