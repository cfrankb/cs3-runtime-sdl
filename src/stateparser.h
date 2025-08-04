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
#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class CStates;
class IFile;

class CStateParser
{
public:
    CStateParser();
    ~CStateParser();

    void clear();
    void parseStates(const char *data, CStates &states);
    void parseStates(FILE *sfile, CStates &states);
    void parseStates(IFile &sfile, CStates &states);
    void parse(const char *data);
    uint16_t get(const char *k);
    bool exists(const char *k);
    void debug();

private:
    std::unordered_map<std::string, uint16_t> m_defines;
};