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
#include "t_ifile.h"
#define LOG_TAG "t_map"
#include <cstring>
#include "t_map.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/FileMem.h"
#include "../src/shared/IFile.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include <filesystem>

constexpr const char *OUTFILE = "tests/out/ifile.dat";

bool infuse_create(IFile &file, const std::string outfile, const char *context)
{
    int filesize = 0;
    auto testPtr = [&file, &filesize, context](const char *now)
    {
        if (file.tell() != filesize)
        {
            LOGE("[%s][%s] filesize is %ld; expecting %d ", context, now, file.tell(), filesize);
            return false;
        }
        return true;
    };

    constexpr const char test[] = "This is a test string";
    constexpr const char emptystr[] = "";
    constexpr const char shortstr[] = "Hello World.";
    constexpr const char longstr[] = "This is a very long string. "
                                     "It tell the story of a boy and a girl who lived in a village "
                                     "years ago. The time was in a different era. Things were much "
                                     "simpler back then. Nowadays the sun is dimmer. Clouds are "
                                     "overhead and everything is grimm. Let's just get over the threshold.";

    constexpr const char rawstring[] = "This is a simple text.";
    constexpr const int32_t year = 2025;
    constexpr const bool IS_VALID = true;
    constexpr int SHORT_ENC = 1;
    constexpr int LONG_ENC = 3;

    if (!file.open(outfile.c_str(), "wb"))
    {
        LOGE("[%s] fail to create %s\n", context, outfile.c_str());
        return false;
    }
    if (!testPtr("origin"))
        return false;

    file.write(test, strlen(test));
    filesize += strlen(test);
    if (!testPtr("basic write"))
        return false;

    file << year;
    filesize += sizeof(year);
    if (!testPtr("int32_t"))
        return false;

    file << IS_VALID;
    filesize += sizeof(IS_VALID);
    if (!testPtr("boolean"))
        return false;

    file += rawstring;
    filesize += strlen(rawstring);
    if (!testPtr("rawstring"))
        return false;

    file << std::string(shortstr);
    filesize += strlen(shortstr) + SHORT_ENC;
    if (!testPtr("std::string shortstr"))
        return false;

    file << std::string(longstr);
    filesize += std::string(longstr).length() + LONG_ENC;
    if (!testPtr("std::string longstr"))
        return false;

    file += std::string(rawstring);
    filesize += strlen(rawstring);
    if (!testPtr("std::string rawstring"))
        return false;

    file << std::string(emptystr);
    filesize += SHORT_ENC;
    if (!testPtr("empty string streamed"))
        return false;

    file += emptystr;
    if (!testPtr("raw empty string"))
        return false;

    file += std::string(emptystr);
    if (!testPtr("raw empty string[2]"))
        return false;

    if (file.getSize() != filesize)
    {
        LOGE("filesize %ld wrong; expecting %d\n", file.getSize(), filesize);
        return false;
    }

    return true;
}

bool test_ifile()
{
    CFileWrap fw;
    if (!infuse_create(fw, OUTFILE, "FileWrap"))
        return false;
    fw.close();

    CFileMem fm;
    if (!infuse_create(fm, OUTFILE, "FileMem"))
        return false;
    fm.close();

    // clean up
    std::filesystem::remove(OUTFILE);

    return true;
}