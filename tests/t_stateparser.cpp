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
#include "../src/stateparser.h"
#include "../src/states.h"
#include <cstring>

bool checkValue(CStateParser &parser, const char *k, const uint16_t v)
{
    const auto a = parser.get(k);
    if (a != v)
    {
        fprintf(stderr, "key %s contains wrong value %.2x -- was expecting %.2x\n",
                k, a, v);
        return false;
    }
    return true;
}

bool test_stateparser()
{
    printf("==> test_stateparser()\n");

    CStateParser parser;
    parser.parse(
        "KITTY 128\n"
        "SONIA 0x90\n"
        "HENRI 1\n"
        "KOALA -1\n"
        "\n"
        " ALANA 0x1990");

    if (!checkValue(parser, "KITTY", 128))
        return false;
    if (!checkValue(parser, "SONIA", 0x90))
        return false;
    if (!checkValue(parser, "HENRI", 1))
        return false;
    if (!checkValue(parser, "KOALA", 0xffff))
        return false;
    if (!checkValue(parser, "ALANA", 0x1990))
        return false;

    parser.debug();

    CStates states;
    parser.parseStates("KITTY 64\n"
                       "SONIA 0x2020\n"
                       "HENRI Sunny_Outside",
                       states);
    states.debug();

    return true;
}