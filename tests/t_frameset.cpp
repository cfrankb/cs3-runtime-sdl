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
#include "thelper.h"
#include <vector>
#include <filesystem>

constexpr const char *IN_HEADY_MCX = "tests/in/mcx/heady.mcx";
constexpr const char *IN_HEADY_OBL4 = "tests/in/obl4/heady.obl";
constexpr const char *IN_HEADY_OBL5 = "tests/in/obl5/heady.obl";
constexpr const char *OUT_PATH = "tests/out/heady%s%s";

enum Format
{
    PNG,
    OBL5_UNPACKED,
    OBL5_SOLID,
};

bool read_data_in(CFrameSet &fs, const char *filepath)
{
    CFileWrap file;
    if (!file.open(filepath, "rb"))
    {
        LOGE("can't read %s\n", filepath);
        return false;
    }

    if (!fs.extract(file))
    {
        LOGE("can't extract from %s: %s\n", filepath, fs.getLastError());
        return false;
    }
    file.close();
    return true;
}

bool write_data_out(CFrameSet &fs, const char *filepath, Format format)
{
    CFileWrap file;
    if (!file.open(filepath, "wb"))
    {
        LOGE("can't create %s\n", filepath);
        return false;
    }

    if (format == PNG)
    {
        std::vector<uint8_t> png;
        if (!fs.toPng(png))
        {
            LOGE("can't convert to png\n");
            return false;
        }

        if (file.write(png.data(), png.size()) != IFILE_OK)
        {
            LOGE("fail to write to %s\n", filepath);
            return false;
        }
    }
    else if (format == OBL5_SOLID)
    {
        if (!fs.write(file, CFrameSet::OBL5_SOLID))
        {
            LOGE("fail to write to %s\n", filepath);
            return false;
        }
    }
    else if (format == OBL5_UNPACKED)
    {
        if (!fs.write(file, CFrameSet::OBL5_UNPACKED))
        {
            LOGE("fail to write to %s\n", filepath);
            return false;
        }
    }
    else
    {
        LOGE("unknown format: %d\n", format);
        return true;
    }

    file.close();

    return true;
}

bool performDoubleBlind(CFrameSet &fs, const Format format, const char *out_path, const char *out_path2)
{
    // write out file
    if (!write_data_out(fs, out_path, format))
    {
        LOGE("failed to write %s\n", out_path);
        return false;
    }

    // read back file
    CFrameSet fs2;
    if (!read_data_in(fs2, out_path))
    {
        LOGE("failed to read %s\n", out_path);
        return false;
    }

    // writeback file
    if (!write_data_out(fs2, out_path2, PNG))
    {
        LOGE("failed to write %s\n", out_path2);
        return false;
    }

    return true;
}

bool test_frameset_seq(const char *source, const char *name)
{
    std::vector<std::string> listOutFiles;
    std::vector<std::string> refFiles;

    CFrameSet fs;
    if (!read_data_in(fs, source))
    {
        LOGE("failed to read %s\n", source);
        return false;
    }

    // plain reference png
    char out_path[256];
    char out_path2[256];
    snprintf(out_path, sizeof(out_path), OUT_PATH, name, ".png");
    if (!write_data_out(fs, out_path, PNG))
    {
        LOGE("failed to write %s\n", out_path);
        return false;
    }
    refFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path);

    snprintf(out_path, sizeof(out_path), OUT_PATH, name, "-obl5.obl");
    snprintf(out_path2, sizeof(out_path2), OUT_PATH, name, "-obl5.png");

    if (!performDoubleBlind(fs, OBL5_UNPACKED, out_path, out_path2))
        return false;
    listOutFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path2);
    refFiles.emplace_back(out_path2);

    snprintf(out_path, sizeof(out_path), OUT_PATH, name, "-obl5s.obl");
    snprintf(out_path2, sizeof(out_path2), OUT_PATH, name, "-obl5s.png");
    if (!performDoubleBlind(fs, OBL5_SOLID, out_path, out_path2))
        return false;
    listOutFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path2);
    refFiles.emplace_back(out_path2);

    if (!compareFiles(refFiles[0].c_str(), refFiles[1].c_str()))
        return false;

    if (!compareFiles(refFiles[0].c_str(), refFiles[2].c_str()))
        return false;

    // clean up
    for (const auto &path : listOutFiles)
        std::filesystem::remove(path);

    return true;
}

bool test_frameset_serializer()
{
    return test_frameset_seq(IN_HEADY_MCX, "mcx") &&
           test_frameset_seq(IN_HEADY_OBL4, "obl4") &&
           test_frameset_seq(IN_HEADY_OBL5, "obl5");
    return true;
}

bool test_frameset()
{
    return test_frameset_serializer();
}