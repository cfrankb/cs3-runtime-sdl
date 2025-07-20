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

CMenuItem::CMenuItem(const std::string &tmpl, const std::vector<std::string> &options, int *value)
{
    m_tmpl = tmpl;
    m_options = options;
    m_value = value;
    m_type = ITEM_OPTIONS;
}

CMenuItem::~CMenuItem()
{
}

void CMenuItem::left()
{
    if (!m_value)
    {
        return;
    }
    int &value = *m_value;
    if (m_type == ITEM_RANGE)
    {
        if (value > m_rangeMin)
            --value;
    }
    else if (m_type == ITEM_OPTIONS)
    {
        if (value > 0)
            --value;
    }
}

void CMenuItem::right()
{
    if (!m_value)
    {
        return;
    }
    int &value = *m_value;
    if (m_type == ITEM_RANGE)
    {
        if (value < m_rangeMax)
            ++value;
    }
    else if (m_type == ITEM_OPTIONS)
    {
        if (value < m_options.size() - 1)
            ++value;
    }
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
