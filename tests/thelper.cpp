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
#include "../src/logger.h"

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

bool compareFiles(const char *f1, const char *f2)
{
    auto size1 = getFileSize(f1);
    auto size2 = getFileSize(f2);

    if (size1 != size2)
    {
        LOGE("file size mismatch: %ld, %ld [%s, %s]",
             size1, size2, f1, f2);
        return false;
    }

    FILE *sfile1 = fopen(f1, "rb");
    if (!sfile1)
    {
        LOGE("can't read: %s", f1);
        return false;
    }

    FILE *sfile2 = fopen(f2, "rb");
    if (!sfile2)
    {
        fclose(sfile1);
        LOGE("can't read: %s", f2);
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
            LOGE("file mismatch at offset: %ld, %.2x %.2x [%s. %s]",
                 i, buf1[i], buf2[i], f1, f2);
            delete[] buf1;
            delete[] buf2;
            return false;
        }
    }

    return true;
}
