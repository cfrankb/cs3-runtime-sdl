#include <cstdlib>
#include <time.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>
#include <map>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>
#include <format>
#include <filesystem>
#include "../../../src/shared/FileWrap.h"
#include "../../../src/maparch.h"
#include "../../../src/map.h"
#include "../../../src/states.h"
#include "../../../src/statedata.h"
#include "../../../src/logger.h"

struct AppParams
{
    std::vector<std::string> files;
    bool strip;
};

/**
 * @brief Show application usage
 *
 * @param cmd
 */
void showUsage(const char *cmd)
{
    printf(
        "mcxz tileset generator\n\n"
        "usage: \n"
        "       %s [-s] file1.mapz [file1.mapz]\n"
        "\n"
        "filex.mapz         mapz file\n"
        "-s                 strip private maps\n"
        "-h --help          show help\n"
        "\n",
        cmd);
}

int parseParams(int argc, char *argv[], AppParams &appSettings)
{
    for (int i = 1; i < argc; ++i)
    {
        char *src = argv[i];
        if (src[0] == '-')
        {
            if (strcmp(src, "--help") == 0 || strcmp(src, "-h") == 0)
            {
                showUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            if (strlen(src) != 2)
            {
                ELOG("invalid switch: %s\n", src);
                return EXIT_FAILURE;
            }
            else if (src[1] == 's')
            {
                appSettings.strip = true;
                continue;
            }
            ELOG("invalid switch: %s\n", src);
            return EXIT_FAILURE;
        }
        appSettings.files.push_back(src);
    }
    return EXIT_SUCCESS;
}

bool doActions(CMapArch &arch, const std::string &filepath, const AppParams &params)
{
    if (params.strip)
    {
        int mapRemoved = 0;
        for (int i = static_cast<int>(arch.size()) - 1; i >= 0; --i)
        {
            CMap *map = arch.at(i);
            CStates &states = map->states();
            if (states.getU(PRIVATE) != 0)
            {
                ++mapRemoved;
                printf("stripping out: level %d - %s\n", i + 1, map->title());
                arch.removeAt(i);
                delete map;
            }
        }
        if (mapRemoved)
        {
            printf("removed %d map%s\n", mapRemoved, mapRemoved > 1 ? "s" : "");
            if (arch.write(filepath.c_str()))
            {
                printf("replaced done: %s\n", filepath.c_str());
            }
            else
            {
                ELOG("failed to replace: %s\n", filepath.c_str());
                return false;
            }
        }
        else
        {
            printf("no map removed\n");
        }
    }
    return true;
}

bool processFile(const std::string &filepath, const AppParams &params)
{
    CFileWrap file;
    if (file.open(filepath.c_str(), "rb"))
    {
        CMapArch arch;
        if (arch.read(file))
        {
            file.close();
            return doActions(arch, filepath, params);
        }
        else
        {
            ELOG("failed to read arch: %s\n", arch.lastError());
            file.close();
            return false;
        }
    }
    else
    {
        ELOG("can't open %s\n", filepath.c_str());
        return false;
    }

    return true;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)envp;
    AppParams params;
    params.strip = false;
    if (parseParams(argc, argv, params) == EXIT_FAILURE)
        return EXIT_FAILURE;

    if (params.files.size() == 0)
        printf("nothing to do\n");

    for (const auto &filepath : params.files)
    {
        if (!processFile(filepath, params))
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
