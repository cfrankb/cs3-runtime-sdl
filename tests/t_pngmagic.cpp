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
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include "../src/gamemixin.h"
#include "../src/skills.h"
#include "../src/runtime.h"
#include "../src/game.h"
#include "../src/shared/PngMagic.h"
#include "../src/shared/FrameSet.h"
#include "thelper.h"
#include "t_runtime.h"
#include <cstdio>
#include <filesystem>

constexpr const char *INPUT_FILE = "tests/in/annie.obl";
constexpr const char *OUTPUT_FILE = "tests/out/annie.obl";

bool test_png_magic()
{
    CFrameSet fs;
    CFileWrap file;
    if (!file.open(INPUT_FILE, "rb"))
    {
        LOGE("can't open for reading %s", INPUT_FILE);
        return false;
    }

    if (!parsePNG(fs, file))
    {
        file.close();
        LOGE("can't decode png: %s; last_error: %s", INPUT_FILE, fs.getLastError());
        return false;
    }

    file.close();

    std::vector<uint8_t> png;
    if (!fs.toPng(png))
    {
        LOGE("cannot convert to png");
        return false;
    }

    if (!file.open(OUTPUT_FILE, "wb"))
    {
        LOGE("can't open for writing %s", OUTPUT_FILE);
        return false;
    }

    if (file.write(png.data(), png.size()) != 1)
    {
        LOGE("failed to write file: %s", OUTPUT_FILE);
        return false;
    }

    file.close();

    if (!compareFiles(INPUT_FILE, OUTPUT_FILE))
    {
        return false;
    }

    // clean up
    std::filesystem::remove(OUTPUT_FILE);

    return true;
}