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
#include <string>
#include <cstdint>

#pragma once

struct data_t
{
    uint8_t *ptr;
    size_t size;
};

namespace AssetMan
{
    std::string addTrailSlash(const std::string &path);
    void setPrefix(const std::string &prefix);
    const std::string &getPrefix();
    bool read(const std::string &filepath, data_t &data, bool terminator = false);
    void free(const data_t data);
    std::string defaultPrefix();
}