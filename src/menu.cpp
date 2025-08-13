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
#include "menu.h"

CMenu::CMenu(const int menuid, const int scaleX, const int scaleY, const int padding, CMenu *parent)
{
    m_parent = parent;
    m_menuid = menuid;
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    m_padding = padding;
}

CMenu::~CMenu()
{
}

CMenuItem &CMenu::addItem(const CMenuItem &menuItem)
{
    m_items.push_back(menuItem);
    return last();
}

size_t CMenu::size() const
{
    return m_items.size();
}

CMenuItem &CMenu::at(int i)
{
    return m_items[i];
}

CMenuItem &CMenu::current()
{
    return m_items[m_currentItem];
}

CMenuItem &CMenu::last()
{
    return m_items[m_items.size() - 1];
}

void CMenu::up()
{
    if (m_currentItem)
        --m_currentItem;
}

void CMenu::down()
{
    if (m_currentItem < static_cast<int>(m_items.size() - 1))
        ++m_currentItem;
}

int CMenu::index() const
{
    return m_currentItem;
}

void CMenu::clear()
{
    m_items.clear();
    setCurrent(0);
}

int CMenu::id() const
{
    return m_menuid;
}

void CMenu::setCurrent(const int i)
{
    m_currentItem = i;
}

int CMenu::height() const
{
    return size() ? (m_scaleY * FONT_SIZE + m_padding) * size() - m_padding : 0;
}

int CMenu::scaleX() const
{
    return m_scaleX;
}

int CMenu::scaleY() const
{
    return m_scaleY;
}

int CMenu::paddingY() const
{
    return m_padding;
}
