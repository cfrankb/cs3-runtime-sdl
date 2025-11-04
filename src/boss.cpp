/*
    cs3-runtime-sdl
    Copyright (C) 2025  Francois Blanchette

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

#include <cmath>
#include "boss.h"
#include "game.h"
#include "tilesdata.h"
#include "sprtypes.h"
#include "logger.h"
#include "randomz.h"
#include "ai_path.h"
#include "shared/IFile.h"
#include "bossdata.h"
#include "filemacros.h"
#include "map.h"

namespace BossPrivate
{
    constexpr int16_t MAX_POS = 512;
    constexpr int MAX_HP = 4096;
    constexpr int MAX_SPEED = 10;
    constexpr int MAX_FRAME = 16;
};

using namespace BossPrivate;

CBoss::CBoss(const int16_t x, const int16_t y, const bossData_t *data) : m_bossData(data)
{
    m_x = x;
    m_y = y;
    m_speed = data->speed;
    m_state = Patrol;
    m_framePtr = 0;
    m_hp = maxHp(); // data->hp;
    setSolidOperator();
}

bool CBoss::isPlayer(const Pos &pos)
{
    const CMap &map = CGame::getMap();
    const auto c = map.at(pos.x, pos.y);
    const TileDef &def = getTileDef(c);
    return def.type == TYPE_PLAYER;
}

bool CBoss::isIceCube(const Pos &pos)
{
    const CMap &map = CGame::getMap();
    const auto c = map.at(pos.x, pos.y);
    const TileDef &def = getTileDef(c);
    return def.type == TYPE_ICECUBE;
}

bool CBoss::meltIceCube(const Pos &pos)
{
    CGame *game = CGame::getGame();
    CMap &map = CGame::getMap();
    int i = game->findMonsterAt(pos.x, pos.y);
    if (i != CGame::INVALID)
    {
        game->deleteMonster(i);
        game->getSfx().emplace_back(sfx_t{pos.x, pos.y, SFX_EXPLOSION6, SFX_EXPLOSION6_TIMEOUT});
        map.set(pos.x, pos.y, TILES_BLANK);
    }
    return true;
}

bool CBoss::isSolid(const Pos &pos)
{
    CMap &map = CGame::getMap();
    const auto c = map.at(pos.x, pos.y);
    const TileDef &def = getTileDef(c);
    return def.type != TYPE_BACKGROUND && def.type != TYPE_PLAYER;
}

bool CBoss::isGhostBlocked(const Pos &pos)
{
    CMap &map = CGame::getMap();
    const auto c = map.at(pos.x, pos.y);
    const TileDef &def = getTileDef(c);
    return def.type == TYPE_SWAMP || def.type == TYPE_ICECUBE || c == TILES_WALLS93_3;
}

bool CBoss::testHitbox(hitboxPosCallback_t testCallback, hitboxPosCallback_t actionCallback) const
{
    // add primary hitbox
    const auto &hbMain = m_bossData->hitbox; // primary hitbox
    const int x = m_x / BOSS_GRANULAR_FACTOR;
    const int y = m_y / BOSS_GRANULAR_FACTOR;
    const int w = hbMain.width / BOSS_GRANULAR_FACTOR;
    const int h = hbMain.height / BOSS_GRANULAR_FACTOR;
    std::vector<hitbox_t> hitboxes;
    hitboxes.push_back({x, y, w, h, BossData::HitBoxType::MAIN});

    const sprite_hitbox_t *hbData = getHitboxes(m_bossData->sheet, currentFrame());
    for (int i = 0; hbData != nullptr && i < hbData->count; ++i)
    {
        // add secondary hitbox - relative to primary
        const hitbox_t &c = hbData->hitboxes[i];
        const hitbox_t hb{
            (m_x + c.x - hbMain.x) / BOSS_GRANULAR_FACTOR,
            (m_y + c.y - hbMain.y) / BOSS_GRANULAR_FACTOR,
            c.width / BOSS_GRANULAR_FACTOR,
            c.height / BOSS_GRANULAR_FACTOR,
            c.type,
        };
        hitboxes.push_back(hb);
    }

    // test all hitboxes
    for (const auto &hb : hitboxes)
    {
        for (int ay = 0; ay < hb.height; ++ay)
        {
            for (int ax = 0; ax < hb.width; ++ax)
            {
                const Pos pos{static_cast<int16_t>(hb.x + ax), static_cast<int16_t>(hb.y + ay)};
                if (testCallback(pos))
                    return actionCallback ? actionCallback(pos) : true;
            }
        }
    }
    return false;
}

bool CBoss::canMove(const JoyAim aim) const
{
    const CMap &map = CGame::getMap();
    const int mapLen = map.len();
    const int mapHei = map.hei();
    const int x = m_x / BOSS_GRANULAR_FACTOR;
    const int y = m_y / BOSS_GRANULAR_FACTOR;
    const int w = m_bossData->hitbox.width / BOSS_GRANULAR_FACTOR;
    const int h = m_bossData->hitbox.height / BOSS_GRANULAR_FACTOR;
    const int maxX = mapLen * BOSS_GRANULAR_FACTOR;
    const int maxY = mapHei * BOSS_GRANULAR_FACTOR;

    // Check if move stays within same 8x8 grid cell and map bounds
    int next_x = m_x;
    int next_y = m_y;
    switch (aim)
    {
    case JoyAim::AIM_UP:
        --next_y;
        break;
    case JoyAim::AIM_DOWN:
        ++next_y;
        break;
    case JoyAim::AIM_LEFT:
        --next_x;
        break;
    case JoyAim::AIM_RIGHT:
        ++next_x;
        break;
    default:
        LOGW("invalid aim: %.2x on %d", aim, __LINE__);
        return false;
    }
    if (next_x / BOSS_GRANULAR_FACTOR == m_x / BOSS_GRANULAR_FACTOR &&
        next_y / BOSS_GRANULAR_FACTOR == m_y / BOSS_GRANULAR_FACTOR &&
        next_x >= 0 && next_x + m_bossData->hitbox.width <= maxX &&
        next_y >= 0 && next_y + m_bossData->hitbox.height <= maxY)
    {
        return true; // Sub-tile move within same 8x8 cell and map bounds
    }

    // Check collisions for new 8x8 grid cell
    switch (aim)
    {
    case JoyAim::AIM_UP:
        if (next_y < 0)
            return false;
        for (int ax = x; ax < x + w; ++ax)
        {
            if (ax < 0 || ax >= mapLen)
                continue;
            const Pos pos{static_cast<int16_t>(ax), static_cast<int16_t>(y - 1)};
            if (m_isSolidOperator(pos))
                return false;
        }
        break;

    case JoyAim::AIM_DOWN:
        if (next_y + m_bossData->hitbox.height > maxY)
            return false;
        for (int ax = x; ax < x + w; ++ax)
        {
            if (ax < 0 || ax >= mapLen)
                continue;
            const Pos pos{static_cast<int16_t>(ax), static_cast<int16_t>(y + h)};
            if (m_isSolidOperator(pos))
                return false;
        }
        break;

    case JoyAim::AIM_LEFT:
        if (next_x < 0)
            return false;
        for (int ay = y; ay < y + h; ++ay)
        {
            if (ay < 0 || ay >= mapHei)
                continue;
            const Pos pos{static_cast<int16_t>(x - 1), static_cast<int16_t>(ay)};
            if (m_isSolidOperator(pos))
                return false;
        }
        break;

    case JoyAim::AIM_RIGHT:
        if (next_x + m_bossData->hitbox.width > maxX)
            return false;
        for (int ay = y; ay < y + h; ++ay)
        {
            if (ay < 0 || ay >= mapHei)
                continue;
            const Pos pos{static_cast<int16_t>(x + w), static_cast<int16_t>(ay)};
            if (m_isSolidOperator(pos))
                return false;
        }
        break;
    default:
        LOGE("invalid aim: %.2x on line %d", aim, __LINE__);
        return false;
    }
    return true;
}

void CBoss::move(const JoyAim aim)
{
    switch (aim)
    {
    case JoyAim::AIM_UP:
        --m_y;
        break;
    case JoyAim::AIM_DOWN:
        ++m_y;
        break;
    case JoyAim::AIM_LEFT:
        --m_x;
        break;
    case JoyAim::AIM_RIGHT:
        ++m_x;
        break;
    default:
        LOGE("invalid aim: %.2x", aim);
        return;
    }
    m_aim = aim;
}

/**
 * @brief Calculate the distance between the boss and another actor
 *
 * @param actor
 * @return int
 */
int CBoss::distance(const CActor &actor) const
{
    int dx = std::abs(actor.m_x - m_x / BOSS_GRANULAR_FACTOR);
    int dy = std::abs(actor.m_y - m_y / BOSS_GRANULAR_FACTOR);
    return std::sqrt(dx * dx + dy * dy);
}

void CBoss::move(const Pos pos)
{
    move(pos.x, pos.y);
}

const boss_seq_t *CBoss::getCurrentSeq() const
{
    const boss_seq_t *seq = nullptr;

    if (m_state == BossState::Patrol)
    {
        seq = &m_bossData->idle;
    }
    else if (m_state == BossState::Chase)
    {
        seq = &m_bossData->moving;
    }
    else if (m_state == BossState::Attack)
    {
        seq = &m_bossData->attack;
    }
    else if (m_state == BossState::Hurt)
    {
        seq = &m_bossData->hurt;
    }
    else if (m_state == BossState::Death)
    {
        seq = &m_bossData->death;
    }
    else
    {
        return nullptr;
    }
    return seq;
}

void CBoss::animate()
{
    const boss_seq_t *seq = getCurrentSeq();
    if (seq == nullptr)
    {
        m_framePtr = 0;
        return;
    }

    int maxFrames = seq->lenght;
    // sanity check
    if (maxFrames == 0)
    {
        m_framePtr = 0;
        return;
    }

    ++m_framePtr;
    if (m_framePtr >= maxFrames)
    {
        m_framePtr = 0;
        if (m_state == BossState::Hurt)
            m_state = BossState::Patrol;
        else if (m_state == BossState::Attack)
            m_state = BossState::Chase;
        else if (m_state == BossState::Death)
            m_state = BossState::Hidden;
    }
}

int CBoss::currentFrame() const
{
    const boss_seq_t *seq = getCurrentSeq();
    if (seq == nullptr)
        return 0;
    const int normalizedAim = m_aim % m_bossData->aims;
    const int baseFrame = seq->base + seq->lenght * normalizedAim;
    return baseFrame + m_framePtr;
}

void CBoss::setState(const BossState state)
{
    m_state = state;
    m_framePtr = 0;
};

int CBoss::maxHp() const
{
    auto skill = (int)CGame::getGame()->skill();
    return (int)m_bossData->hp * ((skill * 0.5) + 1);
}

bool CBoss::subtainDamage(const int lostHP)
{
    bool justDied = false;
    m_hp = std::max(m_hp - lostHP, 0);
    if (m_hp == 0)
    {
        if (m_state != Death)
        {
            justDied = true;
            setState(Death);
        }
    }
    else
    {
        setState(Hurt);
    }
    return justDied;
}

int CBoss::damage() const
{
    return m_bossData->damage;
}

const Pos CBoss::toPos(int x, int y)
{
    return Pos{static_cast<int16_t>(x), static_cast<int16_t>(y)};
}

bool CBoss::followPath(const Pos &playerPos, const IPath &astar)
{
    return m_path.followPath(*this, playerPos, astar);
}

void CBoss::patrol()
{
    constexpr JoyAim dirs[] = {AIM_UP, AIM_DOWN, AIM_LEFT, AIM_RIGHT};
    Random &rng = CGame::getRandom();
    int dir = rng.range(0, 4); // Choose random direction
    JoyAim aim = dirs[dir];
    if (canMove(aim))
    {
        move(aim);
    }
}

bool CBoss::read(IFile &sfile)
{
    auto readfile = [&sfile](auto ptr, auto size) -> bool
    {
        return sfile.read(ptr, size) == IFILE_OK;
    };

    auto checkBound = [](auto a, auto b)
    {
        using T = decltype(a);

        if constexpr (std::is_same_v<T, size_t>)
        {
            if (a > b)
            {
                LOGE("%s: %lu outside expected -- expected < %lu", _S(a), a, b);
                return false;
            }
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            if (a > b)
            {
                LOGE("%s: %u outside expected -- expected < %u", _S(a), a, b);
                return false;
            }
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if (a > b || a < 0)
            {
                LOGE("%s: %d outside expected -- expected > 0 && < %d", _S(a), a, b);
                return false;
            }
        }
        else
        {
            static_assert(std::is_integral_v<T>, "checkBound only supports integral types");
        }

        return true;
    };

    constexpr size_t DATA_SIZE = 2;
    uint8_t type = 0;
    _R(&type, sizeof(uint8_t));
    auto data = getBossData(type);
    if (!data)
    {
        LOGE("invalid boss type 0x%.2x", type);
        return false;
    }
    m_bossData = data;

    m_framePtr = 0;
    _R(&m_framePtr, DATA_SIZE);
    checkBound(m_framePtr, MAX_FRAME);
    _R(&m_x, sizeof(m_x)); // uint16_t
    checkBound(m_x, MAX_POS);
    _R(&m_y, sizeof(m_y)); // uint16_t
    checkBound(m_y, MAX_POS);

    m_hp = 0;
    _R(&m_hp, DATA_SIZE);
    checkBound(m_hp, MAX_HP);

    _R(&m_state, sizeof(m_state)); // uint8_t
    checkBound((uint8_t)m_state, (uint8_t)BossState::MAX_STATES);

    m_speed = 0;
    _R(&m_speed, DATA_SIZE);
    checkBound(m_speed, MAX_SPEED);

    _R(&m_aim, sizeof(m_aim)); // uint8_t
    checkBound(static_cast<uint8_t>(m_aim), JoyAim::TOTAL_AIMS);

    // read path
    m_path.read(sfile);

    setSolidOperator();
    return true;
}

bool CBoss::write(IFile &tfile)
{
    auto writefile = [&tfile](auto ptr, auto size)
    {
        return tfile.write(ptr, size) == IFILE_OK;
    };

    constexpr size_t DATA_SIZE = 2;
    _W(&m_bossData->type, sizeof(uint8_t));
    _W(&m_framePtr, DATA_SIZE);
    _W(&m_x, sizeof(m_x)); // uint16_t
    _W(&m_y, sizeof(m_y)); // uint16_t
    _W(&m_hp, DATA_SIZE);
    _W(&m_state, sizeof(m_state)); // uint8_t
    _W(&m_speed, DATA_SIZE);
    _W(&m_aim, sizeof(m_aim)); // uint8_t

    // write path
    m_path.write(tfile);

    return true;
}

void CBoss::setSolidOperator()
{
    LOGI("set solidOperator for boss %.2x", m_bossData->type);
    if (m_bossData->type == BOSS_MR_DEMON)
        m_isSolidOperator = isSolid;
    else if (m_bossData->type == BOSS_GHOST)
        m_isSolidOperator = isGhostBlocked;
    else if (m_bossData->type == BOSS_HARPY)
        m_isSolidOperator = isSolid;
    else
    {
        LOGW("No custom solidOperator for this boss");
        m_isSolidOperator = isSolid;
    }
}
