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
#include <memory>
#include <cassert>
#include "thelper.h"
#include "t_maparch.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include "../src/strhelper.h"

constexpr uint16_t KEY1 = 0x1990;
constexpr uint16_t KEY2 = 0x1990;
constexpr uint16_t VAL1 = 0x0522;
constexpr const char *VAL2 = "Beginning of the End";
constexpr const char *IN_FILE = "tests/in/levels1.mapz";
constexpr const char *OUT_FILE1 = "tests/out/levels1.mapz";
constexpr const char *OUT_FILE2 = "tests/out/levels2.mapz";
constexpr const char *OUT_ARCH_TEST2 = "tests/out/arch%ld.mapz";

void injectStates(CMapArch &arch)
{
    CMap *map = arch.at(0);
    CStates &states = map->states();
    states.setU(KEY1, VAL1);
    states.setS(KEY2, VAL2);
}

bool checkInjectedStates(CMapArch &arch, const char *context)
{
    CMap *map = arch.at(0);
    auto &states = map->states();
    if (states.getU(KEY1) != VAL1)
    {
        LOGE("missing injected state. %s\n", context);
        return false;
    }

    if (std::string(states.getS(KEY2)) != VAL2)
    {
        LOGE("missing injected state. %s\n", context);
        return false;
    }
    return true;
}

bool test_maparch_1()
{
    CMapArch arch1;
    arch1.read(IN_FILE);
    injectStates(arch1);
    if (!checkInjectedStates(arch1, "initial injection"))
    {
        return false;
    }

    // test writing: method 1
    arch1.write(OUT_FILE1);

    // test reading: method 1
    CMapArch arch2;
    arch2.read(OUT_FILE1);
    if (arch1.size() != arch2.size())
    {
        LOGE("map count mismatch in arch after write\n");
        return false;
    }

    // test reading: method 2
    CMapArch arch3;
    CFileWrap file;
    file.open(OUT_FILE1);
    arch3.read(file);
    file.close();

    // test writing: method 2
    arch3.write(OUT_FILE2);
    if (getFileSize(OUT_FILE1) != getFileSize(OUT_FILE2))
    {
        LOGE("different disk size for identical content\n");
        return false;
    }

    // check persistence after reload
    if (!checkInjectedStates(arch3, "data persistence"))
    {
        return false;
    }

    // clean up
    std::filesystem::remove(OUT_FILE1);
    std::filesystem::remove(OUT_FILE2);
    return true;
}

bool test_maparch_2()
{
    CMapArch arch1;
    if (!arch1.read(IN_FILE))
    {
        LOGE("can't open %s; last error: %s\n", IN_FILE, arch1.lastError());
        return false;
    }
    size_t size_maparch1 = arch1.size();
    std::unique_ptr<CMap> map(new CMap(16, 16));
    CMap *tmap = map.get();
    if (arch1.add(std::move(map)) != size_maparch1)
    {
        LOGE("map not inserted\n");
        return false;
    }

    if (arch1.at(size_maparch1) != tmap)
    {
        LOGE("map not inserted at the end\n");
        return false;
    };

    map = arch1.removeAt(size_maparch1);
    if (map.get() != tmap)
    {
        LOGE("last map not what is expected\n");
        return false;
    }

    if (arch1.size() != size_maparch1)
    {
        LOGE("maparch size is not what is expected\n");
        return false;
    }

    CMapArch arch2;
    if (!arch2.extract(IN_FILE))
    {
        LOGE("can't extact from %s; last error: %s\n", IN_FILE, arch2.lastError());
        return false;
    }

    if (arch1.size() != arch2.size())
    {
        LOGE("maparch1 and maparch2 don't have same map count\n");
        return false;
    }

    CFileWrap file;
    if (!file.open(IN_FILE))
    {
        LOGE("can't open %s", IN_FILE);
        return false;
    }
    CMapArch arch3;
    if (!arch3.read(file))
    {
        LOGE("can't read from IFile for %s; last error: %s\n", IN_FILE, arch3.lastError());
        return false;
    }
    file.seek(0);

    if (arch2.size() != arch3.size())
    {
        file.close();
        LOGE("maparch2 and maparch3 don't have same map count\n");
        return false;
    }

    int size = file.getSize();
    char *buf = new char[size];
    if (file.read(buf, size) != IFILE_OK)
    {
        file.close();
        LOGE("read error for %s\n", IN_FILE);
        return false;
    }
    file.close();

    CMapArch arch4;
    if (!arch4.fromMemory(reinterpret_cast<uint8_t *>(buf)))
    {
        LOGE("maparch fromMemory on data from IFile failed for %s; last error: %s\n", IN_FILE, arch4.lastError());
        return false;
    }
    delete[] buf;

    std::vector<CMapArch *> archs{&arch1, &arch2, &arch3, &arch4};
    std::vector<size_t> archFileSizes;
    for (size_t i = 0; i < archs.size(); ++i)
    {
        CMapArch *arch = archs[i];
        char fpath[128];
        snprintf(fpath, sizeof(fpath), OUT_ARCH_TEST2, i);
        if (!arch->write(fpath))
        {
            LOGE("write error for %s; last error: %s\n", fpath, arch->lastError());
            return false;
        }
        CFileWrap file;
        if (!file.open(fpath))
        {
            LOGE("can't open %s", fpath);
            return false;
        }
        archFileSizes.push_back(file.getSize());
        file.close();
        if (i > 0 && archFileSizes[i] != archFileSizes[i - 1])
        {
            LOGE("filesize for file%ld and file%ld don't match\n", i, i - 1);
            return false;
        }
    }

    std::unique_ptr<CMap> map2(new CMap(16, 16));
    CMap *tmap2 = map2.get();
    arch1.insertAt(0, std::move(map2));
    if (arch1.size() != size_maparch1 + 1)
    {
        LOGE("map not inserted sucessfully\n");
        return false;
    }
    if (arch1.at(0) != tmap2)
    {
        LOGE("map inserted not found at the start\n");
        return false;
    }

    // clean up
    for (size_t i = 0; i < archs.size(); ++i)
    {
        char fpath[128];
        snprintf(fpath, sizeof(fpath), OUT_ARCH_TEST2, i);
        std::filesystem::remove(fpath);
    }

    return true;
}

bool test_maparch_3()
{
    CFileWrap file;
    if (!file.open(IN_FILE))
    {
        LOGE("can't open %s", IN_FILE);
        return false;
    }
    int size = file.getSize();
    char *buf = new char[size];
    if (file.read(buf, size) != IFILE_OK)
    {
        file.close();
        LOGE("read error for %s\n", IN_FILE);
        return false;
    }
    file.close();

    IndexVector index1;
    if (!CMapArch::indexFromFile(IN_FILE, index1))
    {
        LOGE("failed to get index from file %s\n", IN_FILE);
        return false;
    }

    IndexVector index2;
    if (!CMapArch::indexFromMemory(reinterpret_cast<uint8_t *>(buf), index2))
    {
        LOGE("failed to get index from memory\n");
        return false;
    }
    if (index1.size() != index2.size())
    {
        LOGE("index1 size [%ld] doesn't match index2 [%ld\n", index1.size(), index2.size());
        return false;
    }

    CMap *map = new CMap();
    for (size_t i = 0; i < index1.size(); ++i)
    {
        if (index1.at(i) != index2.at(i))
        {
            LOGE("index1 [0x%.lx] at position %ld mismatch w/ index2 [0x%.lx]\n", index1.at(i), i, index2.at(i));
            return false;
        }
        if (!map->fromMemory(reinterpret_cast<uint8_t *>(buf) + index1.at(i)))
        {
            LOGE("failed to serialize map at offset 0x%.lx from archfile %s\n", index1.at(i), IN_FILE);
            return false;
        }
    }
    delete map;
    delete[] buf;
    return true;
}

void testIndexFromMemory()
{
    // Create a test MAAZ buffer
    CMapArch arch;
    auto map1 = std::make_unique<CMap>(2, 2, 1);
    auto map2 = std::make_unique<CMap>(3, 3, 2);
    arch.add(std::move(map1));
    arch.add(std::move(map2));
    CFileWrap file;
    file.open("test.maaz", "wb");
    // arch.write(file);
    file.close();

    // Read buffer
    auto data = readFile("test.maaz"); // From level.cpp
    IndexVector index;
    assert(CMapArch::indexFromMemory(data.data(), index));
    assert(index.size() == 2);
    // assert(index[0] > sizeof(MAAZ_SIG) + sizeof(uint16_t) * 2 + sizeof(uint32_t));
    assert(index[1] > index[0]);
    LOGI("Offsets: %ld, %ld\n", index[0], index[1]);
}