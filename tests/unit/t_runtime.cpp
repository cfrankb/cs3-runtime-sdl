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

#include <cstring>
#include <filesystem>
#include "../../src/shared/FileWrap.h"
#include "../../src/shared/helper.h"
#include "../../src/logger.h"
#include "../../src/gamemixin.h"
#include "../../src/skills.h"
#include "../../src/runtime.h"
#include "../../src/game.h"
#include "thelper.h"
#include "t_runtime.h"

bool test_runtime()
{
    constexpr const char *GEN_FILE = "tests/out/savegame-gen.dat";
    constexpr const char *OUT_FILE = "tests/out/savegame-cs3.dat";

    // Generate the savegame with the current engine: a checked-in fixture
    // written by an older ENGINE_VERSION is rejected on load by design
    // (CGame::validateSignature), which would fail this test on every
    // version bump. Default-constructed CRuntime holds a default CGame.
    std::string name = "test";
    CRuntime runtime;
    runtime.setQuiet(true);

    // a default (0x0, layerless) map can't round-trip: zero dimensions are
    // written as 0 but read back as MAX_SIZE; give the game map real content
    if (!CGame::getMap().resize(20, 25, 0, true))
    {
        LOGE("failed to resize game map");
        return false;
    }

    if (!runtime.saveToFile(GEN_FILE, name))
    {
        LOGE("failed to write %s", GEN_FILE);
        return false;
    }

    if (!runtime.loadFromFile(GEN_FILE, name))
    {
        LOGE("read error: %s", GEN_FILE);
        return false;
    }

    if (!runtime.saveToFile(OUT_FILE, name))
    {
        LOGE("failed to write %s", OUT_FILE);
        return false;
    }

    if (!compareFiles(GEN_FILE, OUT_FILE))
    {
        LOGE("%s and %s differ", GEN_FILE, OUT_FILE);
        return false;
    }

    // clean up
    std::filesystem::remove(GEN_FILE);
    std::filesystem::remove(OUT_FILE);
    return true;
}
