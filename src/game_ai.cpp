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

#include "map.h"
#include "boss.h"

constexpr int TARGET_DISTANCE = 10;

// Represents a grid cell (node) in the A* algorithm
struct Node
{
    // int x, y;     // Coordinates
    Pos pos;
    int gCost;    // Cost from start to this node
    int hCost;    // Heuristic (estimated cost to goal)
    Node *parent; // Parent node for path reconstruction

    Node(Pos pos, int g = 0, int h = 0, Node *p = nullptr)
        : pos(pos), gCost(g), hCost(h), parent(p) {}

    // Total cost (f = g + h) for priority queue
    int fCost() const { return gCost + hCost; }
};

// Comparator for priority queue (min heap based on fCost)
struct CompareNode
{
    bool operator()(const Node *a, const Node *b) const
    {
        return a->fCost() > b->fCost(); // Lower fCost has higher priority
    }
};

// A* Pathfinding class
class AStar
{
public:
    uint32_t toKey(const Pos &pos) const
    {
        return pos.x | (pos.y << 16);
    }

    std::vector<Pos> findPath(
        const CBoss &boss,
        const Pos goal) const
    {
        const Pos start{.x = boss.x(), .y = boss.y()};
        CBoss m(boss);

        // Check if start or goal are valid
        /*
        if (!isValid(start.first, start.second, rows, cols) ||
            !isValid(goal.first, goal.second, rows, cols) ||
            grid[start.first][start.second] == 1 ||
            grid[goal.first][goal.second] == 1)
        {
            return {}; // Return empty path if invalid
        }*/

        // Priority queue for open list
        std::priority_queue<Node *, std::vector<Node *>, CompareNode> openList;
        std::unordered_map<uint32_t, Node *> nodes;
        std::unordered_map<uint32_t, bool> closedList;

        // Create start node
        Node *startNode = new Node(start, 0, manhattanDistance(start, goal));
        openList.push(startNode);
        nodes[toKey(start)] = startNode;

        const Pos deltas[] = {
            {-1, 0}, // Up
            {1, 0},  // Down
            {0, -1}, // Left
            {0, 1},  // Right
        };
        const JoyAim aims[] = {JoyAim::AIM_UP, JoyAim::AIM_DOWN, JoyAim::AIM_LEFT, JoyAim::AIM_RIGHT};

        while (!openList.empty())
        {
            Node *current = openList.top();
            openList.pop();
            closedList[toKey(current->pos)] = true;

            // Reached goal
            if (current->pos.x == goal.x && current->pos.y == goal.y)
            {
                std::vector<Pos> path = reconstructPath(current);
                cleanup(nodes);
                return path;
            }

            // Explore neighbors
            size_t count = sizeof(deltas) / sizeof(deltas[0]);
            for (size_t i = 0; i < count; ++i)
            {
                m.move(current->pos.x, current->pos.y);
                const auto &delta = deltas[i];
                const Pos newPos{static_cast<int16_t>(current->pos.x + delta.x), static_cast<int16_t>(current->pos.y + delta.y)};
                const auto newKey = toKey(newPos);
                if (!m.canMove(aims[i]) || closedList[newKey])
                    continue;

                int newGCost = current->gCost + 1; // Cost to move to neighbor
                Node *neighbor = nodes[newKey];
                if (!neighbor)
                {
                    neighbor = new Node(newPos, newGCost, manhattanDistance(newPos, goal), current);
                    nodes[newKey] = neighbor;
                    openList.push(neighbor);
                }
                else if (newGCost < neighbor->gCost)
                {
                    neighbor->gCost = newGCost;
                    neighbor->parent = current;
                    openList.push(neighbor); // Re-push to update priority
                }
            }
        }

        // No path found
        cleanup(nodes);
        return {};
    }

private:
    // Manhattan distance heuristic
    int manhattanDistance(const Pos &a, const Pos &b) const
    {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    // Reconstruct path from goal to start
    std::vector<Pos> reconstructPath(Node *node) const
    {
        std::vector<Pos> path;
        while (node)
        {
            path.emplace_back(node->pos);
            node = node->parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    // Clean up dynamically allocated nodes
    // void cleanup(std::vector<std::vector<Node *>> &nodes) const
    void cleanup(std::unordered_map<uint32_t, Node *> &nodes) const
    {
        for (auto &[k, v] : nodes)
            delete v;
    }
};

void CGame::manageBosses(const int ticks)
{
    const AStar aStar;
    CActor &player = m_player;
    for (auto &boss : m_bosses)
    {
        const int bx = boss.x() / 2;
        const int by = boss.y() / 2;
        if (boss.speed() != 0 && (ticks % boss.speed() != 0))
            continue;
        if (boss.isPlayerHere())
        {
            addHealth(-1);
        }

        if (boss.state() == BossState::Patrol)
        {
            JoyAim aim = static_cast<JoyAim>(rand() & 3);
            if (boss.canMove(aim))
            {
                boss.move(aim);
            }

            if (boss.distance(player) <= TARGET_DISTANCE)
            {
                boss.setState(BossState::Chase);
            }
        }
        else if (boss.state() == BossState::Chase)
        {
            if (boss.distance(player) > TARGET_DISTANCE)
            {
                boss.setState(BossState::Patrol);
                continue;
            }

            Pos playerPos{static_cast<int16_t>(m_player.getX() * CBoss::BOSS_GRANULAR_FACTOR),
                          static_cast<int16_t>(m_player.getY() * CBoss::BOSS_GRANULAR_FACTOR)};
            const auto path = aStar.findPath(boss, playerPos);
            if (path.size() > 1)
            {
                boss.move(path[1].x, path[1].y);
                continue;
            }

            if (bx < player.getX() && boss.canMove(JoyAim::AIM_RIGHT))
            {
                boss.move(JoyAim::AIM_RIGHT);
            }

            if (bx > player.getX() && boss.canMove(JoyAim::AIM_LEFT))
            {
                boss.move(JoyAim::AIM_LEFT);
            }

            if (by < player.getY() && boss.canMove(JoyAim::AIM_DOWN))
            {
                boss.move(JoyAim::AIM_DOWN);
            }

            if (by > player.getY() && boss.canMove(JoyAim::AIM_UP))
            {
                boss.move(JoyAim::AIM_UP);
            }
        }
    }
}
