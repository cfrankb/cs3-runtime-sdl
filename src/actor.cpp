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
#include "actor.h"
#include "tilesdata.h"
#include "game.h"
#include "sprtypes.h"
#include <cstdio>

uint8_t AIMS[] = {
    AIM_DOWN, AIM_RIGHT, AIM_UP, AIM_LEFT,
    AIM_UP, AIM_LEFT, AIM_DOWN, AIM_RIGHT,
    AIM_RIGHT, AIM_UP, AIM_LEFT, AIM_DOWN,
    AIM_LEFT, AIM_DOWN, AIM_RIGHT, AIM_UP};

CActor::CActor(uint8_t x, uint8_t y, uint8_t type, uint8_t aim)
{
    m_x = x;
    m_y = y;
    m_type = type;
    m_aim = aim;
    m_pu = TILES_BLANK;
}

CActor::CActor(const Pos &pos, uint8_t type, uint8_t aim)
{
    m_x = pos.x;
    m_y = pos.y;
    m_type = type;
    m_aim = aim;
    m_pu = TILES_BLANK;
}

CActor::~CActor()
{
}

bool CActor::canMove(int aim)
{
    CMap &map = CGame::getMap();
    const Pos &pos = Pos{m_x, m_y};
    const Pos &newPos = CGame::translate(pos, aim);
    if (pos.x == newPos.x && pos.y == newPos.y)
    {
        return false;
    }

    uint8_t c = map.at(newPos.x, newPos.y);
    const TileDef &def = getTileDef(c);
    if (def.type == TYPE_BACKGROUND)
    {
        return true;
    }
    else if (m_type == TYPE_PLAYER)
    {
        if (def.type == TYPE_SWAMP ||
            def.type == TYPE_PICKUP ||
            def.type == TYPE_DIAMOND ||
            def.type == TYPE_STOP ||
            def.type == TYPE_KEY)
        {
            return true;
        }
        else if (def.type == TYPE_DOOR)
        {
            return CGame::hasKey(c + 1);
        }
    }
    return false;
}

void CActor::move(const int aim)
{
    CMap &map = CGame::getMap();
    uint8_t c = map.at(m_x, m_y);
    map.set(m_x, m_y, m_pu);

    Pos pos{m_x, m_y};
    pos = CGame::translate(pos, aim);
    m_x = pos.x;
    m_y = pos.y;

    m_pu = map.at(m_x, m_y);
    map.set(m_x, m_y, c);
    m_aim = aim;
}

uint8_t CActor::getX() const
{
    return m_x;
}

uint8_t CActor::getY() const
{
    return m_y;
}

uint8_t CActor::getPU() const
{
    return m_pu;
}

void CActor::setPU(const uint8_t c)
{
    m_pu = c;
}

void CActor::setXY(const Pos &pos)
{
    m_x = pos.x;
    m_y = pos.y;
}

int CActor::findNextDir()
{
    int i = TOTAL_AIMS - 1;
    while (i >= 0)
    {
        int aim = AIMS[m_aim * TOTAL_AIMS + i];
        if (canMove(aim))
        {
            return aim;
        }
        --i;
    }
    return AIM_NONE;
}

uint8_t CActor::getAim() const
{
    return m_aim;
}

void CActor::setAim(const uint8_t aim)
{
    m_aim = aim;
}

bool CActor::isPlayerThere(uint8_t aim)
{
    const uint8_t c = tileAt(aim);
    const TileDef &def = getTileDef(c);
    return def.type == TYPE_PLAYER;
}

uint8_t CActor::tileAt(uint8_t aim)
{
    CMap &map = CGame::getMap();
    const Pos &p = CGame::translate(Pos{m_x, m_y}, aim);
    return map.at(p.x, p.y);
}

void CActor::setType(const uint8_t type)
{
    m_type = type;
}

bool CActor::within(int x1, int y1, int x2, int y2) const
{
    return (m_x >= x1) && (m_x < x2) && (m_y >= y1) && (m_y < y2);
}

bool CActor::read(FILE *sfile)
{
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    readfile(&m_x, sizeof(m_x));
    readfile(&m_y, sizeof(m_y));
    readfile(&m_type, sizeof(m_type));
    readfile(&m_aim, sizeof(m_aim));
    readfile(&m_pu, sizeof(m_pu));
    return true;
}

bool CActor::write(FILE *tfile)
{
    auto writefile = [tfile](auto ptr, auto size)
    {
        return fwrite(ptr, size, 1, tfile) == 1;
    };
    writefile(&m_x, sizeof(m_x));
    writefile(&m_y, sizeof(m_y));
    writefile(&m_type, sizeof(m_type));
    writefile(&m_aim, sizeof(m_aim));
    writefile(&m_pu, sizeof(m_pu));
    return true;
}
