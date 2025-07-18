#include "menu.h"

CMenu::CMenu(const int menuid, CMenu *parent)
{
    m_parent = parent;
    m_menuid = menuid;
}

CMenu::~CMenu()
{
}

CMenuItem &CMenu::addItem(const CMenuItem &menuItem)
{
    m_items.push_back(menuItem);
    return last();
}

size_t CMenu::size()
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
    if (m_currentItem < m_items.size() - 1)
        ++m_currentItem;
}

int CMenu::index()
{
    return m_currentItem;
}

void CMenu::clear()
{
    m_items.clear();
}

int CMenu::id()
{
    return m_menuid;
}