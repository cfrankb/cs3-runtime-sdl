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

#define LOG_TAG "t_runtime"
#include <cstring>
#include <filesystem>
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include "../src/gamemixin.h"
#include "../src/skills.h"
#include "../src/runtime.h"
#include "thelper.h"
#include "t_runtime.h"

constexpr const char *IN_FILE = "tests/in/savegame-cs3.dat";
constexpr const char *OUT_FILE = "tests/out/savegame-cs3.dat";

bool test_runtime()
{
    CRuntime runtime;
    std::string name;
    if (!runtime.loadFromFile(IN_FILE, name))
    {
        LOGE("read error: %s \n", IN_FILE);
        return false;
    }

    if (!runtime.saveToFile(OUT_FILE, name))
    {
        LOGE("failed write %s\n", OUT_FILE);
        return false;
    }

    if (getFileSize(OUT_FILE) != getFileSize(IN_FILE))
    {
        LOGE("different disk size for %s and %s; %ld != %ld\n",
             IN_FILE, OUT_FILE, getFileSize(IN_FILE), getFileSize(OUT_FILE));
        return false;
    }

    // clean up
    std::filesystem::remove(OUT_FILE);
    return true;
}