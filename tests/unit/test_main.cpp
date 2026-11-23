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
#include "test_main.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include "../../src/logger.h"

int run_suite(const char *suite, const TestEntry *tests, int count, int argc, char *argv[])
{
    const char *only = (argc > 1 && std::strcmp(argv[1], "--list") != 0) ? argv[1] : nullptr;

    if (argc > 1 && std::strcmp(argv[1], "--list") == 0)
    {
        for (int i = 0; i < count; ++i)
        {
            std::printf("%s\n", tests[i].name);
        }
        return 0;
    }

    std::filesystem::create_directories("tests/out");
    LOGI("running %s tests", suite);

    int failed = 0;
    int matched = 0;
    for (int i = 0; i < count; ++i)
    {
        if (only && std::strcmp(only, tests[i].name) != 0)
        {
            continue;
        }
        ++matched;
        LOGI("==> %s()", tests[i].name);
        if (!tests[i].fn())
        {
            LOGE("XXXXXX %s() FAILED", tests[i].name);
            ++failed;
        }
    }

    if (only && matched == 0)
    {
        LOGE("unknown test '%s' in suite %s", only, suite);
        return 1;
    }

    if (failed != 0)
    {
        LOGE("###### %s failed: %d", suite, failed);
    }
    else
    {
        LOGI("###### %s completed", suite);
    }
    return failed != 0;
}
