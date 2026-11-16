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

#include "../src/game.h"
#include "../src/logger.h"
#include "../src/menu.h"
#include "../src/menumanager.h"
#include "t_menu.h"

// Findings vs the round-2 plan assumptions (implementation is the spec here):
// - up()/down() do NOT wrap at the ends; they return false and stay put.
// - disabled items are NOT skipped by up()/down().
// - range left()/right() step the raw value by 1 within [min,max];
//   m_start/m_factor only scale the str() rendering.
// - options/bar left()/right() clamp at 0 and size-1; they do not wrap.
// - menuItemAt() hit-tests the y band only (x is ignored) and reports
//   disabled rows as hits.

bool test_menu_nav()
{
    CMenu menu(1, 2, 2, 4);
    if (menu.size() != 0 || menu.id() != 1)
    {
        LOGE("fresh menu size %zu id %d", menu.size(), menu.id());
        return false;
    }
    menu.addItem(CMenuItem("alpha"));
    menu.addItem(CMenuItem("beta"));
    menu.addItem(CMenuItem("gamma"));
    if (menu.size() != 3)
    {
        LOGE("menu size %zu; expecting 3", menu.size());
        return false;
    }
    if (&menu.last() != &menu.at(2))
    {
        LOGE("last() is not the last item");
        return false;
    }

    if (menu.index() != 0 || menu.up())
    {
        LOGE("up() at first item returned true");
        return false;
    }
    if (!menu.down() || menu.index() != 1)
    {
        LOGE("down() from 0 did not move to 1");
        return false;
    }
    if (!menu.down() || menu.index() != 2)
    {
        LOGE("down() from 1 did not move to 2");
        return false;
    }
    if (menu.down() || menu.index() != 2)
    {
        LOGE("down() at last item moved the caret");
        return false;
    }
    if (&menu.current() != &menu.at(menu.index()))
    {
        LOGE("current() does not match index()");
        return false;
    }

    menu.setCurrent(1);
    if (menu.index() != 1)
    {
        LOGE("setCurrent(1) ignored");
        return false;
    }
    menu.setCurrent(99);
    if (menu.index() != 0)
    {
        LOGE("setCurrent(99) not clamped to 0");
        return false;
    }
    menu.setCurrent(-1);
    if (menu.index() != 0)
    {
        LOGE("setCurrent(-1) not clamped to 0");
        return false;
    }

    if (menu.at(-1).str() != menu.at(0).str() ||
        menu.at(7).str() != menu.at(0).str())
    {
        LOGE("at() out of range does not fall back to item 0");
        return false;
    }

    menu.disableCaret();
    if (!menu.isCaretDisabled())
    {
        LOGE("disableCaret() not recorded");
        return false;
    }
    if (!menu.down() || menu.index() != 1 || !menu.up() || menu.index() != 0)
    {
        LOGE("caret-disabled menu does not navigate");
        return false;
    }
    return true;
}

bool test_menu_disabled()
{
    CMenu menu;
    menu.addItem(CMenuItem("on"));
    menu.addItem(CMenuItem("off").disable());
    menu.addItem(CMenuItem("on again"));

    if (menu.at(0).isDisabled() || !menu.at(1).isDisabled() ||
        menu.at(2).isDisabled())
    {
        LOGE("disable() state mismatch");
        return false;
    }
    menu.at(0).disable(false);
    if (menu.at(0).isDisabled())
    {
        LOGE("disable(false) did not re-enable");
        return false;
    }

    // actual behaviour: the caret visits disabled items (finding: the plan
    // assumed skipping); just pin that navigation cannot hang.
    size_t visited = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (!menu.down())
        {
            break;
        }
        ++visited;
    }
    if (visited != 2)
    {
        LOGE("disabled-item menu moved %zu times; expecting 2", visited);
        return false;
    }

    CMenu dead;
    dead.addItem(CMenuItem("x").disable());
    dead.addItem(CMenuItem("y").disable());
    if (!dead.down() || dead.index() != 1 || dead.down() ||
        !dead.up() || dead.index() != 0 || dead.up())
    {
        LOGE("all-disabled menu moved out of bounds");
        return false;
    }
    return true;
}

bool test_item_range()
{
    int value = 3;
    CMenuItem item("vol %d", 2, 6, &value);
    if (item.type() != CMenuItem::ITEM_RANGE || item.value() != 3)
    {
        LOGE("range ctor mismatch type %d value %d", item.type(), item.value());
        return false;
    }
    if (!item.right() || value != 4)
    {
        LOGE("range right() did not increment");
        return false;
    }
    int steps = 0;
    while (item.right())
    {
        ++steps;
    }
    if (value != 6 || steps != 2)
    {
        LOGE("range right() stopped at %d after %d steps", value, steps);
        return false;
    }
    while (item.left())
    {
    }
    if (value != 2 || item.left())
    {
        LOGE("range left() did not clamp at min (%d)", value);
        return false;
    }

    // m_start/m_factor scale str() only
    int raw = 2;
    CMenuItem scaled("x%d", 0, 10, &raw, 1, 2);
    if (scaled.str() != "x6")
    {
        LOGE("scaled str() '%s'; expecting 'x6'", scaled.str().c_str());
        return false;
    }

    CMenuItem stat("static");
    if (stat.left() || stat.right() || stat.value() != 0)
    {
        LOGE("static item without a value pointer mutated state");
        return false;
    }
    return true;
}

bool test_item_options()
{
    const std::vector<std::string> options = {"lo", "mid", "hi"};
    int value = 0;
    CMenuItem item("speed: %s", options, &value);
    if (item.type() != CMenuItem::ITEM_OPTIONS || item.size() != 3)
    {
        LOGE("options ctor mismatch type %d size %zu", item.type(), item.size());
        return false;
    }
    if (item.option(0) != "lo" || item.option(2) != "hi")
    {
        LOGE("option() round-trip failed");
        return false;
    }
    if (!item.right() || !item.right() || value != 2)
    {
        LOGE("options right() did not advance to last");
        return false;
    }
    if (item.right() || value != 2)
    {
        LOGE("options right() wrapped past the last option (finding: plan expected wrap)");
        return false;
    }
    if (!item.left() || !item.left() || item.left() || value != 0)
    {
        LOGE("options left() did not clamp at 0");
        return false;
    }

    item.setRole(42).setUserData(99).setType(CMenuItem::ITEM_STATIC);
    if (item.role() != 42 || item.userData() != 99 ||
        item.type() != CMenuItem::ITEM_STATIC)
    {
        LOGE("accessor round-trip failed");
        return false;
    }

    int bar = 1;
    CMenuItem barItem(options, &bar, CMenuItem::ITEM_BAR);
    if (barItem.type() != CMenuItem::ITEM_BAR)
    {
        LOGE("bar ctor type %d", barItem.type());
        return false;
    }
    if (!barItem.right() || bar != 2 || barItem.right())
    {
        LOGE("bar right() bounds wrong (value %d)", bar);
        return false;
    }
    while (barItem.left())
    {
    }
    if (bar != 0 || barItem.left())
    {
        LOGE("bar left() did not clamp at 0 (value %d)", bar);
        return false;
    }
    return true;
}

bool test_item_str()
{
    CMenuItem stat("plain label");
    if (stat.str() != "plain label")
    {
        LOGE("static str() '%s'", stat.str().c_str());
        return false;
    }

    int raw = 2;
    CMenuItem range("vol %d", 0, 10, &raw, 1, 2);
    std::string rendered = range.str();
    if (rendered.find("vol") == std::string::npos ||
        rendered.find(std::to_string((raw + 1) * 2)) == std::string::npos)
    {
        LOGE("range str() '%s' misses label or value", rendered.c_str());
        return false;
    }

    const std::vector<std::string> options = {"lo", "mid", "hi"};
    int value = 1;
    CMenuItem opts("speed: %s", options, &value);
    if (opts.str() != "speed: mid")
    {
        LOGE("options str() '%s'", opts.str().c_str());
        return false;
    }

    int bar = 0;
    CMenuItem barItem(options, &bar, CMenuItem::ITEM_BAR);
    if (barItem.str().empty())
    {
        LOGE("bar str() is empty");
        return false;
    }
    return true;
}

bool test_menu_roles()
{
    CMenu menu;
    menu.addItem(CMenuItem("a").setRole(10));
    menu.addItem(CMenuItem("b").setRole(20));
    menu.addItem(CMenuItem("c").setRole(30));

    if (!menu.containsRole(20) || menu.containsRole(99))
    {
        LOGE("containsRole mismatch");
        return false;
    }
    CMenuItem *hit = menu.getItemByRole(20);
    if (!hit || hit->role() != 20 || menu.getItemByRole(99) != nullptr)
    {
        LOGE("getItemByRole mismatch");
        return false;
    }

    menu.setCurrent(0);
    menu.moveItem(30, 0); // [30,10,20]; item crosses the caret, caret 0->1
    if (menu.at(0).role() != 30 || menu.at(1).role() != 10 ||
        menu.at(2).role() != 20)
    {
        LOGE("moveItem ordering wrong after move to 0");
        return false;
    }
    if (menu.index() != 1)
    {
        LOGE("caret %d did not shift when an item moved across it", menu.index());
        return false;
    }

    // caret tracks the moved item to its new position
    menu.setCurrent(0); // role 30
    menu.moveItem(30, 2); // [10,20,30]
    if (menu.index() != 2 || menu.at(2).role() != 30)
    {
        LOGE("caret %d did not follow the moved item to 2", menu.index());
        return false;
    }
    menu.moveItem(20, -5); // clamped to 0 -> [20,10,30]
    if (menu.at(0).role() != 20)
    {
        LOGE("moveItem with negative pos did not clamp to front");
        return false;
    }
    size_t sizeBefore = menu.size();
    menu.moveItem(77, 1); // unknown role: no-op
    if (menu.size() != sizeBefore)
    {
        LOGE("moveItem for unknown role changed size");
        return false;
    }

    menu.removeRole(20);
    if (menu.containsRole(20) || menu.size() != 2)
    {
        LOGE("removeRole left %zu items", menu.size());
        return false;
    }
    return true;
}

bool test_menu_scale()
{
    CMenu menu(7, 3, 4, 5);
    if (menu.scaleX() != 3 || menu.scaleY() != 4 || menu.paddingY() != 5)
    {
        LOGE("ctor scale/padding mismatch");
        return false;
    }
    if (menu.height() != 0)
    {
        LOGE("empty menu height %d", menu.height());
        return false;
    }
    menu.addItem(CMenuItem("one"));
    menu.addItem(CMenuItem("two"));
    const int row = menu.scaleY() * 8 + menu.paddingY();
    if (menu.height() != 2 * row - menu.paddingY())
    {
        LOGE("height %d; expecting %d", menu.height(), 2 * row - menu.paddingY());
        return false;
    }
    menu.setScaleX(9);
    menu.setScaleY(1);
    if (menu.scaleX() != 9 || menu.scaleY() != 1)
    {
        LOGE("setScaleX/Y ignored");
        return false;
    }
    if (menu.height() >= 2 * row)
    {
        LOGE("height did not shrink with scaleY");
        return false;
    }
    menu.setCurrent(1);
    menu.clear();
    if (menu.size() != 0 || menu.height() != 0 || menu.index() != 0)
    {
        LOGE("clear() left size %zu height %d index %d",
             menu.size(), menu.height(), menu.index());
        return false;
    }
    return true;
}

bool test_mgr_get()
{
    MenuManager mgr;
    CMenu *first = mgr.get(MENUID_MAINMENU);
    CMenu *again = mgr.get(MENUID_MAINMENU);
    if (!first || first != again)
    {
        LOGE("get() does not return a stable pointer");
        return false;
    }
    if (first->id() != MENUID_MAINMENU)
    {
        LOGE("menu id %d; expecting %d", first->id(), MENUID_MAINMENU);
        return false;
    }
    if (mgr.get(MENUID_GAMEMENU) == mgr.get(MENUID_USERS))
    {
        LOGE("distinct ids share a menu");
        return false;
    }
    if (mgr.last() != nullptr)
    {
        LOGE("last() is not null before setLastMenu");
        return false;
    }
    mgr.setLastMenu(first, 1, 2);
    if (mgr.last() != first)
    {
        LOGE("setLastMenu not recorded");
        return false;
    }
    return true;
}

bool test_mgr_active()
{
    MenuManager mgr;
    CGame *game = CGame::getGame();
    const CGame::GameMode saved = game->mode();

    if (mgr.isMenuActive(MENUID_USERS))
    {
        LOGE("menu active before setActive");
        return false;
    }
    mgr.setActive(MENUID_USERS, true);
    if (!mgr.isMenuActive(MENUID_USERS) || mgr.isMenuActive(MENUID_SKILLS))
    {
        LOGE("setActive leaked to another menu");
        return false;
    }
    mgr.toggleMenuActive(MENUID_USERS);
    if (mgr.isMenuActive(MENUID_USERS))
    {
        LOGE("toggle did not deactivate");
        return false;
    }
    mgr.toggleMenuActive(MENUID_USERS);
    if (!mgr.isMenuActive(MENUID_USERS))
    {
        LOGE("toggle did not reactivate");
        return false;
    }

    game->setMode(CGame::MODE_TITLE);
    if (!mgr.isMenuActive())
    {
        LOGE("title mode is not menu-active");
        game->setMode(saved);
        return false;
    }
    game->setMode(CGame::MODE_PLAY);
    if (mgr.isMenuActive())
    {
        LOGE("play mode with no active menu id reported active");
        game->setMode(saved);
        return false;
    }
    mgr.setActive(MENUID_GAMEMENU, true);
    if (!mgr.isMenuActive())
    {
        LOGE("active gamemenu during play not menu-active");
        game->setMode(saved);
        return false;
    }
    game->setMode(saved);
    return true;
}

bool test_mgr_hit()
{
    MenuManager mgr;
    CGame *game = CGame::getGame();
    const CGame::GameMode saved = game->mode();
    game->setMode(CGame::MODE_PLAY);

    if (mgr.menuItemAt(10, 100) != -1)
    {
        LOGE("hit reported with no last menu");
        game->setMode(saved);
        return false;
    }

    CMenu *menu = mgr.get(MENUID_GAMEMENU);
    menu->clear();
    menu->addItem(CMenuItem("one"));
    menu->addItem(CMenuItem("two"));
    menu->addItem(CMenuItem("three").disable());
    mgr.setLastMenu(menu, 0, 100);

    // inactive menu: no hits even on a row
    if (mgr.menuItemAt(0, 100) != -1)
    {
        LOGE("hit reported while no menu is active");
        game->setMode(saved);
        return false;
    }
    mgr.setActive(MENUID_GAMEMENU, true);

    // rows are scaleY*8 tall, spaced by +padding
    const int h = menu->scaleY() * 8;
    const int step = h + menu->paddingY();
    if (mgr.menuItemAt(0, 99) != -1)
    {
        LOGE("hit above the first row");
        game->setMode(saved);
        return false;
    }
    if (mgr.menuItemAt(0, 100) != 0 || mgr.menuItemAt(0, 100 + h) != 0)
    {
        LOGE("row 0 band mismatch");
        game->setMode(saved);
        return false;
    }
    if (mgr.menuItemAt(0, 100 + h + 1) != -1)
    {
        LOGE("hit inside the padding gap");
        game->setMode(saved);
        return false;
    }
    if (mgr.menuItemAt(0, 100 + step) != 1)
    {
        LOGE("row 1 miss");
        game->setMode(saved);
        return false;
    }
    // finding: disabled rows are hits (plan expected a miss)
    if (mgr.menuItemAt(0, 100 + 2 * step) != 2)
    {
        LOGE("row 2 miss");
        game->setMode(saved);
        return false;
    }
    if (mgr.menuItemAt(0, 100 + 3 * step) != -1)
    {
        LOGE("hit below the last row");
        game->setMode(saved);
        return false;
    }

    menu->setCurrent(0);
    mgr.followPointer(0, 100 + step);
    if (menu->index() != 1)
    {
        LOGE("followPointer hit did not move the caret");
        game->setMode(saved);
        return false;
    }
    mgr.followPointer(0, 9999);
    if (menu->index() != 1)
    {
        LOGE("followPointer miss moved the caret");
        game->setMode(saved);
        return false;
    }

    mgr.setLastMenu(nullptr, -1, -1);
    if (mgr.menuItemAt(0, 100) != -1)
    {
        LOGE("hit reported after last menu cleared");
        game->setMode(saved);
        return false;
    }
    game->setMode(saved);
    return true;
}
