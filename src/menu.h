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
#include <cstdint>
#include <vector>
#include "menuitem.h"

class CMenu
{
public:
    CMenu(const int menuid = 0, const int scaleX = 2, const int scaleY = 2, const int padding = FONT_SIZE, CMenu *parent = nullptr);
    ~CMenu();

    CMenuItem &addItem(const CMenuItem &menuItem);
    size_t size() const;
    CMenuItem &at(int i);
    void up();
    void down();
    int index() const;
    void clear();
    CMenuItem &current();
    CMenuItem &last();
    int id() const;
    int height() const;
    void setCurrent(const int i);
    int scaleX() const;
    int scaleY() const;
    int paddingY() const;

private:
    enum
    {
        FONT_SIZE = 8,
    };
    int m_scaleX = 2;
    int m_scaleY = 2;
    int m_padding = FONT_SIZE;
    int m_menuid = 0;
    CMenu *m_parent = nullptr;
    int m_currentItem = 0;
    std::vector<CMenuItem> m_items;
};