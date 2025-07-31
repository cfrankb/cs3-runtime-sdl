#include "../src/states.h"
#include "../src/shared/FileWrap.h"
#include <cstdio>
#include <cstring>

const uint8_t k1 = 1;
const char *v1 = "test1";

const uint8_t k2 = 0x22;
const uint16_t v2 = 0x44;

static bool checkState(CStates &states, const char *context)
{
    if (strcmp(states.getS(k1), v1))
    {
        printf("%s: string not matching\n", context);
        return false;
    }

    auto a2 = states.getU(k2);
    if (a2 != v2)
    {
        printf("%s: uint8 not matching %d %d\n", context,
               a2, v2);
        return false;
    }
    return true;
}

bool test_states()
{
    printf("==> test_states()\n");

    CStates states;
    states.setU(k2, v2);
    states.setS(k1, v1);
    if (!checkState(states, "direct"))
        return false;

    FILE *tfile;
    FILE *sfile;
    CFileWrap file;

    // write test
    tfile = fopen("tests/out/state0.bin", "wb");
    states.write(tfile);
    fclose(tfile);

    file.open("tests/out/state1.bin", "wb");
    states.write(file);
    file.close();

    // read test
    states.clear();
    sfile = fopen("tests/out/state0.bin", "rb");
    states.read(sfile);
    fclose(sfile);
    if (!checkState(states, "read FILE*"))
        return false;

    file.open("tests/out/state2.bin", "wb");
    states.write(file);
    file.close();

    states.clear();
    file.open("tests/out/state0.bin", "rb");
    states.read(file);
    file.close();
    if (!checkState(states, "read FileWrap"))
        return false;

    file.open("tests/out/state3.bin", "wb");
    states.write(file);
    file.close();

    CStates states2 = states;
    if (!checkState(states2, "copy constructor"))
        return false;

    return true;
}