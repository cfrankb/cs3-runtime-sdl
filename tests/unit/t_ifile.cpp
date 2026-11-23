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
#include "../../src/maparch.h"
#include "../../src/map.h"
#include "../../src/states.h"
#include "../../src/shared/FileWrap.h"
#include "../../src/shared/FileMem.h"
#include "../../src/shared/IFile.h"
#include "../../src/shared/ISerial.h"
#include "../../src/game_auto.h"
#include "../../src/shared/helper.h"
#include "../../src/logger.h"
#include "../../src/shared/FrameSet.h"
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
        LOGE("[%s] fail to create %s", context, outfile.c_str());
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
        LOGE("filesize %ld wrong; expecting %d", file.getSize(), filesize);
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
    LOGI("tell: %lu", fm.tell());
    LOGI("getSize: %lu", fm.getSize());
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
        LOGE("can't open for reading %s", IN_FILE);
        return false;
    }

    CFrameSet fs;
    if (!fs.extract(fw))
    {
        LOGE("failed to extract images from %s [%s]", IN_FILE, fs.getLastError());
        return false;
    }
    size_t imageCount = fs.getSize();

    fw.seek(0);
    int filesize = fw.getSize();
    uint8_t *buf = new uint8_t[filesize];
    if (fw.read(buf, filesize) != 1)
    {
        LOGE("failed to read %s", IN_FILE);
        return false;
    }
    fw.close();

    if (!fw.open(OUT_FILE1, "wb"))
    {
        LOGE("can't open for writing %s", OUT_FILE1);
        return false;
    }
    if (!fs.write(fw))
    {
        LOGE("can't serialize CFrameSet Object to %s", OUT_FILE1);
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
        LOGE("failed to extract images from [memfile] size: %d - %s", filesize, fs.getLastError());
        return false;
    }
    fs.setTag("UUID", uuid.c_str());

    if (fs.getSize() != imageCount)
    {
        LOGE("images from [memfile] %ld; expecting %ld", fs.getSize(), imageCount);
        return false;
    }

    fm.close();

    if (!fw.open(OUT_FILE2, "wb"))
    {
        LOGE("can't open for writing %s [trace1]", OUT_FILE2);
        return false;
    }
    if (!fs.write(fw))
    {
        LOGE("can't serialize CFrameSet Object to %s", OUT_FILE2);
        return false;
    }
    fw.close();

    if (getFileSize(OUT_FILE1) != getFileSize(OUT_FILE2))
    {
        LOGE("file size %ld mismatch; expecting: %ld",
             getFileSize(OUT_FILE2), getFileSize(OUT_FILE1));
        return false;
    }

    // clean up
    std::filesystem::remove(OUT_FILE1);
    std::filesystem::remove(OUT_FILE2);

    return true;
}

/**
 * @brief Test the generic std::vector<T> serializer (readVector/writeVector)
 *        round-tripping a vector of spawn_t through an IFile stream.
 *
 * @return true
 * @return false
 */
bool test_ifile_vector()
{
    const std::vector<spawn_t> in = {
        {1, 10, 20, 100, 0xc0, 1},
        {2, -5, 3, 0, 0xc0, 1},
        {65535, 32767, -32768, 65534, 0xc0, 1},
    };
    // 4-byte count + 3 * 11-byte serialized elements
    // (spawn_t serializes field-by-field: 5 * uint16 + uint8; sizeof(spawn_t) is 12 with padding)
    const long expectedSize = sizeof(uint32_t) +
                              static_cast<long>(in.size()) * (5 * sizeof(uint16_t) + sizeof(uint8_t));

    CFileMem mem;
    if (!mem.open("tests/out/vector.dat", "rw+"))
    {
        LOGE("can't open for writing vector.dat");
        return false;
    }
    if (!writeVector(OBJECT_NAME(in), in, mem))
    {
        LOGE("writeVector(spawn_t) failed");
        mem.close();
        return false;
    }
    if (mem.getSize() != expectedSize)
    {
        LOGE("vector size %ld; expecting %ld", mem.getSize(), expectedSize);
        mem.close();
        return false;
    }
    if (!mem.seek(0))
    {
        LOGE("seek(0) failed");
        mem.close();
        return false;
    }

    std::vector<spawn_t> out;
    if (!readVector(OBJECT_NAME(out), out, mem))
    {
        LOGE("readVector(spawn_t) failed");
        mem.close();
        return false;
    }
    mem.close();

    if (out.size() != in.size())
    {
        LOGE("vector count %zu; expecting %zu", out.size(), in.size());
        return false;
    }
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (out[i].tileID != in[i].tileID || out[i].x != in[i].x ||
            out[i].y != in[i].y || out[i].delay != in[i].delay)
        {
            LOGE("vector[%zu] mismatch", i);
            return false;
        }
    }

    std::filesystem::remove("tests/out/vector.dat");
    return true;
}

/**
 * @brief BUG-C1: CFileMem string reader must reject stream-declared lengths
 *        that exceed the buffer instead of reading past it.
 *
 * @return true
 * @return false
 */
bool test_ifile_string_bounds()
{
    auto must_fail = [](const std::vector<uint8_t> &buf, const char *what)
    {
        CFileMem fm;
        fm.replace(buf.data(), buf.size());
        std::string str("untouched");
        if (fm >> str)
        {
            LOGE("[%s] expected failure, got success", what);
            return false;
        }
        if (str != "untouched")
        {
            LOGE("[%s] str modified on failure path", what);
            return false;
        }
        return true;
    };

    // 1-byte header claims 16 bytes of payload, none present
    if (!must_fail({0x10}, "short header claims payload"))
        return false;
    // 0xffff 16-bit extension claims 32767 bytes, none present
    if (!must_fail({0xff, 0xff, 0x7f}, "16-bit length overrun"))
        return false;
    // 0xffff 0xffff 32-bit extension claims 256 bytes, none present
    if (!must_fail({0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x00, 0x00}, "32-bit length overrun"))
        return false;
    // 3-byte tail: 32-bit extension would read 1 byte past the buffer
    if (!must_fail({0xff, 0xff, 0xff}, "truncated 32-bit extension"))
        return false;
    // header present but payload truncated
    if (!must_fail({0x04, 'a', 'b'}, "truncated payload"))
        return false;

    // valid data must still round-trip after the fix
    CFileMem fm;
    if (!fm.open("tests/out/stringbounds.dat", "wb"))
    {
        LOGE("can't open stringbounds.dat");
        return false;
    }
    const std::string text("The quick brown fox.");
    fm << std::string_view(text);
    const size_t encodedSize = fm.getSize();
    if (!fm.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    std::string back;
    if (!(fm >> back))
    {
        LOGE("valid string read failed");
        return false;
    }
    if (back != text)
    {
        LOGE("round-trip mismatch '%s'", back.c_str());
        return false;
    }
    // buffer exhausted: reading one more string must fail, not over-read
    fm.seek(static_cast<long>(encodedSize));
    if (fm >> back)
    {
        LOGE("expected failure reading past end of buffer");
        return false;
    }
    fm.close();
    return true;
}

/**
 * @brief BUG-C2: CFileMem transfers must reject negative sizes instead of
 *        letting them wrap into huge size_t memcpy counts.
 *
 * @return true
 * @return false
 */
bool test_ifile_negative_size()
{
    char scratch[16] = {0};

    CFileMem fm;
    if (!fm.open("tests/out/negsize.dat", "rw"))
    {
        LOGE("can't open negsize.dat");
        return false;
    }
    // populated buffer with a nonzero position: the write-wrap OOB path
    if (fm.write("seed", 4) != IFILE_OK)
    {
        LOGE("seed write failed");
        return false;
    }
    const long sizeBefore = fm.getSize();
    const long posBefore = fm.tell();
    if (fm.write(scratch, -1) != IFILE_NOT_OK)
    {
        LOGE("write(buf, -1) accepted");
        return false;
    }
    if (fm.getSize() != sizeBefore || fm.tell() != posBefore)
    {
        LOGE("negative write altered the buffer");
        return false;
    }
    if (fm.read(scratch, -1) != IFILE_NOT_OK)
    {
        LOGE("read(buf, -1) accepted");
        return false;
    }
    if (fm.read(scratch, -1000000) != IFILE_NOT_OK)
    {
        LOGE("read(buf, -1000000) accepted");
        return false;
    }
    // valid transfers must still work
    if (fm.write("ok", 2) != IFILE_OK)
    {
        LOGE("write after rejected negatives failed");
        return false;
    }
    if (!fm.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    char buf[8] = {0};
    if (fm.read(buf, 6) != IFILE_OK)
    {
        LOGE("read of valid payload failed");
        return false;
    }
    if (std::string(buf, 6) != "seedok")
    {
        LOGE("payload mismatch: %s", buf);
        return false;
    }
    fm.close();
    return true;
}

/**
 * @brief BUG-C10: the disk backend must decode all three string length tiers
 *        (raw byte <0xff, 0xff+uint16, 0xff+0xffff+uint32) and emit an
 *        encoding byte-identical to CFileMem's.
 *
 * @return true
 * @return false
 */
bool test_ifile_string_tiers()
{
    const std::vector<std::string> samples = {
        std::string(0, 'e'),      // zero length: << must be a success no-op (BUG-H8)
        std::string(5, 'a'),      // byte tier
        std::string(300, 'b'),    // 16-bit tier
        std::string(65535, 'c'),  // 32-bit tier boundary
        std::string(100000, 'd'), // 32-bit tier
    };

    CFileMem mem;
    if (!mem.open("tests/out/strtiers_mem.dat", "rw"))
    {
        LOGE("can't open strtiers_mem.dat");
        return false;
    }
    for (const auto &s : samples)
        if (!(mem << std::string_view(s)))
        {
            LOGE("mem write failed for size %zu", s.size());
            return false;
        }

    CFileWrap disk;
    if (!disk.open("tests/out/strtiers_disk.dat", "w+b"))
    {
        LOGE("can't open strtiers_disk.dat");
        return false;
    }
    for (const auto &s : samples)
        if (!(disk << std::string_view(s)))
        {
            LOGE("disk write failed for size %zu", s.size());
            return false;
        }

    // both backends run the same codec: encodings must match byte for byte
    const auto &memBuf = mem.buffer();
    if (disk.getSize() != static_cast<long>(memBuf.size()))
    {
        LOGE("disk encoding size %ld; expecting %zu", disk.getSize(), memBuf.size());
        return false;
    }
    if (!disk.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    std::vector<uint8_t> diskBuf(memBuf.size());
    if (!memBuf.empty() &&
        disk.read(diskBuf.data(), static_cast<int>(diskBuf.size())) != IFILE_OK)
    {
        LOGE("failed reading back disk encoding");
        return false;
    }
    if (diskBuf != memBuf)
    {
        LOGE("disk and memory encodings differ");
        return false;
    }

    // all samples must round-trip on the disk backend
    if (!disk.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    for (const auto &s : samples)
    {
        std::string back;
        if (!(disk >> back))
        {
            LOGE("disk read failed for size %zu", s.size());
            return false;
        }
        if (back != s)
        {
            LOGE("disk round-trip mismatch at size %zu (got %zu)", s.size(), back.size());
            return false;
        }
    }
    if (disk.tell() != disk.getSize())
    {
        LOGE("disk stream not consumed exactly");
        return false;
    }

    // and on the memory backend (the codec reference)
    if (!mem.seek(0))
    {
        LOGE("seek(0) failed");
        return false;
    }
    for (const auto &s : samples)
    {
        std::string back;
        if (!(mem >> back) || back != s)
        {
            LOGE("memory round-trip failed at size %zu", s.size());
            return false;
        }
    }
    disk.close();
    mem.close();
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
            LOGE("[%s][%s] filesize is %ld; expecting %zu ", context, now, size, expectedSize);
            return false;
        }
        auto pos = file.tell();
        if (file.tell() != (long)expectedPos)
        {
            LOGE("[%s][%s] filepos is %ld; expecting %zu ", context, now, pos, expectedPos);
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
        LOGE("seek(expectedSize + %zu) failed", leap);
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

    if (fwrite(mem.buffer().data(), mem.getSize(), 1, tfile2) != 1)
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