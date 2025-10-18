import shutil
import re
import copy

notice = """
/*
    cs3-runtime-sdl
    Copyright (C) 2025  Francois Blanchette

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
""".strip()

struct = """
namespace BossData
{
    enum Path:uint8_t {
        ASTAR,
        BFS,
        LOS,
        ASTAR_SMOOTH
    };
}

struct boss_seq_t
{
    int base;
    int lenght;
};

struct bossData_t
{
    const char* name;       // boss name
    int speed;              // movement speed
    int a_speed;            // animation speed
    int hp;                 // hp
    int type;               // type
    int score;              // score received
    int damage;             // damage given
    uint32_t flags;         // custom flags
    uint32_t path;          // path finding algo
    uint8_t bullet;         // boss bullet
    uint8_t bullet_speed;   // boss bullet speed
    int chase_distance;     // distance to engage chase
    int pursuit_distance;   // continue pursuit within distance (chase)
    bool is_goal;           // is this boss a map goal?
    bool show_details;      // display hp bar/name
    boss_seq_t moving;      // animation seq: moving
    boss_seq_t attack;      // animation seq: attack
    boss_seq_t hurt;        // animation seq: hurt
    boss_seq_t death;       // animation seq: death
    boss_seq_t idle;        // animation seq: idle
    Rect hitbox;            // boss hitbox
    int sheet;              // sprite sheet used
};
"""

cpp_body = """
bossData_t *getBossData(const int type)
{
    constexpr size_t bossCount = sizeof(g_bosses) / sizeof(g_bosses[0]);
    for (size_t i = 0; i < bossCount; ++i)
    {
        if (g_bosses[i].type == type)
            return &g_bosses[i];
    }
    return nullptr;
}
""".strip()


def clean_name(name):
    return re.sub("[^0-9a-zA-Z_]+", "_", name.upper())


def sanity_check(seq):
    result = True
    missing_fields = [k for k in all_fields if k not in seq]
    if missing_fields:
        print(f"missing fields: {missing_fields} for `{seq['name']}`")
        result = False
    invalid_fields = [k for k in seq.keys() if k not in all_fields]
    if invalid_fields:
        print(f"invalid fields: {invalid_fields} for `{seq['name']}`")
        result = False
    return result


def save_seq(all_seqs, seq):
    if seq:
        all_seqs.append(copy.deepcopy(seq))
        seq = {}


attr_names = [
    "speed",
    "a_speed",
    "hp",
    "type",
    "score",
    "damage",
    "flags",
    "path",
    "bullet",
    "bullet_speed",
    "chase_distance",
    "pursuit_distance",
    "is_goal",
    "show_details",
]
seq_names = ["moving", "attack", "hurt", "death", "idle"]
misc_names = ["hitbox", "sheet"]
all_fields = ["name"] + attr_names + seq_names + misc_names


EXIT_FAILURE = 1
EXIT_SUCCESS = 0

with open("bosses.ini") as sfile:
    data = sfile.read()

boss_types = []
g_vars = []
defines = []
all_seqs = []
seq = {}
sheet = 0

section = ""
line = 0
frame = 0
for t in data.split("\n"):
    line += 1
    t = t.split("#", 1)[0].strip()
    if not t:
        continue

    e = [x for x in t.split() if x]
    if t[0] == "[" and t[-1] == "]":
        # save previous boss
        save_seq(all_seqs, seq)
        section = t[1:-1].strip()
        if not section:
            print(f"section header empty on line {line}")
            exit(EXIT_FAILURE)
        seq["name"] = section
        seq["sheet"] = sheet
    elif t[0] == "[":
        # save previous boss
        save_seq(all_seqs, seq)
        print(f"section header not terminated on line {line}")
        section = t[1:].strip()
        if not section:
            print(f"section header empty on line {line}")
            exit(EXIT_FAILURE)
        seq["name"] = section
        seq["sheet"] = sheet
    elif e[0] == ">>>sheet" and len(e) == 2:
        sheet = int(e[1])
        print(f"switched to sheet {sheet}")
        frame = 0
    elif not section:
        if len(e) != 3:
            print(f"expecting 3 literals on line {line}: {t} -- found {len(e)}")
            continue
        if e[0] == "private":
            name = clean_name(e[1])
            g_vars.append(f"constexpr auto {name} = {e[2]};")
        elif e[0] == "define":
            defines.append(f"#define {e[1].upper()} {e[2]}")
        else:
            f"unknown operator `{e[0]}` on line {line}"
    elif section:
        item_name = e[0]
        if item_name == "hitbox":
            if len(e) == 5:
                seq[item_name] = " ".join(e[1:])
            else:
                print(f"{item_name} must have 4 dimensions")
            continue
        if len(e) != 2:
            print(f"missing qualifier on line {line}: {t}")
            continue
        if item_name in seq_names:
            seq_name = item_name
            if e[1][0] == "@":
                ref_seq = e[1][1:]
                vars = copy.deepcopy(seq[ref_seq])
            else:
                frame_count = int(e[1])
                prefix = clean_name(section)
                suffix = e[0].upper()
                vars = []
                var_name = f"{prefix}_{suffix}_BASE"
                vars.append(var_name)
                g_vars.append(f"constexpr int {var_name} = {frame};")
                var_name = f"{prefix}_{suffix}_LEN"
                vars.append(var_name)
                g_vars.append(f"constexpr int {var_name} = {frame_count};")
                frame += frame_count
            seq[seq_name] = vars  # [frame, frame_count]
        elif item_name == "flags":
            seq[item_name] = " ".join(e[1:]).replace(",", " |")
        elif item_name in attr_names:
            seq[item_name] = e[1]
        else:
            print(f"item name {item_name} on {line} is unknown")
    else:
        print(f"unknown data on line {line}: {t}")

save_seq(all_seqs, seq)

"""
bossData_t bosses[] = {
    {
        .moving={},
        .attack={},
        .hurt={},
        .death={},
    }
};
"""

for seq in all_seqs:
    if not sanity_check(seq):
        print("please fix config")
        exit(EXIT_FAILURE)
    name = clean_name(seq["name"])
    boss_types.append(f"#define BOSS_{name} {seq['type']}")

SPACE = " " * 4
SPACE2 = " " * 8

with open("bossdata.cpp", "w") as tfile:
    tfile.write(notice + "\n\n")
    tfile.write("//////////////////////////////////////////////////\n")
    tfile.write("// autogenerated\n\n")
    tfile.write("#include <cstddef>\n")
    tfile.write('#include "color.h"\n')
    tfile.write('#include "tilesdata.h"\n')
    tfile.write('#include "bossdata.h"\n\n')
    tfile.write("namespace BossData\n")
    tfile.write(f"{{\n{SPACE}")
    tfile.write(f"\n{SPACE}".join(g_vars) + "\n")
    tfile.write("};\n\n")
    tfile.write("using namespace BossData;\n\n")
    tfile.write("bossData_t g_bosses[] = {\n")
    for seq in all_seqs:
        tfile.write(f"{SPACE}{{\n")
        code = f"{SPACE2}.name = \"{seq['name']}\",\n"
        tfile.write(code)
        for name in attr_names:
            code = f"{SPACE2}.{name} = {seq[name]},\n"
            tfile.write(code)
        for name in seq_names:
            code = f"{SPACE2}.{name} = {{{seq[name][0]}, {seq[name][1]}}},\n"
            tfile.write(code)
        code = f"{SPACE2}.hitbox = {{{seq['hitbox']}}},\n"
        tfile.write(code)
        code = f"{SPACE2}.sheet = {seq['sheet']},\n"
        tfile.write(code)
        tfile.write(f"{SPACE}}},\n")
    tfile.write(f"}};\n")
    tfile.write("\n" + cpp_body + "\n\n")
with open("bossdata.h", "w") as tfile:
    tfile.write(notice + "\n\n")
    tfile.write("//////////////////////////////////////////////////\n")
    tfile.write("// autogenerated\n\n")
    tfile.write("#pragma once\n\n")
    tfile.write("#include <cstdint>\n")
    tfile.write('#include "rect.h"\n\n')
    tfile.write("\n".join(defines) + "\n\n")
    tfile.write("\n".join(boss_types) + "\n\n")
    tfile.write(struct)
    tfile.write("\nbossData_t *getBossData(const int type);\n")

shutil.copy("bossdata.h", "../../src")
shutil.copy("bossdata.cpp", "../../src")

exit(EXIT_SUCCESS)
