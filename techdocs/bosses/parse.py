import shutil
import re
import copy
import os
import filecmp
import json


COPYRIGHT_NOTICE = """
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


SHEET_SPACER = 1000
EXIT_FAILURE = 1
EXIT_SUCCESS = 0
MAX_HITBOX_PER_FRAME = 4

CPP_FILE = "bossdata.cpp"
H_FILE = "bossdata.h"

TAB = " " * 4


HEADER_STRUCT = """
#define MAX_HITBOX_PER_FRAME {MAX_HITBOX_PER_FRAME}

namespace BossData
{
    enum Path:uint8_t {
        NONE,
        ASTAR,
        BFS,
        LOS,
        ASTAR_SMOOTH
    };

    enum HitBoxType:uint8_t {
        MAIN,
        ATTACK,
        SPECIAL1,
        SPECIAL2,
    };
}

struct boss_seq_t
{
    int base;
    int lenght;
};

struct hitbox_t
{
    int x;
    int y;
    int width;
    int height;
    int type;
};

struct sprite_hitbox_t
{
    int spriteID;
    int count;
    hitbox_t hitboxes[MAX_HITBOX_PER_FRAME];
};

struct bossData_t
{
    const char* name;       // boss name
    int speed;              // movement speed (lower = faster)
    int speed_anime;        // animation speed (lower = faster)
    int hp;                 // hp
    uint8_t type;           // type
    int score;              // score received
    int damage;             // damage given
    uint32_t flags;         // custom flags
    uint32_t path;          // path finding algo
    uint8_t bullet;         // boss bullet
    uint8_t bullet_rate;    // boss bullet rate (lower = faster)
    uint8_t bullet_algo;    // bullet algo
    uint8_t bullet_sound;   // bullet firing sound
    int bullet_timeout;     // bullet timeout (# of moves)
    uint8_t attack_sound;   // sound during an attack
    int distance_chase;     // distance to engage chase
    int distance_pursuit;   // continue pursuit within distance (chase)
    int distance_attack;    // distance within which the boss can attack
    bool is_goal;           // is this boss a map goal?
    bool show_details;      // display hp bar/name
    Color color_hp;         // hp bar color
    Color color_name;       // namr color
    int aims;               // aim count (typically 1 or 4)
    boss_seq_t moving;      // animation seq: moving
    boss_seq_t attack;      // animation seq: attack
    boss_seq_t hurt;        // animation seq: hurt
    boss_seq_t death;       // animation seq: death
    boss_seq_t idle;        // animation seq: idle
    hitbox_t hitbox;        // boss hitbox
    int sheet;              // sprite sheet used
};
""".replace(
    "{MAX_HITBOX_PER_FRAME}", str(MAX_HITBOX_PER_FRAME).strip()
)

CPP_BODY = """
const bossData_t *getBossData(const uint8_t type)
{
    for (size_t i = 0; i < BOSS_COUNT; ++i)
    {
        if (g_bosses[i].type == type)
            return &g_bosses[i];
    }
    return nullptr;
}

const sprite_hitbox_t *getHitbox(const int sheet, const int frameID)
{
    const int spriteID = sheet * SHEET_SPACER + frameID;
    for (size_t i = 0; i < HITBOX_COUNT; ++i)
    {
        if (g_hitboxes[i].spriteID == spriteID)
            return &g_hitboxes[i];
    }
    return nullptr;
}

""".strip()


class GridBox:
    GRID_SIZE = 8
    x: int = 0
    y: int = 0
    w: int = 0
    h: int = 0
    type: int = 0

    def __init__(self, x: int, y: int, w: int, h: int, type: int):
        self.x = int(x / self.GRID_SIZE)
        self.y = int(y / self.GRID_SIZE)
        self.w = int(w / self.GRID_SIZE)
        self.h = int(h / self.GRID_SIZE)
        self.type = type

    def __str__(self) -> str:
        return f"{{ {self.x}, {self.y}, {self.w}, {self.h}, {self.type} }}"

    def __repr__(self) -> str:
        return f"Box({self.x}, {self.y}, {self.w}, {self.h}, {self.type})"


def read_hitbox_file(input_file: str) -> dict[list[GridBox]]:

    sprites: dict[list[GridBox]] = dict()

    try:
        with open(input_file, "r") as file:
            data = json.load(file)
    except FileNotFoundError:
        print("Error: 'data.json' not found.")
        return {}
    except json.JSONDecodeError:
        print("Error: Invalid JSON format in 'data.json'.")
        return {}

    frame_width, frame_heigth = [data["frame"]["width"], data["frame"]["height"]]
    frames_per_row = data["frame"]["cols"]

    count = 0

    for hitbox in data["hitboxes"]:

        x = hitbox["x"]
        y = hitbox["y"]

        col = int(x / frame_width)
        row = int(y / frame_heigth)
        id = row * frames_per_row + col

        rel_x = x - col * frame_width
        rel_y = y - row * frame_heigth

        if hitbox["type"] == 0:
            #   print(id, hitbox, rel_x, rel_y)
            count += 1
            continue

        box: GridBox = GridBox(rel_x, rel_y, hitbox["w"], hitbox["h"], hitbox["type"])
        if id not in sprites:
            sprites[id] = []
        sprites[id].append(box)
        # print(box)

    # print(sprites)
    print(os.path.basename(input_file), count)
    return sprites


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


def copyfile(src, dfolder):
    dpath = f"{dfolder}/{src}"
    if not os.path.isfile(dpath) or not filecmp.cmp(src, dpath):
        print(f"copying {src} to {dfolder}")
        shutil.copy(src, "../../src")


class Frame:
    hitboxes: list[GridBox]
    frame_id: int

    def __init__(self, frame_id):
        self.hitboxes: list[GridBox] = []
        self.frame_id = frame_id

    def addBox(self, grid_box: GridBox):
        self.hitboxes.append(grid_box)

        """
        sprite_hitbox_t hb[] = {
            1005,
            1,
            {3, 4, 2, 3, 2},
            {0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0},
        };
        """

    def __str__(self) -> str:
        r = f"{{ {self.frame_id}, {len(self.hitboxes)}, {{"

        for i in range(len(self.hitboxes)):
            r += f"{self.hitboxes[i]}, "

        r += "}}"
        return r

    def __repr__(self) -> str:
        r = f"[frame:{self.frame_id}][{len(self.hitboxes)}]"
        for hb in self.hitboxes:
            r += f"({hb})"
        return r


def handle_hitboxes(hitbox_file: str, sheet: int, base_frame: int):
    global all_frames

    # global hitbox_index
    sprites = read_hitbox_file(hitbox_file)
    for sprite_id, sprite_hitboxes in sprites.items():
        for hitbox in sprite_hitboxes:
            frame_id = base_frame + sprite_id + sheet * SHEET_SPACER
            if frame_id not in all_frames:
                all_frames[frame_id] = Frame(frame_id=frame_id)
            all_frames[frame_id].addBox(hitbox)
            # print(all_frames[frame_id])


def write_cpp(file_path):

    with open(file_path, "w") as tfile:
        tfile.write(COPYRIGHT_NOTICE + "\n\n")
        tfile.write("//////////////////////////////////////////////////\n")
        tfile.write("// autogenerated\n\n")
        tfile.write("#include <cstddef>\n")
        tfile.write('#include "tilesdata.h"\n')
        tfile.write('#include "sounds.h"\n')
        tfile.write('#include "bossdata.h"\n\n')
        tfile.write("namespace BossData\n")
        tfile.write(f"{{\n{TAB}")
        tfile.write(f"\n{TAB}".join(g_vars) + "\n")
        tfile.write("};\n\n")
        tfile.write("using namespace BossData;\n\n")

        tfile.write("const sprite_hitbox_t g_hitboxes[] = {\n")
        for k, frame in all_frames.items():
            tfile.write(f"{TAB}{frame},\n")
        tfile.write(f"}};\n\n")

        tfile.write("const bossData_t g_bosses[] = {\n")
        for seq in all_seqs:
            tfile.write(f"{TAB}{{\n")
            code = f"{TAB*2}.name = \"{seq['name']}\",\n"
            tfile.write(code)
            for name in attr_names:
                code = f"{TAB*2}.{name} = {seq[name]},\n"
                tfile.write(code)
            for name in seq_names:
                code = f"{TAB*2}.{name} = {{{seq[name][0]}, {seq[name][1]}}},\n"
                tfile.write(code)
            code = f"{TAB*2}.hitbox = {{{seq['hitbox']}}},\n"
            tfile.write(code)
            code = f"{TAB*2}.sheet = {seq['sheet']},\n"
            tfile.write(code)
            tfile.write(f"{TAB}}},\n")
        tfile.write(f"}};\n")
        tfile.write("\n" + CPP_BODY + "\n\n")


def write_header(file_path: str):

    with open(file_path, "w") as tfile:
        tfile.write(COPYRIGHT_NOTICE + "\n\n")
        tfile.write("//////////////////////////////////////////////////\n")
        tfile.write("// autogenerated\n\n")
        tfile.write("#pragma once\n\n")
        tfile.write("#include <cstdint>\n")
        tfile.write('#include "rect.h"\n\n')
        tfile.write('#include "color.h"\n')
        tfile.write("\n".join(defines) + "\n\n")
        tfile.write("\n".join(boss_types) + "\n\n")
        tfile.write(HEADER_STRUCT)
        tfile.write("\nconst bossData_t *getBossData(const uint8_t type);")
        tfile.write(
            "\nconst sprite_hitbox_t *getHitbox(const int sheet, const int frameID);\n"
        )


attr_names = [
    "speed",
    "speed_anime",
    "hp",
    "type",
    "score",
    "damage",
    "flags",
    "path",
    "bullet",
    "bullet_rate",
    "bullet_algo",
    "bullet_sound",
    "bullet_timeout",
    "attack_sound",
    "distance_chase",
    "distance_pursuit",
    "distance_attack",
    "is_goal",
    "show_details",
    "color_hp",
    "color_name",
    "aims",
]
seq_names = ["moving", "attack", "hurt", "death", "idle"]
misc_names = ["hitbox", "sheet"]
composite_fields = ["bullet", "distance", "speed", "color"]
all_fields = ["name"] + attr_names + seq_names + misc_names
in_file = "bosses.ini"

print(f"parsing {in_file}")

with open(in_file) as sfile:
    data = sfile.read()

boss_types = []
g_vars = []
defines = []
all_seqs = []
seq = {}
sheet = 0
all_frames: dict[int, Frame] = {}
hitbox_index = []


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
        # general config
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
        # boss config
        item_name = e[0]
        if item_name in composite_fields:
            if len(e) == 1:
                print(f"missing value on line {line}: {t}")
            elif len(e) > 1:
                for i in range(1, len(e)):
                    if "=" in e[i]:
                        v = e[i].split("=")
                        if len(v) != 2:
                            print(f"too many separators on line {line} for: {e[i]}")
                            continue
                        elif len(v[0].strip()) == 0:
                            print(f"empty suffix on {line} for: {e[i]}")
                            continue
                        elif len(v[1].strip()) == 0:
                            print(f"empty value on {line} for: {e[i]}")
                            continue
                        seq[f"{item_name}_{v[0]}"] = v[1]
                    else:
                        seq[item_name] = e[i]
            continue
        elif item_name == "hitbox":
            if len(e) == 5:
                seq[item_name] = " ".join(e[1:] + [", 0"])
            else:
                print(f"{item_name} on line {line} must have 4 dimensions: {t}")
            continue
        if item_name not in seq_names and len(e) != 2:
            print(f"missing qualifier on line {line}: {t}")
            continue
        elif len(e) != 3 and len(e) != 2:
            print(f"seq must have 1 or 2 qualifiers on line {line}: {t}")
            continue
        if item_name in seq_names:
            seq_name = item_name
            hitbox_file = e[2] if len(e) == 3 else ""
            if e[1][0] == "@":
                ref_seq = e[1][1:]
                vars = copy.deepcopy(seq[ref_seq])
                if hitbox_file:
                    print(f"hitbox file: `{hitbox_file}` on line {line} ignored")
            else:
                if hitbox_file:
                    handle_hitboxes(hitbox_file, sheet, frame)
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
                if not "aims" in seq:
                    print(f"aims must be defined before animation on line {line}")
                    exit(EXIT_FAILURE)
                # print(f'aims: {seq["aims"]}')
                aims = int(seq["aims"])
                frame += frame_count * aims
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


for seq in all_seqs:
    if not sanity_check(seq):
        print("please fix config")
        exit(EXIT_FAILURE)
    name = clean_name(seq["name"])
    boss_types.append(f"#define BOSS_{name} {seq['type']}")

g_vars.append(f"constexpr size_t BOSS_COUNT = {len(all_seqs)};")
g_vars.append(f"constexpr size_t HITBOX_COUNT = {len(all_frames)};")
g_vars.append(f"constexpr size_t SHEET_SPACER = {SHEET_SPACER};")


write_header(H_FILE)
write_cpp(CPP_FILE)

copyfile(H_FILE, "../../src")
copyfile(CPP_FILE, "../../src")


exit(EXIT_SUCCESS)
