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

#include "../../src/gameui.h"
#include "../../src/logger.h"
#include "t_gameui.h"

// Findings vs the round-2 plan assumptions (implementation is the spec here):
// - width()/height() are the max button extents (x+width / y+height);
//   margin is not part of them and setMargin() does not change them.
// - clear() empties the button list but keeps the computed extents.

bool test_gameui()
{
    CGameUI ui;
    if (ui.size() != 0 || ui.isVisible() || ui.width() != 0 || ui.height() != 0)
    {
        LOGE("fresh CGameUI not empty/hidden/zero-sized");
        return false;
    }
    if (ui.margin() != 8)
    {
        LOGE("default margin %d; expecting 8", ui.margin());
        return false;
    }

    const button_t first{1, 10, 20, 30, 40, "one", 0xff000000};
    const button_t &stored = ui.addButton(first);
    if (&stored != &ui.at(0))
    {
        LOGE("addButton does not return a reference into the UI");
        return false;
    }
    if (ui.size() != 1 || stored.id != 1 || stored.text != "one" ||
        stored.x != 10 || stored.y != 20 || stored.width != 30 ||
        stored.height != 40 || stored.color != 0xff000000)
    {
        LOGE("addButton field copy mismatch");
        return false;
    }
    if (ui.width() != 40 || ui.height() != 60)
    {
        LOGE("extents after one button: %d x %d; expecting 40 x 60",
             ui.width(), ui.height());
        return false;
    }

    // smaller button does not grow the extents
    ui.addButton(button_t{2, 0, 0, 5, 5, "two", 0});
    if (ui.width() != 40 || ui.height() != 60)
    {
        LOGE("smaller button changed extents to %d x %d", ui.width(), ui.height());
        return false;
    }
    // larger button grows both extents
    ui.addButton(button_t{3, 50, 70, 50, 50, "three", 0});
    if (ui.width() != 100 || ui.height() != 120)
    {
        LOGE("extents after growth: %d x %d; expecting 100 x 120",
             ui.width(), ui.height());
        return false;
    }

    // buttons() exposes the vector, insertion order preserved
    const auto &vec = ui.buttons();
    if (vec.size() != 3 || vec[0].id != 1 || vec[1].id != 2 || vec[2].id != 3)
    {
        LOGE("buttons() order mismatch");
        return false;
    }
    if (&vec[0] != &ui.at(0))
    {
        LOGE("buttons() is not the internal vector");
        return false;
    }

    ui.show();
    if (!ui.isVisible())
    {
        LOGE("show() not visible");
        return false;
    }
    ui.hide();
    if (ui.isVisible())
    {
        LOGE("hide() still visible");
        return false;
    }
    ui.show();
    ui.hide();
    if (ui.isVisible())
    {
        LOGE("show/hide round trip broken");
        return false;
    }

    ui.setMargin(20);
    if (ui.margin() != 20)
    {
        LOGE("setMargin ignored");
        return false;
    }
    if (ui.width() != 100 || ui.height() != 120)
    {
        LOGE("setMargin changed extents (%d x %d)", ui.width(), ui.height());
        return false;
    }

    ui.clear();
    if (ui.size() != 0)
    {
        LOGE("clear() left %zu buttons", ui.size());
        return false;
    }

    // extents are max-based, not tied to the current button list
    CGameUI empty;
    if (empty.width() != 0 || empty.height() != 0)
    {
        LOGE("empty UI extents %d x %d", empty.width(), empty.height());
        return false;
    }
    return true;
}
