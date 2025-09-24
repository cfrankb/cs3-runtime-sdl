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
#pragma once
#include <vector>
#include <string>
#include <cstdint>

class IFile;
class CMap;

typedef std::vector<long> IndexVector;

class CMapArch
{
public:
    CMapArch();
    virtual ~CMapArch();

    size_t size();
    const char *lastError();
    void clear();
    size_t add(CMap *map);
    CMap *removeAt(int i);
    void insertAt(int i, CMap *map);
    CMap *at(int i);
    bool read(IFile &file);
    bool read(const char *filename);
    bool extract(const char *filename);
    bool write(const char *filename);
    const char *signature();
    void removeAll();
    static bool indexFromFile(const char *filename, IndexVector &index);
    static bool indexFromMemory(uint8_t *ptr, IndexVector &index);
    bool fromMemory(uint8_t *ptr);

protected:
    enum
    {
        OFFSET_COUNT = 6,
        OFFSET_INDEX = 8,
    };
    template <typename ReadFunc, typename SeekFunc, typename ReadMapFunc>
    bool readCommon(ReadFunc readfile, SeekFunc seekfile, ReadMapFunc readmap);
    std::vector<CMap *> m_maps;
    std::string m_lastError;
};
