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
#include "t_strhelper.h"
#include <cstdio>
#include <cstring>
#include "../src/strhelper.h"

bool test_strhelper()
{
    std::string t1 = "toto123 ";
    // printf("`%s` => `%s`\n", t1.c_str(), trimString(t1).c_str());
    if (trimString(t1) != "toto123")
        return false;

    std::string t2 = " toto123";
    // printf("`%s` => `%s`\n", t2.c_str(), trimString(t2).c_str());
    if (trimString(t2) != "toto123")
        return false;

    std::string t3 = " toto123 ";
    // printf("`%s` => `%s`\n", t3.c_str(), trimString(t3).c_str());
    if (trimString(t3) != "toto123")
        return false;

    std::string t4 = "toto123";
    // printf("`%s` => `%s`\n", t4.c_str(), trimString(t4).c_str());
    if (trimString(t4) != "toto123")
        return false;

    std::string t5 = "";
    // printf("`%s` => `%s`\n", t5.c_str(), trimString(t5).c_str());
    if (trimString(t5) != "")
        return false;

    std::string t6 = " ";
    // printf("`%s` => `%s`\n", t6.c_str(), trimString(t6).c_str());
    if (trimString(t6) != "")
        return false;

    std::string t7 = " a";
    // printf("`%s` => `%s`\n", t7.c_str(), trimString(t7).c_str());
    if (trimString(t7) != "a")
        return false;

    std::string t8 = "\ta";
    if (trimString(t8) != "a")
        return false;

    std::string t9 = "\ra";
    if (trimString(t9) != "a")
        return false;

    return true;
}