#include "t_maparch.h"
#include "../src/maparch.h"
#include "../src/map.h"
#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"

#define KEY1 0x1990
#define KEY2 0x1990
#define VAL1 0x0522
#define VAL2 "Beginning of the End"
#define IN_FILE "tests/in/levels1.mapz"
#define OUT_FILE1 "tests/out/levels1.mapz"
#define OUT_FILE2 "tests/out/levels2.mapz"

void injectStates(CMapArch &arch)
{
    CMap *map = arch.at(0);
    CStates &states = map->states();
    states.setU(KEY1, VAL1);
    states.setS(KEY2, VAL2);
}

bool checkInjectedStates(CMapArch &arch, const char *context)
{
    CMap *map = arch.at(0);
    auto &states = map->states();
    if (states.getU(KEY1) != VAL1)
    {
        fprintf(stderr, "missing injected state. %s\n", context);
        return false;
    }

    if (std::string(states.getS(KEY2)) != VAL2)
    {
        fprintf(stderr, "missing injected state. %s\n", context);
        return false;
    }
    return true;
}

bool test_maparch()
{
    CMapArch arch1;
    arch1.read(IN_FILE);
    injectStates(arch1);
    if (!checkInjectedStates(arch1, "initial injection"))
    {
        return false;
    }

    // test writing: method 1
    arch1.write(OUT_FILE1);

    // test reading: method 1
    CMapArch arch2;
    arch2.read(OUT_FILE1);
    if (arch1.size() != arch2.size())
    {
        fprintf(stderr, "map count mismatch in arch after write\n");
        return false;
    }

    // test reading: method 2
    CMapArch arch3;
    CFileWrap file;
    file.open(OUT_FILE1);
    arch3.read(file);
    file.close();

    // test writing: method 2
    arch3.write(OUT_FILE2);
    if (getFileSize(OUT_FILE1) != getFileSize(OUT_FILE2))
    {
        fprintf(stderr, "different disk size for identical content\n");
        return false;
    }

    // check persistence after reload
    if (!checkInjectedStates(arch3, "data persistence"))
    {
        return false;
    }
    return true;
}