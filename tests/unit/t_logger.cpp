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
#include <fstream>
#include <string>
#include "t_logger.h"
#include "../../src/logger.h"

static std::string readAll(const std::string &name)
{
    std::ifstream f(name, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

// Logger file output + level filtering. The logger is restored to stdout
// before the file checks so later suites keep clean output. LOGF exits(1)
// and is therefore not exercised here.
bool test_logger()
{
    namespace fs = std::filesystem;
    const std::string f1 = "tests/out/logger_1.log";
    const std::string f2 = "tests/out/logger_2.log";
    fs::remove(f1);
    fs::remove(f2);

    Logger::setLevel(Logger::L_INFO);
    Logger::setOutputFile(f1);
    LOGI("logger-info %d", 42);
    LOGW("logger-warn");
    LOGE("logger-error");
    LOGI("explicit-newline\n");

    // second setOutputFile must close the previous file and reopen
    Logger::setOutputFile(f2);
    LOGI("logger-second-file");

    // everything below L_WARN is filtered out
    Logger::setLevel(Logger::L_WARN);
    LOGI("logger-hidden-info");
    LOGW("logger-visible-warn");
    LOGE("logger-visible-error");

    // reset: setOutputFile("") fails to fopen and restores stdout routing
    Logger::setLevel(Logger::L_INFO);
    Logger::setOutputFile("");

    const std::string s1 = readAll(f1);
    if (s1.find("[t_logger.cpp/INFO] logger-info 42") == std::string::npos ||
        s1.find("[t_logger.cpp/WARN] logger-warn") == std::string::npos ||
        s1.find("[t_logger.cpp/ERROR] logger-error") == std::string::npos)
    {
        LOGE("test_logger: missing tagged line in %s", f1.c_str());
        return false;
    }
    // a format ending in \n must not get a second newline appended
    if (s1.find("explicit-newline\n\n") != std::string::npos)
    {
        LOGE("test_logger: duplicated newline in %s", f1.c_str());
        return false;
    }

    const std::string s2 = readAll(f2);
    if (s2.find("logger-second-file") == std::string::npos)
    {
        LOGE("test_logger: line missing after output file switch");
        return false;
    }
    if (s2.find("logger-hidden-info") != std::string::npos)
    {
        LOGE("test_logger: filtered INFO still logged");
        return false;
    }
    if (s2.find("[t_logger.cpp/WARN] logger-visible-warn") == std::string::npos ||
        s2.find("[t_logger.cpp/ERROR] logger-visible-error") == std::string::npos)
    {
        LOGE("test_logger: WARN/ERROR lines missing at L_WARN level");
        return false;
    }
    // the filtered line must not have leaked into the first file either
    if (readAll(f1).find("logger-hidden-info") != std::string::npos)
    {
        LOGE("test_logger: filtered INFO landed in first file");
        return false;
    }
    return true;
}
