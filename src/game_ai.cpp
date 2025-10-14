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
#include <memory>
#include <algorithm>
#include <set>
#include <random>
#include "map.h"
#include "boss.h"
#include "attr.h"
#include "tilesdata.h"
#include "sprtypes.h"
#include "logger.h"
#include "actor.h"
#include "game_ai.h"
#include "randomz.h"

constexpr JoyAim g_dirs[] = {AIM_UP, AIM_DOWN, AIM_LEFT, AIM_RIGHT};
constexpr int PURSUIT_DISTANCE = 15;
constexpr int CHASE_DISTANCE = 10;
constexpr int TARGET_DISTANCE = 5;
constexpr std::array<Pos, 4> g_deltas = {
    Pos{-1, 0}, // Up
    Pos{1, 0},  // Down
    Pos{0, -1}, // Left
    Pos{0, 1},  // Right
};

namespace std
{
    template <>
    struct hash<Pos>
    {
        size_t operator()(const Pos &pos) const
        {
            return (static_cast<uint32_t>(pos.x) << 16) | (pos.y & 0xFFFF);
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

// Comparator for priority queue (min heap based on fCost)
struct CompareNode
{
    bool operator()(const Node *a, const Node *b) const
    {
        return a->fCost() > b->fCost(); // Lower fCost has higher priority
    }
};

int AStar::manhattanDistance(const Pos &a, const Pos &b) const
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

std::vector<JoyAim> AStar::findPath(ISprite &sprite, const Pos &goalPos) const
{
    int granularFactor = sprite.getGranularFactor();
    const CMap &map = CGame::getMap();
    int mapLen = map.len() * granularFactor;
    int mapHei = map.hei() * granularFactor;
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
    std::priority_queue<Node *, std::vector<Node *>, CompareNode> openList;
    std::unordered_map<Pos, std::unique_ptr<Node>> nodes;
    std::unordered_map<Pos, bool> closedList;

    // Create start node
    nodes[startPos] = std::make_unique<Node>(startPos, 0, manhattanDistance(startPos, goalPos), nullptr);
    openList.push(nodes[startPos].get());

    while (!openList.empty())
    {
        Node *current = openList.top();
        openList.pop();

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
        for (size_t i = 0; i < g_deltas.size(); ++i)
        {
            const Pos newPos{static_cast<int16_t>(current->pos.x + g_deltas[i].x),
                             static_cast<int16_t>(current->pos.y + g_deltas[i].y)};

            // Check bounds before moving
            if (newPos.x < 0 || newPos.x >= mapLen || newPos.y < 0 || newPos.y >= mapHei)
                continue;

            if (closedList.find(newPos) != closedList.end())
                continue;

            // save original position
            Pos originalPos{sprite.pos()};

            // Check if move is valid
            sprite.move(newPos);
            if (!sprite.canMove(g_dirs[i]))
            {
                const_cast<ISprite &>(sprite).move(originalPos);
                continue;
            }
            sprite.move(originalPos);

            int newGCost = current->gCost + 1;
            int newHCost = manhattanDistance(newPos, goalPos);

            auto it = nodes.find(newPos);
            if (it == nodes.end() || newGCost < it->second->gCost)
            {
                nodes[newPos] = std::make_unique<Node>(newPos, newGCost, newHCost, current);
                openList.push(nodes[newPos].get());
            }
        }
    }
    // No path found
    return directions; // Empty if no path found
}

CActor *CGame::spawnBullet(int x, int y, JoyAim aim, uint8_t tile)
{
    if (x < 0 || y < 0 || x >= m_map.len() || y >= m_map.hei())
    {
        LOGW("Cannot spawn at invalid coordinates: %d,%d", x, y);
        return nullptr;
    }

    const TileDef &def = getTileDef(tile);
    const uint8_t pu = m_map.at(x, y);
    CActor actor(x, y, def.type, aim);
    actor.setPU(pu);
    if (pu == TILES_BLANK)
    {
        m_map.set(x, y, tile);
        int size = addMonster(actor);
        return &m_monsters[size - 1];
    }
    return nullptr;
}

void CGame::manageBosses(const int ticks)
{
    auto &rng = getRandom();
    rng.setTick(ticks);

    // static std::mt19937 rng(std::random_device{}());
    // static std::uniform_int_distribution<int> dist(0, 3);
    const AStar aStar;
    const CActor &player = m_player;
    for (auto &boss : m_bosses)
    {
        if (boss.state() == CBoss::BossState::Hidden)
            continue;

        // customize animation speed
        const int a_speed = boss.data()->a_speed;
        if (a_speed == 0 || (ticks % a_speed) == 0)
            boss.animate();

        // ignore dead bosses
        if (boss.state() == CBoss::BossState::Death)
            continue;

        const int bx = boss.x() / 2;
        const int by = boss.y() / 2;
        if (boss.testHitbox(CBoss::isPlayer, nullptr))
            addHealth(-boss.damage());
        if (boss.testHitbox(CBoss::isIceCube, CBoss::meltIceCube))
            boss.subtainDamage(ICE_CUBE_DAMAGE);

        if (boss.speed() != 0 && (ticks % boss.speed() != 0))
            continue;

        if (boss.state() == CBoss::BossState::Patrol)
        {
            boss.patrol();
            if (boss.distance(player) <= CHASE_DISTANCE)
            {
                boss.setState(CBoss::BossState::Chase);
            }

            /*JoyAim aim = g_dirs[dist(rng)];
            if (boss.canMove(aim))
            {
                boss.move(aim);
            }

            if (boss.distance(player) <= CHASE_DISTANCE)
            {
                boss.setState(CBoss::BossState::Chase);
            }*/
        }
        else if (boss.state() == CBoss::BossState::Chase)
        {
            if (boss.distance(player) > PURSUIT_DISTANCE)
            {
                boss.setState(CBoss::BossState::Patrol);
                continue;
            }

            // Fireball spawning
            if (rng.range(0, 15) == 0 && bx > 2 && by > 0)
            {
                CActor *bullet = nullptr;
                if (player.y() < by - boss.hitbox().height)
                {
                    bullet = spawnBullet(bx, by - boss.hitbox().height, JoyAim::AIM_UP, TILES_FIREBALL);
                }
                else if (player.y() > by + boss.hitbox().height)
                {
                    bullet = spawnBullet(bx, by + boss.hitbox().height, JoyAim::AIM_DOWN, TILES_FIREBALL);
                }
                else if (player.x() < bx)
                {
                    bullet = spawnBullet(bx - 1, by - 1, JoyAim::AIM_LEFT, TILES_FIREBALL);
                }
                else if (player.x() > bx + boss.hitbox().width)
                {
                    bullet = spawnBullet(bx + boss.hitbox().width, by - 1, JoyAim::AIM_RIGHT, TILES_FIREBALL);
                }
                if (bullet != nullptr)
                {
                    boss.setState(CBoss::BossState::Attack);
                }
            }

            Pos playerPos{static_cast<int16_t>(m_player.x() * CBoss::BOSS_GRANULAR_FACTOR),
                          static_cast<int16_t>(m_player.y() * CBoss::BOSS_GRANULAR_FACTOR)};
            // CBoss tmp{boss};

            if (boss.followPath(playerPos, aStar))
                continue;

            /*auto path = aStar.findPath(boss, playerPos);
            if (path.size() > 1)
            {
                boss.move((path)[1]);
                continue;
            }*/

            // Fallback movement
            if (bx < player.x() && boss.canMove(JoyAim::AIM_RIGHT))
            {
                boss.move(JoyAim::AIM_RIGHT);
            }
            else if (bx > player.x() && boss.canMove(JoyAim::AIM_LEFT))
            {
                boss.move(JoyAim::AIM_LEFT);
            }
            else if (by < player.y() && boss.canMove(JoyAim::AIM_DOWN))
            {
                boss.move(JoyAim::AIM_DOWN);
            }
            else if (by > player.y() && boss.canMove(JoyAim::AIM_UP))
            {
                boss.move(JoyAim::AIM_UP);
            }
        }
    }
}

/**
 * @brief Handle all the monsters on the current map
 *
 * @param ticks clock ticks since start
 */

void CGame::manageMonsters(const int ticks)
{
    std::vector<CActor> newMonsters;
    std::set<int, std::greater<int>> deletedMonsters;

    auto isDeleted = [&deletedMonsters](int i)
    {
        return deletedMonsters.find(i) != deletedMonsters.end();
    };

    constexpr int speedCount = 9;
    bool speeds[speedCount];
    for (uint32_t i = 0; i < sizeof(speeds); ++i)
    {
        speeds[i] = i ? (ticks % i) == 0 : true;
    }

    for (size_t i = 0; i < m_monsters.size(); ++i)
    {
        if (isDeleted(i))
            continue;
        CActor &actor = m_monsters[i];
        const Pos pos = actor.pos();
        const uint8_t cs = m_map.at(pos.x, pos.y);
        const uint8_t attr = m_map.getAttr(pos.x, pos.y);
        if (RANGE(attr, ATTR_WAIT_MIN, ATTR_WAIT_MAX))
        {
            const uint8_t distance = (attr & 0xf) + 1;
            if (actor.distance(m_player) <= distance)
                m_map.setAttr(pos.x, pos.y, 0);
            else
                continue;
        }

        const TileDef &def = getTileDef(cs);
        if (!speeds[def.speed])
        {
            continue;
        }
        if (actor.type() == TYPE_MONSTER)
        {
            handleMonster(actor, def);
        }
        else if (actor.type() == TYPE_DRONE)
        {
            handleDrone(actor, def);
        }
        else if (actor.type() == TYPE_VAMPLANT)
        {
            handleVamPlant(actor, def, newMonsters);
        }
        else if (RANGE(actor.type(), ATTR_CRUSHER_MIN, ATTR_CRUSHER_MAX))
        {
            handleCrusher(actor, speeds);
        }
        else if (actor.type() == TYPE_ICECUBE)
        {
            handleIceCube(actor);
        }
        else if (actor.type() == TYPE_FIREBALL)
        {
            handleFirball(actor, def, i, deletedMonsters);
        }
        else if (actor.type() == TYPE_BOULDER)
        {
            // Do nothing for now
        }
        else
        {
            LOGW("unhandled monster type: %.x", actor.type());
        }
    }

    // moved here to avoid reallocation while using a reference
    for (auto const &monster : newMonsters)
    {
        addMonster(monster);
    }

    // remove deleted monsters
    for (auto const &i : deletedMonsters)
    {
        m_monsters.erase(m_monsters.begin() + i);
    }
}

void CGame::handleMonster(CActor &actor, const TileDef &def)
{
    if (actor.isPlayerThere(actor.getAim()))
    {
        // apply health damages
        addHealth(def.health);
        if (def.ai & AI_STICKY)
        {
            return;
        }
    }
    bool reverse = def.ai & AI_REVERSE;
    JoyAim aim = actor.findNextDir(reverse);
    if (aim != AIM_NONE)
    {
        actor.move(aim);
        if (!(def.ai & AI_ROUND))
        {
            return;
        }
    }
    for (uint8_t i = 0; i < sizeof(g_dirs); ++i)
    {
        if (actor.getAim() != g_dirs[i] &&
            actor.isPlayerThere(g_dirs[i]))
        {
            // apply health damages
            addHealth(def.health);
            if (def.ai & AI_FOCUS)
            {
                actor.setAim(g_dirs[i]);
            }
            break;
        }
    }
}

void CGame::handleDrone(CActor &actor, const TileDef &def)
{
    JoyAim aim = actor.getAim();
    if (aim < AIM_LEFT)
    {
        aim = AIM_LEFT;
    }
    if (actor.isPlayerThere(actor.getAim()))
    {
        // apply health damages
        addHealth(def.health);
        if (def.ai & AI_STICKY)
        {
            return;
        }
    }
    if (actor.canMove(aim))
    {
        actor.move(aim);
    }
    else if (aim == AIM_LEFT)
        aim = AIM_RIGHT;
    else
        aim = AIM_LEFT;
    actor.setAim(aim);
}

void CGame::handleVamPlant(CActor &actor, const TileDef &def, std::vector<CActor> &newMonsters)
{
    for (uint8_t i = 0; i < sizeof(g_dirs); ++i)
    {
        const Pos p = CGame::translate(Pos{actor.x(), actor.y()}, g_dirs[i]);
        const uint8_t ct = m_map.at(p.x, p.y);
        const TileDef &defT = getTileDef(ct);
        if (defT.type == TYPE_PLAYER)
        {
            // apply damage from config
            addHealth(def.health);
        }
        else if (defT.type == TYPE_SWAMP)
        {
            m_map.set(p.x, p.y, TILES_VAMPLANT);
            newMonsters.emplace_back(CActor(p.x, p.y, TYPE_VAMPLANT));
            break;
        }
        else if (defT.type == TYPE_MONSTER || defT.type == TYPE_DRONE)
        {
            const int j = findMonsterAt(p.x, p.y);
            if (j == INVALID)
                continue;
            CActor &m = m_monsters[j];
            m.setType(TYPE_VAMPLANT);
            m_map.set(p.x, p.y, TILES_VAMPLANT);
            break;
        }
    }
}

void CGame::handleCrusher(CActor &actor, const bool speeds[])
{
    const uint8_t speed = (actor.type() & CRUSHER_SPEED_MASK) + SPEED_VERYFAST;
    if (!speeds[speed])
    {
        return;
    }
    JoyAim aim = actor.getAim();
    const bool isPlayerThere = actor.isPlayerThere(aim);
    if (isPlayerThere && !isGodMode())
    {
        // apply health damages
        addHealth(AUTOKILL);
    }
    if (actor.canMove(aim) && !(isPlayerThere && isGodMode()))
        actor.move(aim);
    else if (aim == AIM_LEFT)
        aim = AIM_RIGHT;
    else if (aim == AIM_RIGHT)
        aim = AIM_LEFT;
    else if (aim == AIM_DOWN)
        aim = AIM_UP;
    else if (aim == AIM_UP)
        aim = AIM_DOWN;
    actor.setAim(aim);
}

void CGame::handleIceCube(CActor &actor)
{
    JoyAim aim = actor.getAim();
    if (aim != JoyAim::AIM_NONE)
    {
        if (actor.canMove(aim))
            actor.move(aim);
        else
            actor.setAim(JoyAim::AIM_NONE);
    }
}

void CGame::handleFirball(CActor &actor, const TileDef &def, const int i, std::set<int, std::greater<int>> &deletedMonsters)
{
    JoyAim aim = actor.getAim();
    if (actor.canMove(aim))
    {
        actor.move(aim);
    }
    else
    {
        m_map.set(actor.x(), actor.y(), actor.getPU());
        deletedMonsters.insert(i);
        m_sfx.emplace_back(sfx_t{.x = actor.x(), .y = actor.y(), .sfxID = SFX_EXPLOSION1, .timeout = SFX_EXPLOSION1_TIMEOUT});

        if (CGame::translate(Pos{actor.x(), actor.y()}, aim) == actor.pos())
            // coordonate outside map bounds
            return;
        const uint8_t tileID = actor.tileAt(aim);
        const TileDef &defX = getTileDef(tileID);
        if (defX.type == TYPE_ICECUBE)
        {
            const Pos &pos = translate(actor.pos(), aim);
            int i = findMonsterAt(pos.x, pos.y);
            if (i != INVALID)
            {
                deletedMonsters.insert(i);
                m_sfx.emplace_back(sfx_t{.x = pos.x, .y = pos.y, .sfxID = SFX_EXPLOSION6, .timeout = SFX_EXPLOSION6_TIMEOUT});
                m_map.set(pos.x, pos.y, TILES_BLANK);
            }
        }
        else if (actor.isPlayerThere(aim) && !isGodMode())
        {
            addHealth(def.health);
        }
    }
}