/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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
#include <cstring>
#include "maparch.h"
#include "map.h"
#include "level.h"
#include "shared/IFile.h"

constexpr const char MAAZ_SIG[]{'M', 'A', 'A', 'Z'};
const uint16_t MAAZ_VERSION = 0;

CMapArch::CMapArch()
{
}

CMapArch::~CMapArch()
{
    clear();
}

/**
 * @brief get the last error
 *
 * @return const char*
 */
const char *CMapArch::lastError()
{
    return m_lastError.c_str();
}

/**
 * @brief get map count
 *
 * @return size_t
 */
size_t CMapArch::size()
{
    return m_maps.size();
}

/**
 * @brief clear the maparch. remove all maps.
 *
 */
void CMapArch::clear()
{
    for (auto i = 0; i < m_maps.size(); ++i)
    {
        if (m_maps[i])
            delete m_maps[i];
    }
    m_maps.clear();
}

/**
 * @brief add new map
 *
 * @param map
 * @return size_t mapIndex pos
 */
size_t CMapArch::add(CMap *map)
{
    m_maps.push_back(map);
    return m_maps.size() - 1;
}

/**
 * @brief remove map at index
 *
 * @param i
 * @return CMap*
 */
CMap *CMapArch::removeAt(int i)
{
    CMap *map = m_maps[i];
    m_maps.erase(m_maps.begin() + i);
    return map;
}

/**
 * @brief insert map at index
 *
 * @param i
 * @param map
 */
void CMapArch::insertAt(int i, CMap *map)
{
    m_maps.insert(m_maps.begin() + i, map);
}

/**
 * @brief get map at index
 *
 * @param i
 * @return CMap*
 */
CMap *CMapArch::at(int i)
{
    return m_maps[i];
}

/**
 * @brief Deserialize the data from IFile Interface object
 *
 * @param file
 * @return true
 * @return false
 */
bool CMapArch::read(IFile &file)
{
    auto readfile = [&file](auto ptr, auto size)
    {
        return file.read(ptr, size);
    };

    typedef struct
    {
        uint8_t sig[sizeof(MAAZ_SIG)];
        uint16_t version;
        uint16_t count;
        uint32_t offset;
    } Header;

    Header hdr;
    // read header
    readfile(&hdr, 12);
    // check signature
    if (memcmp(hdr.sig, MAAZ_SIG, sizeof(MAAZ_SIG)) != 0)
    {
        m_lastError = "MAAZ signature is incorrect";
        return false;
    }
    // check version
    if (hdr.version != MAAZ_VERSION)
    {
        m_lastError = "MAAZ Version is incorrect";
        return false;
    }
    // read index
    file.seek(hdr.offset);
    uint32_t *indexPtr = new uint32_t[hdr.count];
    readfile(indexPtr, sizeof(uint32_t) * hdr.count);
    // read levels
    clear();
    for (int i = 0; i < hdr.count; ++i)
    {
        file.seek(indexPtr[i]);
        CMap *map = new CMap();
        if (!map->read(file))
        {
            return false;
        }
        m_maps.push_back(map);
    }
    delete[] indexPtr;
    return true;
}

/**
 * @brief Deserialize from the standard IO
 *
 * @param filename
 * @return true
 * @return false
 */
bool CMapArch::read(const char *filename)
{
    // read levelArch
    typedef struct
    {
        uint8_t sig[4];
        uint16_t version;
        uint16_t count;
        uint32_t offset;
    } Header;

    FILE *sfile = fopen(filename, "rb");
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    if (sfile)
    {
        Header hdr;
        // read header
        readfile(&hdr, sizeof(Header));
        // check signature
        if (memcmp(hdr.sig, MAAZ_SIG, sizeof(MAAZ_SIG)) != 0)
        {
            m_lastError = "MAAZ signature is incorrect";
            return false;
        }
        // check version
        if (hdr.version != MAAZ_VERSION)
        {
            m_lastError = "MAAZ Version is incorrect";
            return false;
        }
        // read index
        fseek(sfile, hdr.offset, SEEK_SET);
        uint32_t *indexPtr = new uint32_t[hdr.count];
        readfile(indexPtr, sizeof(uint32_t) * hdr.count);
        // read levels
        clear();
        for (int i = 0; i < hdr.count; ++i)
        {
            fseek(sfile, indexPtr[i], SEEK_SET);
            CMap *map = new CMap();
            map->read(sfile);
            m_maps.push_back(map);
        }
        delete[] indexPtr;
        fclose(sfile);
    }
    else
    {
        m_lastError = "can't read file[0]";
    }
    return sfile != nullptr;
}

/**
 * @brief Write file to disk
 *
 * @param filename
 * @return true
 * @return false
 */
bool CMapArch::write(const char *filename)
{
    bool result;
    // write levelArch
    FILE *tfile = fopen(filename, "wb");
    if (tfile)
    {
        std::vector<long> index;
        // write temp header
        fwrite(MAAZ_SIG, sizeof(MAAZ_SIG), 1, tfile);
        fwrite("\0\0\0\0", 4, 1, tfile);
        fwrite("\0\0\0\0", 4, 1, tfile);
        for (auto i = 0; i < m_maps.size(); ++i)
        {
            // write maps
            index.push_back(ftell(tfile));
            m_maps[i]->write(tfile);
        }
        // write index
        long indexPtr = ftell(tfile);
        size_t size = index.size();
        for (unsigned int i = 0; i < index.size(); ++i)
        {
            long ptr = index[i];
            fwrite(&ptr, 4, 1, tfile);
        }
        // write version
        fseek(tfile, 4, SEEK_SET);
        fwrite(&MAAZ_VERSION, 2, 1, tfile);
        // write size
        fseek(tfile, 6, SEEK_SET);
        fwrite(&size, 2, 1, tfile);
        // write indexPtr
        fwrite(&indexPtr, 4, 1, tfile);
        fclose(tfile);
    }
    else
    {
        m_lastError = "can't write file";
    }
    result = tfile != nullptr;
    return result;
}

/**
 * @brief get file signature
 *
 * @return const char*
 */
const char *CMapArch::signature()
{
    return MAAZ_SIG;
}

/**
 * @brief extract map from file. this allows reading maparch, individual map files and legacy files.
 *
 * @param filename
 * @return true
 * @return false
 */
bool CMapArch::extract(const char *filename)
{
    FILE *sfile = fopen(filename, "rb");
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    if (!sfile)
    {
        m_lastError = "can't read header";
        return false;
    }
    char sig[sizeof(MAAZ_SIG)];
    readfile(sig, sizeof(MAAZ_SIG));
    fclose(sfile);

    if (memcmp(sig, MAAZ_SIG, sizeof(MAAZ_SIG)) == 0)
    {
        return read(filename);
    }
    else
    {
        clear();
        m_maps.push_back(new CMap());
        return fetchLevel(*m_maps[0], filename, m_lastError);
    }
}

/**
 * @brief get mapIndex from file. create a vector of map offsets within the file
 *
 * @param filename
 * @param index
 * @return true
 * @return false
 */
bool CMapArch::indexFromFile(const char *filename, IndexVector &index)
{
    typedef struct
    {
        uint8_t sig[sizeof(MAAZ_SIG)];
        uint16_t version;
        uint16_t count;
        uint32_t offset;
    } Header;

    FILE *sfile = fopen(filename, "rb");
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };
    if (sfile)
    {
        Header hdr;
        readfile(&hdr, sizeof(Header));
        // check signature
        if (memcmp(hdr.sig, MAAZ_SIG, sizeof(MAAZ_SIG)) != 0)
        {
            return false;
        }
        fseek(sfile, hdr.offset, SEEK_SET);
        uint32_t *indexPtr = new uint32_t[hdr.count];
        readfile(indexPtr, sizeof(uint32_t) * hdr.count);
        index.clear();
        for (int i = 0; i < hdr.count; ++i)
        {
            index.push_back(indexPtr[i]);
        }
        delete[] indexPtr;
        fclose(sfile);
    }
    return sfile != nullptr;
}

/**
 * @brief create an index from memory. create a vector of map offsets within the memory blob
 *
 * @param ptr
 * @param index
 * @return true
 * @return false
 */
bool CMapArch::indexFromMemory(uint8_t *ptr, IndexVector &index)
{
    // check signature
    if (memcmp(ptr, MAAZ_SIG, sizeof(MAAZ_SIG)) != 0)
    {
        return false;
    }
    index.clear();
    uint16_t count = 0;
    memcpy(&count, ptr + OFFSET_COUNT, sizeof(count));
    uint32_t indexBase = 0;
    memcpy(&indexBase, ptr + OFFSET_INDEX, sizeof(indexBase));
    for (uint16_t i = 0; i < count; ++i)
    {
        long idx = 0;
        memcpy(&idx, ptr + indexBase + i * sizeof(uint32_t), sizeof(uint32_t));
        index.push_back(idx);
    }
    return true;
}

/**
 * @brief remove all the maps from the maparch without deleting the memory pointers
 *
 */
void CMapArch::removeAll()
{
    m_maps.clear();
}

/**
 * @brief Read maps from a memory blob
 *
 * @param ptr
 * @return true
 * @return false
 */
bool CMapArch::fromMemory(uint8_t *ptr)
{
    if (memcmp(ptr, MAAZ_SIG, sizeof(MAAZ_SIG)) != 0)
    {
        m_lastError = "mapArch signature is wrong";
        return false;
    }
    clear();
    uint16_t count = 0;
    memcpy(&count, ptr + OFFSET_COUNT, sizeof(count));
    uint32_t indexBase = 0;
    memcpy(&indexBase, ptr + OFFSET_INDEX, sizeof(indexBase));
    for (uint16_t i = 0; i < count; ++i)
    {
        uint32_t idx = 0;
        memcpy(&idx, ptr + indexBase + i * sizeof(uint32_t), sizeof(uint32_t));
        CMap *map = new CMap();
        if (!map->fromMemory(ptr + idx))
        {
            m_lastError = "map data is corrupt";
            return false;
        }
        add(map);
    }
    return true;
}