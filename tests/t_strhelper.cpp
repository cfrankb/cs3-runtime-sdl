#include "t_strhelper.h"
#include <cstdio>
#include <cstring>
#include "../src/strhelper.h"

bool test_strhelper()
{
    std::string t1 = "toto123 ";
    // printf("`%s` => `%s`\n", t1.c_str(), trimString(t1).c_str());
    if (trimString(t1) != "toto123")
        return false;

    std::string t2 = " toto123";
    // printf("`%s` => `%s`\n", t2.c_str(), trimString(t2).c_str());
    if (trimString(t2) != "toto123")
        return false;

    std::string t3 = " toto123 ";
    // printf("`%s` => `%s`\n", t3.c_str(), trimString(t3).c_str());
    if (trimString(t3) != "toto123")
        return false;

    std::string t4 = "toto123";
    // printf("`%s` => `%s`\n", t4.c_str(), trimString(t4).c_str());
    if (trimString(t4) != "toto123")
        return false;

    std::string t5 = "";
    // printf("`%s` => `%s`\n", t5.c_str(), trimString(t5).c_str());
    if (trimString(t5) != "")
        return false;

    std::string t6 = " ";
    // printf("`%s` => `%s`\n", t6.c_str(), trimString(t6).c_str());
    if (trimString(t6) != "")
        return false;

    std::string t7 = " a";
    // printf("`%s` => `%s`\n", t7.c_str(), trimString(t7).c_str());
    if (trimString(t7) != "a")
        return false;

    std::string t8 = "\ta";
    if (trimString(t8) != "a")
        return false;

    std::string t9 = "\ra";
    if (trimString(t9) != "a")
        return false;

    return true;
}