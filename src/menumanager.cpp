#include "menumanager.h"
#include "menu.h"
#include "game.h"
#include "logger.h"

#define RANGE(_x, _min, _max) (_x >= _min && _x <= _max)

MenuManager::MenuManager()
{
    MenuID menuIOs[] = {
        MENUID_MAINMENU,
        MENUID_GAMEMENU,
        MENUID_OPTIONMENU,
        MENUID_USERS,
        MENUID_SKILLS,
    };

    for (const auto &menuID : menuIOs)
    {
        m_menus[menuID] = std::make_unique<CMenu>(menuID);
    }

    m_lastMenu = nullptr;
}

CMenu *MenuManager::get(MenuID menuID)
{
    if (auto it = m_menus.find(menuID); it != m_menus.end())
        return it->second.get();
    return nullptr;
}

MenuManager::~MenuManager()
{
}

int MenuManager::menuItemAt(int x, int y)
{
    if (!m_lastMenu || !isMenuActive())
        return -1;

    CMenu &menu = *m_lastMenu;
    int baseY = m_lastMenuBaseY;
    int startY = baseY;
    for (size_t i = 0; i < menu.size(); ++i)
    {
        int h = menu.scaleY() * FONT_SIZE;
        if (RANGE(y, startY, startY + h))
        {
            // if (m_trace && !m_quiet)
            //   LOGI("menuItem at: %d %d ==> %zu", x, y, i);
            return i;
        }
        startY += h + menu.paddingY();
    }
    return -1;
}

void MenuManager::followPointer(int x, int y)
{
    if (!m_lastMenu)
        return;

    CMenu &menu = *m_lastMenu;
    int i = menuItemAt(x, y);
    if (i != INVALID)
        menu.setCurrent(i);
}

void MenuManager::setActive(const MenuID menuID, const bool active)
{
    m_menusActive[menuID] = active;
}

bool MenuManager::isMenuActive(const MenuID menuID)
{
    if (const auto it = m_menusActive.find(menuID); it != m_menusActive.end())
        return it->second;

    return false;
}

bool MenuManager::isMenuActive()
{
    const int mode = CGame::getGame()->mode();
    return mode == CGame::MODE_TITLE ||
           mode == CGame::MODE_USERSELECT ||
           mode == CGame::MODE_SKLLSELECT ||
           mode == CGame::MODE_OPTIONS ||
           isMenuActive(MENUID_GAMEMENU);
}

void MenuManager::toggleMenuActive(const MenuID menuID)
{
    m_menusActive[menuID] = !m_menusActive[menuID];
}