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
#include "../../src/shared/Frame.h"
#include "../../src/shared/FrameSet.h"
#include "../../src/shared/FileWrap.h"
#include "../../src/shared/FileMem.h"
#include "../../src/shared/ss_limits.h"
#include "../../src/logger.h"
#include "thelper.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <zlib.h>
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
        LOGE("can't read %s", filepath);
        return false;
    }

    if (!fs.extract(file))
    {
        LOGE("can't extract from %s: %s", filepath, fs.getLastError());
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
        LOGE("can't create %s", filepath);
        return false;
    }

    if (format == PNG)
    {
        std::vector<uint8_t> png;
        if (!fs.toPng(png))
        {
            LOGE("can't convert to png");
            return false;
        }

        if (file.write(png.data(), png.size()) != IFILE_OK)
        {
            LOGE("fail to write to %s", filepath);
            return false;
        }
    }
    else if (format == OBL5_SOLID)
    {
        if (!fs.write(file, CFrameSet::OBL5_SOLID))
        {
            LOGE("fail to write to %s", filepath);
            return false;
        }
    }
    else if (format == OBL5_UNPACKED)
    {
        if (!fs.write(file, CFrameSet::OBL5_UNPACKED))
        {
            LOGE("fail to write to %s", filepath);
            return false;
        }
    }
    else
    {
        LOGE("unknown format: %d", format);
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
        LOGE("failed to write %s", out_path);
        return false;
    }

    // read back file
    CFrameSet fs2;
    if (!read_data_in(fs2, out_path))
    {
        LOGE("failed to read %s", out_path);
        return false;
    }

    // writeback file
    if (!write_data_out(fs2, out_path2, PNG))
    {
        LOGE("failed to write %s", out_path2);
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
        LOGE("failed to read %s", source);
        return false;
    }

    // plain reference png
    char out_path[256];
    char out_path2[256];
    snprintf(out_path, sizeof(out_path), OUT_PATH, name, ".png");
    if (!write_data_out(fs, out_path, PNG))
    {
        LOGE("failed to write %s", out_path);
        return false;
    }
    refFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path);

    snprintf(out_path, sizeof(out_path), OUT_PATH, name, "-obl5.obl");
    snprintf(out_path2, sizeof(out_path2), OUT_PATH, name, "-obl5.png");

    if (!performDoubleBlind(fs, OBL5_UNPACKED, out_path, out_path2))
    {
        LOGE("doubleblind `%s` `%s` failed", out_path, out_path2);
        return false;
    }
    listOutFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path2);
    refFiles.emplace_back(out_path2);

    snprintf(out_path, sizeof(out_path), OUT_PATH, name, "-obl5s.obl");
    snprintf(out_path2, sizeof(out_path2), OUT_PATH, name, "-obl5s.png");
    if (!performDoubleBlind(fs, OBL5_SOLID, out_path, out_path2))
    {
        LOGE("doubleblind `%s` `%s` failed", out_path, out_path2);
        return false;
    }
    listOutFiles.emplace_back(out_path);
    listOutFiles.emplace_back(out_path2);
    refFiles.emplace_back(out_path2);

    if (!compareFiles(refFiles[0].c_str(), refFiles[1].c_str()))
    {
        LOGE("compareFiles `%s` `%s` failed", refFiles[0].c_str(), refFiles[1].c_str());
        return false;
    }

    if (!compareFiles(refFiles[0].c_str(), refFiles[2].c_str()))
    {
        LOGE("compareFiles `%s` `%s` failed", refFiles[0].c_str(), refFiles[1].c_str());
        return false;
    }

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
}

bool test_frameset()
{
    return test_frameset_serializer();
}

bool test_frameset_families()
{
    // every synthetic container in tests/in must extract cleanly
    for (const char *dir : {"tests/in/ima", "tests/in/imc1", "tests/in/obl3",
                            "tests/in/obl4", "tests/in/obl5", "tests/in/mcx"})
    {
        for (const auto &entry : std::filesystem::directory_iterator(dir))
        {
            CFrameSet fs;
            if (!read_data_in(fs, entry.path().string().c_str()))
            {
                LOGE("family extract failed: %s [%s]", entry.path().string().c_str(),
                     fs.getLastError());
                return false;
            }
            if (fs.size() == 0)
            {
                LOGE("family %s: no frames from %s", dir,
                     entry.path().string().c_str());
                return false;
            }
            if (fs.getLastError()[0] != '\0')
            {
                LOGE("family %s: error set after success: %s", dir,
                     fs.getLastError());
                return false;
            }
            LOGI("family %s: %s -> %zu frames", dir,
                 entry.path().filename().string().c_str(), fs.size());
        }
    }
    return true;
}

bool test_frameset_reject()
{
    // unknown signatures fall through to the IMA importer, which must fail
    // with a non-empty error
    constexpr const char *PATH = "tests/out/badsig.bin";
    {
        FILE *f = fopen(PATH, "wb");
        if (!f)
        {
            LOGE("cannot create %s", PATH);
            return false;
        }
        const char junk[] = "ZZZZ not any known image format 1234";
        fwrite(junk, 1, sizeof(junk), f);
        fclose(f);
    }
    CFileWrap file;
    if (!file.open(PATH, "rb"))
    {
        LOGE("cannot reopen %s", PATH);
        return false;
    }
    CFrameSet fs;
    const bool ok = fs.extract(file);
    file.close();
    if (ok || fs.getLastError()[0] == '\0')
    {
        LOGE("garbage file: extract %d error '%s'", ok, fs.getLastError());
        return false;
    }
    LOGI("reject message: %s", fs.getLastError());

    // typed read() rejects a foreign signature with a non-empty error
    if (!file.open(PATH, "rb"))
    {
        LOGE("cannot reopen %s for read()", PATH);
        return false;
    }
    CFrameSet fs2;
    const bool readOk = fs2.read(file);
    file.close();
    if (readOk || fs2.getLastError()[0] == '\0')
    {
        LOGE("read() on garbage: result %d error '%s'", readOk,
             fs2.getLastError());
        return false;
    }

    // empty file: extract fails while reading the header
    constexpr const char *EMPTY = "tests/out/empty.bin";
    {
        FILE *f = fopen(EMPTY, "wb");
        if (!f)
        {
            LOGE("cannot create %s", EMPTY);
            return false;
        }
        fclose(f);
    }
    if (!file.open(EMPTY, "rb"))
    {
        LOGE("cannot reopen %s", EMPTY);
        return false;
    }
    CFrameSet fs3;
    const bool emptyOk = fs3.extract(file);
    file.close();
    if (emptyOk || fs3.getLastError()[0] == '\0')
    {
        LOGE("empty extract: result %d error '%s'", emptyOk,
             fs3.getLastError());
        return false;
    }

    std::filesystem::remove(PATH);
    std::filesystem::remove(EMPTY);
    return true;
}

bool test_frameset_container()
{
    CFrameSet fs;
    CFrame *a = new CFrame(4, 4);
    CFrame *b = new CFrame(8, 8);
    if (fs.add(a) != 0 || fs.add(b) != 1 || fs.size() != 2)
    {
        LOGE("add() return/size wrong");
        return false;
    }
    const int idx = ++fs; // appends an empty frame
    if (idx != 2 || fs.size() != 3 || fs[2]->width() != 0)
    {
        LOGE("operator++ mismatch (idx %d)", idx);
        return false;
    }
    // operator-- deletes the last frame (leak fixed; see BUG on one-past-the-end delete)
    CFrameSet emptySet;
    if (--emptySet != 0)
    {
        LOGE("operator-- on empty set not 0");
        return false;
    }
    CFrameSet decSet;
    CFrame *keep = new CFrame(1, 1);
    decSet.add(keep);
    decSet.add(new CFrame(2, 2));
    if (--decSet != 0 || decSet.size() != 1 || decSet[0] != keep)
    {
        LOGE("operator-- mismatch");
        return false;
    }

    if (fs[-1] != nullptr || fs[3] != nullptr || fs[0] != a)
    {
        LOGE("operator[] bounds/identity wrong");
        return false;
    }

    CFrame *c = new CFrame(2, 2);
    fs.insertAt(1, c);
    if (fs[1] != c || fs[2] != b)
    {
        LOGE("insertAt identity wrong");
        return false;
    }
    fs.move(2, 0); // b to front: [a c b e] -> [b a c e]
    if (fs[0] != b || fs[1] != a || fs[2] != c)
    {
        LOGE("move identity wrong");
        return false;
    }
    CFrame *gone = fs.removeAt(3); // the ++-appended empty frame
    if (gone == nullptr || fs.size() != 3)
    {
        LOGE("removeAt mismatch");
        return false;
    }
    delete gone;

    // set() keeps ownership in the set: the replaced frame is deleted
    // (leak-review fix 2026-11-18)
    CFrame *d = new CFrame(1, 1);
    fs.set(1, d);
    if (fs[1] != d)
    {
        LOGE("set() mismatch");
        return false;
    }

    // name + tags
    fs.setName("myset");
    if (std::string(fs.getName()) != "myset")
    {
        LOGE("setName mismatch");
        return false;
    }
    fs.setTag("Style", "cartoon");
    if (fs.tag("Style") != "cartoon")
    {
        LOGE("setTag/tag mismatch");
        return false;
    }
    fs.assignNewUUID();
    const std::string uuid = fs.tag("UUID");
    if (uuid.size() != 36)
    {
        LOGE("assignNewUUID gave '%s'", uuid.c_str());
        return false;
    }
    fs.assignNewUUID();
    if (fs.tag("UUID") == uuid)
    {
        LOGE("assignNewUUID did not refresh");
        return false;
    }

    // toSubset: deep copies [1..2]
    CFrameSet sub;
    fs.toSubset(sub, 1, 2);
    if (sub.size() != 2 || sub[0] == fs[1] || sub[0]->width() != fs[1]->width())
    {
        LOGE("toSubset mismatch");
        return false;
    }

    // assignment: deep copy of frames + tags + name
    CFrameSet dup;
    dup = fs;
    if (dup.size() != fs.size() || dup[0] == fs[0] ||
        std::string(dup.getName()) != "myset" || dup.tag("Style") != "cartoon")
    {
        LOGE("operator= mismatch");
        return false;
    }

    // copy ctor: deep copy; destroying the copy leaves the original intact
    const size_t n = fs.size();
    {
        CFrameSet copy(fs);
        if (copy.size() != n || copy[0] == fs[0] ||
            std::string(copy.getName()) != "myset")
        {
            LOGE("copy ctor mismatch");
            return false;
        }
    }
    if (fs.size() != n || fs[0] == nullptr)
    {
        LOGE("copy ctor dtor damaged original");
        return false;
    }

    // self-assignment must not empty the set
    fs = fs;
    if (fs.size() != n || std::string(fs.getName()) != "myset")
    {
        LOGE("self-assignment emptied the set");
        return false;
    }

    // resize up: new slots are null; clear() deletes them safely
    dup.resize(fs.size() + 2);
    if (dup.size() != fs.size() + 2 || dup[fs.size()] != nullptr)
    {
        LOGE("resize up mismatch");
        return false;
    }
    // resize down: the dropped tail is deleted by the set (leak fix)
    dup.resize(1);
    if (dup.size() != 1 || dup[0] == nullptr)
    {
        LOGE("resize down mismatch");
        return false;
    }
    dup.clear();
    if (dup.size() != 0 || dup.tag("UUID").size() != 0)
    {
        // clear() also drops tags; tag() creates an empty entry on lookup
        LOGE("clear() left frames or re-created tag");
        return false;
    }

    // removeAll detaches without deleting: take ownership back first
    while (fs.size())
    {
        delete fs.removeAt(0);
    }
    if (!fs.frames().empty())
    {
        LOGE("drain mismatch");
        return false;
    }

    // setCurrFrame does NOT clamp (finding vs plan)
    fs.setCurrFrame(999);
    if (fs.currFrame() != 999)
    {
        LOGE("setCurrFrame unexpectedly clamped");
        return false;
    }
    return true;
}

/**
 * @brief BUG-C4: the IMC1 RLE decoder must bound escape sequences against
 *        the source and run lengths against the destination buffer.
 *
 * @return true
 * @return false
 */
bool test_frameset_hostile_imc1()
{
    // header: "IMC1" + len(1) + hei(1) -> destSize = 1*1*8*8 = 64
    auto buildImc1 = [](const std::vector<uint8_t> &payload)
    {
        std::vector<uint8_t> v = {'I', 'M', 'C', '1', 1, 1};
        const int32_t sizeData = static_cast<int32_t>(payload.size());
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&sizeData);
        v.insert(v.end(), p, p + 4);
        v.insert(v.end(), payload.begin(), payload.end());
        return v;
    };
    auto must_fail = [&buildImc1](const std::vector<uint8_t> &payload, const char *what)
    {
        const std::vector<uint8_t> data = buildImc1(payload);
        CFileMem mem;
        mem.replace(data.data(), data.size());
        CFrameSet fs;
        if (fs.extract(mem) || std::string(fs.getLastError()).empty())
        {
            LOGE("imc1: %s accepted [%s]", what, fs.getLastError());
            return false;
        }
        return true;
    };

    // run of 255 overflows the 64-byte destination (pre-fix heap overrun)
    if (!must_fail({0x00, 0xff, 0x41, 0xff, 0x00}, "run overrun"))
        return false;
    // escape truncated at end of source (pre-fix read past the source)
    if (!must_fail({0xff, 0x41, 0x00}, "truncated escape"))
        return false;
    // trailing literals past a full destination are padding (compat with
    // real IMC1 files, which the old one-byte-short decoder also discarded)
    {
        const std::vector<uint8_t> data = buildImc1(std::vector<uint8_t>(70, 0x33));
        CFileMem mem;
        mem.replace(data.data(), data.size());
        CFrameSet fs;
        if (!fs.extract(mem) || fs.size() != 1)
        {
            LOGE("imc1: literal padding rejected [%s]", fs.getLastError());
            return false;
        }
    }

    // a well-formed stream (single run exactly filling dest) still imports
    const std::vector<uint8_t> ok_data = buildImc1({0xff, 0x42, 64, 0x00});
    CFileMem mem;
    mem.replace(ok_data.data(), ok_data.size());
    CFrameSet fs;
    if (!fs.extract(mem) || fs.size() != 1)
    {
        LOGE("imc1: well-formed stream rejected [%s]", fs.getLastError());
        return false;
    }
    return true;
}
/**
 * @brief readSolid must reject a valid zlib stream that decompresses shorter
 *        than declared dimensions (zero-padded frames), and writeSolid must
 *        refuse a tag count readSolid would reject.
 */
bool test_frameset_hostile_solid()
{
    constexpr const char *PATH = "tests/out/hostile_solid.obl";
    CFrameSet fs;
    fs.add(new CFrame(8, 8)); // reader computes totalSize = 8*8*4 = 256
    if (!write_data_out(fs, PATH, OBL5_SOLID))
    {
        LOGE("hostile_solid: write baseline failed");
        return false;
    }

    // splice in a valid zlib stream that decodes to 16 bytes (< 256)
    {
        FILE *f = fopen(PATH, "rb+");
        if (!f)
        {
            LOGE("hostile_solid: cannot reopen %s", PATH);
            return false;
        }
        fseek(f, 0, SEEK_END);
        const long fileSize = ftell(f);
        std::vector<uint8_t> bytes(fileSize);
        fseek(f, 0, SEEK_SET);
        if (fread(bytes.data(), 1, fileSize, f) != static_cast<size_t>(fileSize))
        {
            LOGE("hostile_solid: short read");
            fclose(f);
            return false;
        }
        const uint32_t count = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) |
                               (static_cast<uint32_t>(bytes[7]) << 24);
        const size_t blobOff = 16 + 4 * count; // sig+count+format+srcSize, w/h pairs

        const std::vector<uint8_t> tiny(16, 0); // decodes to 16 != totalSize
        std::vector<uint8_t> z(compressBound(tiny.size()));
        uLongf zlen = z.size();
        if (compress2(z.data(), &zlen, tiny.data(), tiny.size(),
                      Z_DEFAULT_COMPRESSION) != Z_OK)
        {
            LOGE("hostile_solid: compress2 failed");
            fclose(f);
            return false;
        }
        const uint32_t newSrc = static_cast<uint32_t>(zlen);
        for (int b = 0; b < 4; ++b)
            bytes[12 + b] = static_cast<uint8_t>((newSrc >> (8 * b)) & 0xff);
        bytes.resize(blobOff + zlen);
        memcpy(bytes.data() + blobOff, z.data(), zlen);

        truncate(PATH, static_cast<off_t>(bytes.size()));
        fseek(f, 0, SEEK_SET);
        if (fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size())
        {
            LOGE("hostile_solid: short write");
            fclose(f);
            return false;
        }
        fclose(f);
    }

    CFileWrap file;
    if (!file.open(PATH, "rb"))
    {
        LOGE("hostile_solid: cannot open forged file");
        return false;
    }
    CFrameSet forged;
    const bool ok = forged.read(file);
    file.close();
    if (ok || std::string(forged.getLastError()).find("size mismatch") == std::string::npos)
    {
        LOGE("hostile_solid: short decompression accepted [%d: %s]", ok,
             forged.getLastError());
        return false;
    }

    // writeSolid must refuse more serializable tags than readSolid accepts
    CFrameSet manyTags;
    manyTags.add(new CFrame(8, 8));
    for (int i = 0; i <= TAG_COUNT_MAX; ++i)
    {
        manyTags.setTag(("key" + std::to_string(i)).c_str(), "v");
    }
    CFileWrap out;
    if (!out.open("tests/out/manytags.obl", "wb"))
    {
        LOGE("hostile_solid: cannot create manytags file");
        return false;
    }
    if (manyTags.write(out, CFrameSet::OBL5_SOLID) ||
        std::string(manyTags.getLastError()).find("too many") == std::string::npos)
    {
        LOGE("hostile_solid: %d tags accepted for write [%s]", TAG_COUNT_MAX + 1,
             manyTags.getLastError());
        out.close();
        return false;
    }
    out.close();

    std::filesystem::remove(PATH);
    std::filesystem::remove("tests/out/manytags.obl");
    return true;
}
