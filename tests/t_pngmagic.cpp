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

#define LOG_TAG "t_pngmagic"
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

bool compareFiles(const char *f1, const char *f2)
{
    auto size1 = getFileSize(f1);
    auto size2 = getFileSize(f2);

    if (size1 != size2)
    {
        LOGE("file size mismatch: %ld, %ld\n",
             size1, size2);
        return false;
    }

    FILE *sfile1 = fopen(f1, "rb");
    if (!sfile1)
    {
        LOGE("can't read: %s\n", f1);
        return false;
    }

    FILE *sfile2 = fopen(f2, "rb");
    if (!sfile2)
    {
        fclose(sfile1);
        LOGE("can't read: %s\n", f2);
        return false;
    }

    char *buf1 = new char[size1];
    char *buf2 = new char[size2];
    fread(buf1, size1, 1, sfile1);
    fread(buf2, size2, 1, sfile2);

    for (size_t i = 0; i < size1; ++i)
    {
        if (buf1[i] != buf2[i])
        {
            LOGE("file mismatch at offset: %ld, %.2x %.2x", i, buf1[i], buf2[i]);
            delete[] buf1;
            delete[] buf2;
            return false;
        }
    }

    return true;
}

bool test_png_magic()
{
    CFrameSet fs;
    CFileWrap file;
    if (!file.open(INPUT_FILE, "rb"))
    {
        LOGE("can't open for reading %s\n", INPUT_FILE);
        return false;
    }

    if (!parsePNG(fs, file))
    {
        file.close();
        LOGE("can't decode png: %s; last_error: %s\n", INPUT_FILE, fs.getLastError());
        return false;
    }

    file.close();

    uint8_t *data;
    int size;
    if (!fs.toPng(data, size))
    {
        LOGE("cannot convert to png\n");
        return false;
    }

    if (!file.open(OUTPUT_FILE, "wb"))
    {
        LOGE("can't open for writing %s\n", OUTPUT_FILE);
        return false;
    }

    if (file.write(data, size) != 1)
    {
        LOGE("failed to write file: %s\n", OUTPUT_FILE);
        return false;
    }

    file.close();

    if (getFileSize(INPUT_FILE) != getFileSize(OUTPUT_FILE))
    {
        LOGE("file size mismatch: %ld; expecting  %ld\n",
             getFileSize(INPUT_FILE), getFileSize(OUTPUT_FILE));
        return false;
    }

    if (!compareFiles(INPUT_FILE, OUTPUT_FILE))
    {
        return false;
    }

    // clean up
    std::filesystem::remove(OUTPUT_FILE);

    return true;
}