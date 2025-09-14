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
#include <cstring>
#include "stateparser.h"
#include "strhelper.h"
#include "states.h"
#include "shared/FileWrap.h"

CStateParser::CStateParser()
{
}

CStateParser::~CStateParser()
{
}

void CStateParser::clear()
{
    m_defines.clear();
}

bool CStateParser::exists(const char *k) const
{
    const auto &it = m_defines.find(k);
    return it != m_defines.end();
}

void CStateParser::parseStates(FILE *sfile, CStates &states)
{
    fseek(sfile, 0, SEEK_END);
    size_t size = ftell(sfile);
    fseek(sfile, 0, SEEK_SET);
    char *tmp = new char[size + 1];
    fread(tmp, size, 1, sfile);
    tmp[size] = '\0';
    parseStates(tmp, states);
    delete[] tmp;
}

void CStateParser::parseStates(IFile &sfile, CStates &states)
{
    size_t size = sfile.getSize();
    char *tmp = new char[size + 1];
    sfile.seek(0);
    sfile.read(tmp, size);
    tmp[size] = '\0';
    parseStates(tmp, states);
    delete[] tmp;
}

void CStateParser::parseStates(const char *data, CStates &states)
{
    size_t len = strlen(data);
    char *t = new char[len + 1];
    strcpy(t, data);
    char *p = t;
    int line = 1;
    while (p && *p)
    {
        char *next = processLine(p);
        if (p[0])
        {
            std::vector<std::string> list;
            splitString2(p, list);
            if (list.size() == 2)
            {
                const auto ks = list[0].c_str();
                if (exists(ks))
                {
                    const auto k = get(ks);
                    bool isNum;
                    const auto v = parseStringToUShort(list[1], isNum);
                    if (isNum)
                    {
                        states.setU(k, v);
                    }
                    else
                    {
                        states.setS(k, list[1]);
                    }
                }
                else
                {
                    fprintf(stderr, "key %s on line %d not found.\n", ks, line);
                }
            }
            else
            {
                fprintf(stderr, "found %ld args on line %d -- should be 2.\n", list.size(), line);
            }
        }
        p = next;
        ++line;
    }

    delete[] t;
}

uint16_t CStateParser::get(const char *k) const
{
    const auto &it = m_defines.find(k);
    if (it != m_defines.end())
    {
        return it->second;
    }
    else
    {
        return 0;
    }
}

void CStateParser::parse(const char *data)
{
    size_t len = strlen(data);
    char *t = new char[len + 1];
    strcpy(t, data);
    char *p = t;
    int line = 1;
    while (p && *p)
    {
        char *next = processLine(p);
        if (p[0])
        {
            std::vector<std::string> list;
            splitString2(p, list);
            if (list.size() == 2)
            {
                const auto &k = list[0];
                bool isNum;
                const auto v = parseStringToUShort(list[1], isNum);
                if (!isNum)
                {
                    fprintf(stderr, "invalid expression `%s` on line %d\n", list[0].c_str(), line);
                }
                m_defines[k] = v;
            }
            else
            {
                fprintf(stderr, "found %ld args on line %d -- should be 2.\n", list.size(), line);
            }
        }
        p = next;
        ++line;
    }
    delete[] t;
}

void CStateParser::debug()
{
    printf("**** defines\n\n");
    for (const auto &[k, v] : m_defines)
    {
        printf("[%s] => [0x%.4x %u]\n", k.c_str(), v, v);
    }
}