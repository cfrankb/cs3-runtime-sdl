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
#include "../src/shared/FrameSet.h"
#include "thelper.h"
#include <filesystem>

/**
 * @brief create a file. write to it and verify that the data matches the expected lenght.
 *
 * @param file
 * @param outfile
 * @param context
 * @return true
 * @return false
 */
static bool infuse_create(IFile &file, const std::string outfile, const char *context)
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

/**
 * @brief Test the creation of CFileWrap and CFileMem objects
 *
 * @return true
 * @return false
 */
bool test_ifile_create()
{
    constexpr const char *OUTFILE = "tests/out/ifile.dat";

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

/**
 * @brief Test deserialzing an object vis FileMem; serializing this object back to disk
 *        compares the two files
 *
 * @return true
 * @return false
 */
bool test_ifile_read_write()
{
    constexpr const char *IN_FILE = "tests/in/annie.obl";
    constexpr const char *OUT_FILE1 = "tests/out/annie1.obl";
    constexpr const char *OUT_FILE2 = "tests/out/annie2.obl";
    CFileWrap fw;
    if (!fw.open(IN_FILE, "rb"))
    {
        LOGE("can't open for reading %s\n", IN_FILE);
        return false;
    }

    CFrameSet fs;
    if (!fs.extract(fw))
    {
        LOGE("failed to extract images from %s [%s]\n", IN_FILE, fs.getLastError());
        return false;
    }
    size_t imageCount = fs.getSize();

    fw.seek(0);
    int filesize = fw.getSize();
    char *buf = new char[filesize];
    if (fw.read(buf, filesize) != 1)
    {
        LOGE("failed to read %s\n", IN_FILE);
        return false;
    }
    fw.close();

    if (!fw.open(OUT_FILE1, "wb"))
    {
        LOGE("can't open for writing %s\n", OUT_FILE1);
        return false;
    }
    if (!fs.write(fw))
    {
        LOGE("can't serialize CFrameSet Object to %s\n", OUT_FILE1);
        return false;
    }

    const std::string uuid = fs.tag("UUID");

    fw.close();
    fs.clear();

    CFileMem fm;
    fm.replace(buf, filesize);
    delete[] buf;
    if (!fs.extract(fm))
    {
        LOGE("failed to extract images from [memfile] size: %d - %s\n", filesize, fs.getLastError());
        return false;
    }
    fs.setTag("UUID", uuid.c_str());

    if (fs.getSize() != imageCount)
    {
        LOGE("images from [memfile] %ld; expecting %ld\n", fs.getSize(), imageCount);
        return false;
    }

    fm.close();

    if (!fw.open(OUT_FILE2, "wb"))
    {
        LOGE("can't open for writing %s [trace1]\n", OUT_FILE2);
        return false;
    }
    if (!fs.write(fw))
    {
        LOGE("can't serialize CFrameSet Object to %s\n", OUT_FILE2);
        return false;
    }
    fw.close();

    if (getFileSize(OUT_FILE1) != getFileSize(OUT_FILE2))
    {
        LOGE("file size %ld mismatch; expecting: %ld\n",
             getFileSize(OUT_FILE2), getFileSize(OUT_FILE1));
        return false;
    }

    // clean up
    std::filesystem::remove(OUT_FILE1);
    std::filesystem::remove(OUT_FILE2);

    return true;
}

bool test_ifile_serializer(IFile &file, const char *context)
{
    size_t expectedSize = 0;
    size_t expectedPos = 0;
    auto testPtr = [&file, &expectedSize, &expectedPos, context](const char *now)
    {
        if (!file.flush())
        {
            LOGE("flush failed for %s", context);
            return false;
        }
        auto size = file.getSize();
        if (size != (long)expectedSize)
        {
            LOGE("[%s][%s] filesize is %ld; expecting %u ", context, now, size, expectedSize);
            return false;
        }
        auto pos = file.tell();
        if (file.tell() != (long)expectedPos)
        {
            LOGE("[%s][%s] filepos is %ld; expecting %u ", context, now, pos, expectedPos);
            return false;
        }

        return true;
    };

    const std::string STR1("This is a test string.");
    const std::string STR2("Unit Tests are a pain.");
    const std::string STR3("KIWI");
    const char *ALLO = "ALLO";
    const char *TOTO = "TOTO";
    const char *WORLD = "WORLD";

    // write string
    file << WORLD;
    expectedSize += strlen(WORLD) + 1;
    expectedPos += strlen(WORLD) + 1;
    if (!testPtr("<< WORLD"))
        return false;

    // Write std::string <<
    file << STR1;
    expectedSize += STR1.length() + 1;
    expectedPos += STR1.length() + 1;
    if (!testPtr("<< STR1"))
        return false;
    if (!file.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    expectedPos = 0;
    if (!testPtr("seek(0)"))
        return false;

    // Write binary
    if (file.write(ALLO, strlen(ALLO)) != IFILE_OK)
    {
        LOGE("failed to write('ALLO')");
        return false;
    }
    expectedPos += strlen(ALLO);
    if (!testPtr("write('ALLO')"))
        return false;

    // seek(end)
    if (!file.seek(expectedSize))
    {
        LOGE("failed to seek(end)");
        return false;
    }
    expectedPos = expectedSize;
    if (!testPtr("seek(end)"))
        return false;

    // concat string
    const auto here = expectedPos;
    expectedSize += STR2.length();
    expectedPos += STR2.length();
    file += STR2;
    if (!testPtr("+= STR2"))
        return false;

    // rewind fileptr
    if (!file.seek(here))
    {
        LOGE("failed seek(here)");
        return false;
    }
    expectedPos = here + STR3.length();
    file.write(STR3.c_str(), STR3.length()); // write KIWI
    if (!testPtr("+= STR3"))
        return false;

    // seek expectedSize + 10
    // expected to fail
    size_t leap = 10;
    if (!file.seek(expectedSize + leap))
    {
        LOGE("seek(expectedSize + %u) failed", leap);
        return false;
    }
    expectedPos = expectedSize + leap;
    if (!testPtr("seek(expectedSize + 10)"))
        return false;

    if (file.write(TOTO, strlen(TOTO)) != IFILE_OK)
    {
        LOGE("failed to write TOTO");
        return false;
    }
    expectedPos += strlen(TOTO);
    expectedSize = expectedPos;
    if (!testPtr("write TOTO"))
        return false;

    return true;
}

bool test_ifile()
{
    constexpr const char OUT_FILE1[] = "tests/out/fileout1.dat";
    constexpr const char OUT_FILE2[] = "tests/out/fileout2.dat";

    LOGI(">>FileWrap");

    CFileWrap file;
    if (file.open(OUT_FILE1, "wb"))
    {
        if (!test_ifile_serializer(file, "CFileWrap"))
        {
            LOGE("serializer test for CFileWrap failed");
            file.close();
            return false;
        }
    }
    file.close();

    LOGI(">>FileMem");

    CFileMem mem;
    if (mem.open(OUT_FILE1, "wb"))
    {
        if (!test_ifile_serializer(mem, "CFileMem"))
        {
            LOGE("serializer test for CFileMem failed");
            mem.close();
            return false;
        }
    }

    FILE *tfile2 = fopen(OUT_FILE2, "wb");
    if (!tfile2)
    {
        LOGE("failed to create %s", OUT_FILE2);
        return false;
    }

    if (fwrite(mem.buffer(), mem.getSize(), 1, tfile2) != 1)
    {
        LOGE("failed to write %s", OUT_FILE2);
        return false;
    }
    if (fclose(tfile2) != 0)
    {
        LOGE("failed to close %s", OUT_FILE2);
        return false;
    }
    mem.close();

    if (!compareFiles(OUT_FILE1, OUT_FILE2))
        return false;

    // clean up
    std::filesystem::remove(OUT_FILE1);
    std::filesystem::remove(OUT_FILE2);
    return true;
}