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

bool checkState(CStates &states, const uint16_t k, const uint16_t v)
{
    const auto a = states.getU(k);
    if (a != v)
    {
        fprintf(stderr, "key %u / %.2x contains wrong value %.2x -- was expecting %.2x\n",
                k, k, a, v);
        return false;
    }
    return true;
}

bool checkState(CStates &states, const uint16_t k, const std::string v)
{
    const auto a = states.getS(k);
    if (a != v)
    {
        fprintf(stderr, "key %u / %.2x contains wrong value `%s` -- was expecting `%s`\n",
                k, k, a, v.c_str());
        return false;
    }
    return true;
}

bool test_stateparser()
{
    // Create state aliases
    CStateParser parser;
    parser.parse(
        "KITTY 128\n"
        "SONIA 0x90\n"
        "HENRI 1\n"
        "KOALA -1\n"
        "\n"
        " ALANA 0x1990");

    enum
    {
        KITTY = 128,
        SONIA = 0x90,
        HENRI = 1,
        KOALA = 0xffff,
        ALANA = 0x1990,
    };

    if (!checkValue(parser, "KITTY", KITTY))
        return false;
    if (!checkValue(parser, "SONIA", SONIA))
        return false;
    if (!checkValue(parser, "HENRI", HENRI))
        return false;
    if (!checkValue(parser, "KOALA", KOALA))
        return false;
    if (!checkValue(parser, "ALANA", ALANA))
        return false;

    //  parser.debug();

    // populate state entities using aliases
    CStates states;
    parser.parseStates("KITTY 64\n"
                       "SONIA 0x2020\n"
                       "HENRI Sunny_Outside",
                       states);

    if (!checkState(states, KITTY, 64))
        return false;
    if (!checkState(states, SONIA, 0x2020))
        return false;
    if (!checkState(states, HENRI, "Sunny_Outside"))
        return false;

    // states.debug();
    return true;
}