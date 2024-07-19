/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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
#ifndef __ACTOR__H
#define __ACTOR__H
#include <stdint.h>
#include "map.h"

enum JoyAim
{
    AIM_UP = 0,
    AIM_DOWN = 1,
    AIM_LEFT = 2,
    AIM_RIGHT = 3,
    TOTAL_AIMS = 4,
    AIM_NONE = -1
};

class CActor
{

public:
    CActor(uint8_t x = 0, uint8_t y = 0, uint8_t type = 0, uint8_t aim = 0);
    CActor(const Pos &pos, uint8_t type = 0, uint8_t aim = 0);
    ~CActor();

    bool canMove(int aim);
    void move(const int aim);
    uint8_t getX() const;
    uint8_t getY() const;
    uint8_t getPU() const;
    void setPU(const uint8_t c);
    void setXY(const Pos &pos);
    uint8_t getAim() const;
    void setAim(const uint8_t aim);
    int findNextDir();
    bool isPlayerThere(uint8_t aim);
    uint8_t tileAt(uint8_t aim);
    void setType(const uint8_t type);
    bool within(int x1, int y1, int x2, int y2) const;

protected:
    uint8_t m_x;
    uint8_t m_y;
    uint8_t m_type;
    uint8_t m_aim;
    uint8_t m_pu;

    friend class CGame;
};

#endif
