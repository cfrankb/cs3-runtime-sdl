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
#include "../src/strhelper.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/logger.h"

CStateParser::CStateParser()
{
}

CStateParser::~CStateParser()
{
}

/**
 * @brief parse string into uint16_t. diffentiate between decimal and hex notations
 *
 * @param s
 * @param isValid
 * @return uint16_t
 */
uint16_t CStateParser::parseStringToUShort(const std::string &s, bool &isValid)
{
    uint16_t v = 0;
    isValid = false;
    if (s.substr(0, 2) == "0x" ||
        s.substr(0, 2) == "0X")
    {
        v = std::stoul(s.substr(2), 0, 16);
        isValid = true;
    }
    else if (isdigit(s[0]) || s[0] == '-')
    {
        v = std::stoul(s, 0, 10);
        isValid = true;
    }
    return v;
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
    strncpy(t, data, len + 1);
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
                    LOGE("key %s on line %d not found.", ks, line);
                }
            }
            else
            {
                LOGE("found %ld args on line %d -- should be 2.", list.size(), line);
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
    strncpy(t, data, len + 1);
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
                    LOGE("invalid expression `%s` on line %d", list[0].c_str(), line);
                }
                m_defines[k] = v;
            }
            else
            {
                LOGE("found %ld args on line %d -- should be 2.", list.size(), line);
            }
        }
        p = next;
        ++line;
    }
    delete[] t;
}

void CStateParser::debug()
{
    LOGI("**** defines");
    for (const auto &[k, v] : m_defines)
    {
        LOGI("[%s] => [0x%.4x %u]", k.c_str(), v, v);
    }
}
