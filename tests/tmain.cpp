#include <cstdio>
#include "t_recorder.h"
#include "t_states.h"

int main(int argc, char *args[])
{

    printf("running tests\n");
    printf("==============\n\n");

    int failed = !test_recorder() +
                 !test_states();

    printf("\n\n###### failed: %d\n", failed);
    return failed != 0;
}