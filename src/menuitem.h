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
#include <string>
#include <vector>

class CMenu;

class CMenuItem
{
public:
    enum
    {
        ITEM_STATIC,
        ITEM_RANGE,
        ITEM_OPTIONS,
        ROLE_NONE = 0,
    };

    CMenuItem(const std::string &tmpl, const int role = ROLE_NONE, CMenu *menu = nullptr);
    CMenuItem(const std::string &tmpl, const int rangeMin, const int rangeMax, int *value, int start = 1, int factor = 1);
    CMenuItem(const std::string &tmpl, const std::vector<std::string> &options, int *value);
    ~CMenuItem();

    CMenuItem &disable(const bool value = true);
    bool isDisabled() const;
    bool left();
    bool right();
    int value() const;
    int role() const;
    CMenu *menu() const;
    CMenuItem &setRole(const int action = ROLE_NONE);
    std::string str() const;

private:
    std::vector<std::string> m_options;
    std::string m_tmpl;
    int m_type = ITEM_STATIC;
    int *m_value = nullptr;
    int m_rangeMin = 0;
    int m_rangeMax = 0;
    bool m_disabled = false;
    int m_role = ROLE_NONE;
    int m_start = 1;
    int m_factor = 1;
    CMenu *m_menu = nullptr;
};