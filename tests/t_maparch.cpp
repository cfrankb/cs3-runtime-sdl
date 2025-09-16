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
#include <filesystem>
#include "t_maparch.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"

#define KEY1 0x1990
#define KEY2 0x1990
#define VAL1 0x0522
#define VAL2 "Beginning of the End"
#define IN_FILE "tests/in/levels1.mapz"
#define OUT_FILE1 "tests/out/levels1.mapz"
#define OUT_FILE2 "tests/out/levels2.mapz"

namespace fs = std::filesystem;

uint64_t getFileSize(const std::string &filename)
{
    try
    {
        return fs::file_size(filename);
    }
    catch (const fs::filesystem_error &e)
    {
        throw std::runtime_error("Error accessing file: " + std::string(e.what()));
    }
}

void injectStates(CMapArch &arch)
{
    CMap *map = arch.at(0);
    CStates &states = map->states();
    states.setU(KEY1, VAL1);
    states.setS(KEY2, VAL2);
}

bool checkInjectedStates(CMapArch &arch, const char *context)
{
    CMap *map = arch.at(0);
    auto &states = map->states();
    if (states.getU(KEY1) != VAL1)
    {
        fprintf(stderr, "missing injected state. %s\n", context);
        return false;
    }

    if (std::string(states.getS(KEY2)) != VAL2)
    {
        fprintf(stderr, "missing injected state. %s\n", context);
        return false;
    }
    return true;
}

bool test_maparch()
{
    CMapArch arch1;
    arch1.read(IN_FILE);
    injectStates(arch1);
    if (!checkInjectedStates(arch1, "initial injection"))
    {
        return false;
    }

    // test writing: method 1
    arch1.write(OUT_FILE1);

    // test reading: method 1
    CMapArch arch2;
    arch2.read(OUT_FILE1);
    if (arch1.size() != arch2.size())
    {
        fprintf(stderr, "map count mismatch in arch after write\n");
        return false;
    }

    // test reading: method 2
    CMapArch arch3;
    CFileWrap file;
    file.open(OUT_FILE1);
    arch3.read(file);
    file.close();

    // test writing: method 2
    arch3.write(OUT_FILE2);
    if (getFileSize(OUT_FILE1) != getFileSize(OUT_FILE2))
    {
        fprintf(stderr, "different disk size for identical content\n");
        return false;
    }

    // check persistence after reload
    if (!checkInjectedStates(arch3, "data persistence"))
    {
        return false;
    }
    return true;
}