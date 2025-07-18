#pragma once
#include <cstdint>
#include <vector>
#include "menuitem.h"

class CMenu
{
public:
    CMenu(const int menuid = 0, CMenu *parent = nullptr);
    ~CMenu();

    CMenuItem &addItem(const CMenuItem &menuItem);
    size_t size();
    CMenuItem &at(int i);
    void up();
    void down();
    int index() const;
    void clear();
    CMenuItem &current();
    CMenuItem &last();
    int id() const;
    void setCurrent(const int i);

protected:
private:
    int m_menuid = 0;
    CMenu *m_parent = nullptr;
    int m_currentItem = 0;
    std::vector<CMenuItem> m_items;
};