/*
    LGCK Builder Runtime
    Copyright (C) 1999, 2016, 2025  Francois Blanchette

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

#include <string>
#include <string_view>
#include <vector>
#include "IFile.h"

class CFileMem : public IFile
{
public:
    CFileMem();
    ~CFileMem();

    CFileMem &operator>>(std::string &str) override;
    CFileMem &operator<<(const std::string_view &str) override;
    CFileMem &operator<<(const char *s) override;
    CFileMem &operator+=(const std::string_view &str) override;

    CFileMem &operator>>(int &n) override;
    CFileMem &operator<<(int n) override;

    CFileMem &operator>>(bool &b) override;
    CFileMem &operator<<(const bool b) override;
    CFileMem &operator+=(const char *) override;

    bool open(const char *filename = "", const char *mode = "") override;
    int read(void *buf, int size) override;
    int write(const void *buf, int size) override;

    bool close() override;
    long getSize() override;
    bool seek(long i) override;
    long tell() override;

    const char *buffer();
    void replace(const char *buffer, size_t size);
    bool flush() override;

private:
    void append(const void *data, int size);

    std::string m_filename;
    std::vector<char> m_buffer;
    size_t m_max;
    size_t m_ptr;
    std::string m_mode;

    enum
    {
        GROWBY = 8192
    };
};
