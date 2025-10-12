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
#pragma once

#include <cinttypes>
#include <functional>
#include "rect.h"
#include "joyaim.h"
#include "actor.h"
#include "isprite.h"
#include "map.h"

typedef std::function<bool(const uint8_t)> hitboxCallback_t;
typedef std::function<bool(const Pos &)> hitboxPosCallback_t;

class CBoss : public ISprite
{
public:
    enum BossState : uint8_t
    {
        Patrol,
        Chase,
        Attack,
        Flee,
        Hurt,
        Death,
        Hidden,
    };
    CBoss(const int16_t x, const int16_t y, const Rect hitbox, const uint8_t type);
    virtual ~CBoss() = default;
    inline int16_t x() const override { return m_x; }
    inline int16_t y() const override { return m_y; }
    inline const Rect &hitbox() const { return m_hitbox; }
    uint8_t type() const override { return m_type; }
    bool testHitbox(hitboxPosCallback_t testCallback, hitboxPosCallback_t actionCallback) const;
    static bool isPlayer(const Pos &pos);
    static bool isIceCube(const Pos &pos);
    static bool isSolid(const uint8_t c);
    static bool meltIceCube(const Pos &pos);
    bool canMove(const JoyAim aim) const override;
    void move(const JoyAim aim) override;
    int distance(const CActor &actor) const override;
    int speed() const { return m_speed; }
    void setSpeed(int speed) { m_speed = speed; }
    void setHP(const int hp) { m_hp = hp; }
    int hp() const { return m_hp; }
    int maxHp() const;
    void subtainDamage(const int lostHP);
    bool isHidden() const { return m_state == Hidden; }

    void move(const int16_t x, const int16_t y) override
    {
        m_x = x;
        m_y = y;
    }
    void move(const Pos pos) override;
    inline BossState state() const
    {
        return m_state;
    }
    void setState(const BossState state);
    inline int16_t granularFactor() const override { return BOSS_GRANULAR_FACTOR; };
    inline const Pos pos() const override { return Pos{m_x, m_y}; };
    void animate();
    int currentFrame() const;

    enum : int16_t
    {
        BOSS_GRANULAR_FACTOR = 2,
        BASE_BOSS_FLYING = 0,
        LEN_BOSS_FLYING = 4,
        BASE_BOSS_ATTACK = 4,
        LEN_BOSS_ATTACK = 8,
        BASE_BOSS_HURT = 12,
        LEN_BOSS_HURT = 4,
        BASE_BOSS_DEATH = 16,
        LEN_BOSS_DEATH = 6,
        MAX_HP = 64,
    };

private:
    int m_framePtr;
    int16_t m_x;
    int16_t m_y;
    int m_hp;
    uint8_t m_type;
    BossState m_state;
    Rect m_hitbox;
    int m_speed;
};
