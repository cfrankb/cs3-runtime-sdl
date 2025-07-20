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
    const int scaleX() const;
    const int scaleY() const;
    const int paddingY() const;

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