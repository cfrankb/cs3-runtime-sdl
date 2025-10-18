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
#include "../../../src/shared/FileMem.h"
#include "../../../src/maparch.h"
#include "../../../src/map.h"
#include "../../../src/game.h"
#include "../../../src/states.h"
#include "../../../src/statedata.h"
#include "../../../src/logger.h"
#include "../../../src/tilesdata.h"
#include "../../../src/sprtypes.h"

struct AppParams
{
    std::vector<std::string> files;
    bool strip;
    bool report;
};

/**
 * @brief Show application usage
 *
 * @param cmd
 */
void showUsage(const char *cmd)
{
    printf(
        "mapzutil\n\n"
        "usage: \n"
        "       %s [-sr] file1.mapz [file1.mapz]\n"
        "\n"
        "filex.mapz         mapz file\n"
        "-s                 strip private maps\n"
        "-r                 output maps report\n"
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
                LOGE("invalid switch: %s\n", src);
                return EXIT_FAILURE;
            }
            else if (src[1] == 's')
            {
                appSettings.strip = true;
                continue;
            }
            else if (src[1] == 'r')
            {
                appSettings.report = true;
                continue;
            }
            LOGE("invalid switch: %s\n", src);
            return EXIT_FAILURE;
        }
        appSettings.files.emplace_back(src);
    }
    return EXIT_SUCCESS;
}

bool report(CMapArch &mf, IFile &file)
{
    const int BUFSIZE = 4095;
    char *tmp = new char[BUFSIZE + 1];
    auto writeItem = [&file, tmp](auto str, auto v)
    {
        sprintf(tmp, "  -- %-15s: %d\n", str, static_cast<int>(v));
        file += tmp;
    };

    std::unordered_map<uint16_t, std::string> labels;
    const auto &keyOptions = getKeyOptions();
    for (const auto &[v, k] : keyOptions)
    {
        labels[k] = v;
    }

    typedef std::unordered_map<uint8_t, uint32_t> StatMap;

    file += "Map List\n";
    file += "========\n\n";

    LOGI("@@@tell %lu", file.tell());

    for (size_t i = 0; i < mf.size(); ++i)
    {
        CMap *map = mf.at(i);
        sprintf(tmp, "Level %.2lu: %s\n", i + 1, map->title());
        file += tmp;
    }

    file += "\n";
    file += "MapArch statistics\n";
    file += "==================\n\n";

    StatMap globalUsage;

    for (size_t i = 0; i < mf.size(); ++i)
    {
        CMap *map = mf.at(i);
        MapReport report = CGame::generateMapReport(*map);
        StatMap usage;
        int monsters = 0;
        int stops = 0;
        for (int y = 0; y < map->hei(); ++y)
        {
            for (int x = 0; x < map->len(); ++x)
            {
                const auto &c = map->at(x, y);
                ++usage[c];
                ++globalUsage[c];
                auto &def = getTileDef(c);
                if (def.type == TYPE_MONSTER || def.type == TYPE_VAMPLANT)
                {
                    ++monsters;
                }
                if (def.type == TYPE_STOP)
                {
                    ++stops;
                }
            }
        }

        sprintf(tmp, "Level %.2lu: %s\n", i + 1, map->title());
        file += tmp;
        writeItem("Unique tiles", usage.size());
        writeItem("Monsters", monsters);
        writeItem("Attributes", map->attrs().size());
        writeItem("Stops", stops);
        sprintf(tmp, "  -- Size: %d x %d\n", map->len(), map->hei());
        file += tmp;
        writeItem("fruits", report.fruits);
        writeItem("treasures", report.bonuses);
        writeItem("secrets", report.secrets);

        CStates &states = map->states();
        std::vector<StateValuePair> pairs = states.getValues();
        if (pairs.size())
        {
            file += "\nMeta-data\n";
            for (const auto &item : pairs)
            {
                const bool isStr = (item.key & 0xff) >= 0x80;
                const std::string label = labels[item.key];
                sprintf(tmp, "  -- %-12s %s", label.c_str(), isStr ? item.value.c_str() : item.tip.c_str());
                file += tmp;
                if (isStr)
                    strncpy(tmp, "\n", BUFSIZE);
                else
                    sprintf(tmp, " [%s]\n", item.value.c_str());
                file += tmp;
            }
        }
        file += "\n-----------------------------------\n";
        file += "\n";
    }

    file += "Par time\n";
    file += "==================\n\n";
    for (size_t i = 0; i < mf.size(); ++i)
    {
        CMap *map = mf.at(i);
        CStates &states = map->states();
        uint16_t parTime = states.getU(PAR_TIME);
        if (parTime == 0)
            continue;
        sprintf(tmp, "Level %.2lu: %s\n", i + 1, map->title());
        file += tmp;
        const int seconds = parTime % 60;
        const int minutes = parTime / 60;
        sprintf(tmp, "   PAR TIME:   %.2d:%.2d\n\n", minutes, seconds);
        file += tmp;
    }

    sprintf(tmp, "\nGlobal Unique tiles: %lu\n", globalUsage.size());
    file += tmp;

    return true;
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
                // delete map;
                ++mapRemoved;
                printf("stripping out: level %d - %s\n", i + 1, map->title());
                auto map = arch.removeAt(i);
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
                LOGE("failed to replace: %s\n", filepath.c_str());
                return false;
            }
        }
        else
        {
            printf("no map removed\n");
        }
    }

    if (params.report)
    {
        CFileMem mem;
        if (!mem.open("", "wb"))
        {
            LOGE("failed open filemem");
            return false;
        }
        report(arch, mem);
        LOGI("getSize: %lu", mem.getSize());
        LOGI("tell: %lu", mem.tell());
        size_t size = mem.buffer().size();
        char *buf = new char[size + 1];
        buf[size] = '\0';
        LOGI("size: %lu", size);

        memcpy(buf, (void *)mem.buffer().data(), size);
        puts(buf);
        delete[] buf;
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
            LOGE("failed to read arch: %s\n", arch.lastError());
            file.close();
            return false;
        }
    }
    else
    {
        LOGE("can't open %s\n", filepath.c_str());
        return false;
    }

    return true;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)envp;
    AppParams params;
    params.strip = false;
    params.report = false;
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
