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

#include <unordered_set>
#include "../src/logger.h"
#include "../src/statedata.h"
#include "t_statedata.h"

bool test_statedata()
{
    const auto &opts = getKeyOptions();
    if (opts.empty())
    {
        LOGE("getKeyOptions() is empty");
        return false;
    }

    std::unordered_set<uint16_t> values;
    std::unordered_set<std::string> names;
    for (const auto &o : opts)
    {
        if (o.display.empty())
        {
            LOGE("KeyOption with empty display");
            return false;
        }
        if (!values.insert(o.value).second)
        {
            LOGE("duplicate KeyOption value 0x%.2x", o.value);
            return false;
        }
        if (!names.insert(o.display).second)
        {
            LOGE("duplicate KeyOption display `%s`", o.display.c_str());
            return false;
        }
    }

    // display names must match the StateValue they point at
    for (const auto &o : opts)
    {
        const bool ok = (o.display == "TIMEOUT" && o.value == TIMEOUT) ||
                        (o.display == "PRIVATE" && o.value == PRIVATE) ||
                        (o.display == "USERDEF1" && o.value == STATE_USERDEF1) ||
                        (o.display == "AUTHOR" && o.value == AUTHOR) ||
                        (o.display == "MSG0" && o.value == MSG0) ||
                        (o.display == "MSGF" && o.value == MSGF) ||
                        (o.display != "TIMEOUT" && o.display != "PRIVATE" &&
                         o.display != "USERDEF1" && o.display != "AUTHOR" &&
                         o.display != "MSG0" && o.display != "MSGF");
        if (!ok)
        {
            LOGE("KeyOption `%s` has value 0x%.2x", o.display.c_str(), o.value);
            return false;
        }
    }

    return true;
}
