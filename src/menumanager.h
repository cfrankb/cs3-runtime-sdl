#pragma once

#include <memory>
#include <unordered_map>

class CMenu;

enum MenuID
{
    MENUID_MAINMENU = 0x10,
    MENUID_GAMEMENU,
    MENUID_OPTIONMENU,
    MENUID_USERS,
    MENUID_SKILLS
};

class MenuManager
{

public:
    MenuManager();
    ~MenuManager();

    CMenu *get(MenuID menuID);
    CMenu *last() { return m_lastMenu; }
    void setLastMenu(CMenu *menu, const int baseX, const int baseY)
    {
        m_lastMenu = menu;
        m_lastMenuBaseX = baseX;
        m_lastMenuBaseY = baseY;
    }

    int menuItemAt(int x, int y);
    void followPointer(int x, int y);

    void setActive(const MenuID menuID, const bool active);
    bool isMenuActive(const MenuID menuID);
    void toggleMenuActive(const MenuID menuID);
    bool isMenuActive();

private:
    enum
    {
        INVALID = -1,
        FONT_SIZE = 8,
    };

    std::unordered_map<int, std::unique_ptr<CMenu>> m_menus;
    std::unordered_map<int, bool> m_menusActive;

    CMenu *m_lastMenu;
    int m_lastMenuBaseY = -1; // baseY;
    int m_lastMenuBaseX = -1; // baseX;
};