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
#include "t_frameset.h"
#include "../src/shared/FrameSet.h"
#include "../src/shared/FileWrap.h"
#include "../src/logger.h"

constexpr const char *IN_HEADY_MCX = "tests/in/mcx/heady.mcx";
constexpr const char *IN_HEADY_OBL4 = "tests/in/obl4/heady.obl";
constexpr const char *IN_HEADY_OBL5 = "tests/in/obl5/heady.obl";
constexpr const char *OUT_HEADY_PNG1 = "tests/out/heady1.png";
constexpr const char *OUT_HEADY_PNG2 = "tests/out/heady2.png";
constexpr const char *OUT_HEADY_PNG3 = "tests/out/heady3.png";

bool read_data_in(CFrameSet &fs, const char *filepath)
{
    CFileWrap file;
    if (!file.open(filepath, "rb"))
    {
        LOGE("can't read %s\n", filepath);
        return false;
    }

    if (!fs.extract(file, nullptr))
    {
        LOGE("can't extract from %s\n", filepath);
        return false;
    }
    file.close();
    return true;
}

bool write_data_out(CFrameSet &fs, const char *filepath)
{
    CFileWrap file;
    if (!file.open(filepath, "wb"))
    {
        LOGE("can't create %s\n", filepath);
        return false;
    }

    uint8_t *data;
    int size;
    if (!fs.toPng(data, size))
    {
        LOGE("can't convert to png\n");
        return false;
    }

    if (file.write(data, size) != 1)
    {
        delete[] data;
        LOGE("fail to write to %s\n", filepath);
        return false;
    }
    delete[] data;
    file.close();

    return true;
}

bool do_pair(const char *source, const char *target)
{
    CFrameSet fs;
    if (!read_data_in(fs, source))
    {
        LOGE("failed to read %s\n", source);
        return false;
    }
    if (!write_data_out(fs, target))
    {
        LOGE("failed to write %s\n", target);
        return false;
    }
    return true;
}

bool test_frameset()
{
    return do_pair(IN_HEADY_MCX, OUT_HEADY_PNG1) &&
           do_pair(IN_HEADY_OBL4, OUT_HEADY_PNG2) &&
           do_pair(IN_HEADY_OBL5, OUT_HEADY_PNG3);
    return true;
}