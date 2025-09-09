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
#include "menuitem.h"

CMenuItem::CMenuItem(const std::string &tmpl, const int role, CMenu *menu)
{
    m_role = role;
    m_tmpl = tmpl;
    m_menu = menu;
    m_type = ITEM_STATIC;
}

CMenuItem::CMenuItem(const std::string &tmpl, const int rangeMin, const int rangeMax, int *value, int start, int factor)
{
    m_tmpl = tmpl;
    m_rangeMin = rangeMin;
    m_rangeMax = rangeMax;
    m_value = value;
    m_factor = factor;
    m_start = start;
    m_type = ITEM_RANGE;
}

CMenuItem::CMenuItem(const std::string &tmpl, const std::vector<std::string> &options, int *value, const int type)
{
    m_tmpl = tmpl;
    m_options = options;
    m_value = value;
    m_type = type;
}

CMenuItem::CMenuItem(const std::vector<std::string> &options, int *value, const int type)
{
    m_tmpl = "";
    m_options = options;
    m_type = type;
    m_value = value;
}

CMenuItem::~CMenuItem()
{
}

bool CMenuItem::left()
{
    if (!m_value)
    {
        return false;
    }
    int &value = *m_value;
    if (m_type == ITEM_RANGE)
    {
        if (value > m_rangeMin)
        {
            --value;
            return true;
        }
    }
    else if (m_type == ITEM_OPTIONS || m_type == ITEM_BAR)
    {
        if (value > 0)
        {
            --value;
            return true;
        }
    }
    return false;
}

bool CMenuItem::right()
{
    if (!m_value)
    {
        return false;
    }
    int &value = *m_value;
    if (m_type == ITEM_RANGE)
    {
        if (value < m_rangeMax)
        {
            ++value;
            return true;
        }
    }
    else if (m_type == ITEM_OPTIONS || m_type == ITEM_BAR)
    {
        if (value < static_cast<int>(m_options.size() - 1))
        {
            ++value;
            return true;
        }
    }
    return false;
}

std::string CMenuItem::str() const
{
    char tmp[256];
    if (m_type == ITEM_RANGE)
    {
        sprintf(tmp, m_tmpl.c_str(), ((*m_value) + m_start) * m_factor);
        return tmp;
    }
    else if (m_type == ITEM_OPTIONS)
    {
        sprintf(tmp, m_tmpl.c_str(), m_options[*m_value].c_str());
        return tmp;
    }
    else if (m_type == ITEM_STATIC)
    {
        return m_tmpl;
    }
    else
    {
        return "invalid type";
    }
}

int CMenuItem::value() const
{
    return m_value ? *m_value : 0;
}

CMenuItem &CMenuItem::disable(const bool value)
{
    m_disabled = value;
    return *this;
}

bool CMenuItem::isDisabled() const
{
    return m_disabled;
}

int CMenuItem::role() const
{
    return m_role;
}

CMenuItem &CMenuItem::setRole(const int role)
{
    m_role = role;
    return *this;
}

CMenu *CMenuItem::menu() const
{
    return m_menu;
}

CMenuItem &CMenuItem::setUserData(const int userData)
{
    m_userdata = userData;
    return *this;
}

int CMenuItem::userData() const
{
    return m_userdata;
}

int CMenuItem::type() const
{
    return m_type;
}

CMenuItem &CMenuItem::setType(const int type)
{
    m_type = type;
    return *this;
}

std::string CMenuItem::option(int i) const
{
    return m_options[i];
}

size_t CMenuItem::size() const
{
    return m_options.size();
}