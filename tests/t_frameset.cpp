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
#include "../src/shared/Frame.h"
#include "../src/shared/FrameSet.h"
#include "../src/shared/FileWrap.h"
#include "../src/logger.h"
#include "thelper.h"
#include <cstdio>
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
    // NOTE: non-empty operator-- is UB (`delete m_frames[getSize()]` reads
    // one-past-the-end) — only the empty-set guard path is exercised here.
    CFrameSet emptySet;
    if (--emptySet != 0)
    {
        LOGE("operator-- on empty set not 0");
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

    // set() replaces without deleting the old frame; caller owns it
    CFrame *d = new CFrame(1, 1);
    CFrame *old = fs[1];
    fs.set(1, d);
    if (fs[1] != d)
    {
        LOGE("set() mismatch");
        delete d;
        return false;
    }
    delete old;

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

    // resize up: new slots are null; clear() deletes them safely
    dup.resize(fs.size() + 2);
    if (dup.size() != fs.size() + 2 || dup[fs.size()] != nullptr)
    {
        LOGE("resize up mismatch");
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