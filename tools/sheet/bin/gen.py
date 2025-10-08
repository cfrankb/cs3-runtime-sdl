import ntpath
import os


def prepare_deps(deps, fname):
    lines = []
    basename = ntpath.basename(fname)
    name = basename.replace(".cpp", "")
    fname_h = fname.replace(".cpp", ".h")
    ref = fname
    if os.path.isfile(fname_h):
        ref += f" {fname_h}"
    lines.append(f"$(BPATH)/{name}$(EXT): {ref}")
    lines.append(f"\t$(CXX) $(CXXFLAGS) -c $< -o $@")
    deps.append(f"{name}")
    return "\n".join(lines)


def get_deps_blocks():
    deps = []
    deps_blocks = ["all: $(TARGET)"]
    files = [
        "src/main.cpp",
        "../../src/strhelper.cpp",
        "../../src/logger.cpp",
        # "../../src/map.cpp",
        # "../../src/maparch.cpp",
        # "../../src/level.cpp",
        "../../src/shared/FileWrap.cpp",
        "../../src/shared/helper.cpp",
        "../../src/states.cpp",
        "../../src/statedata.cpp",
        # "../../src/tilesdata.cpp",
        "../../src/shared/Frame.cpp",
        "../../src/shared/FrameSet.cpp",
        "../../src/shared/FileMem.cpp",
        "../../src/shared/DotArray.cpp",
        "../../src/shared/PngMagic.cpp",
    ]

    for f in files:
        deps_blocks.append(prepare_deps(deps, f))

    objs = " ".join(f"$(BPATH)/{x}$(EXT)" for x in deps)
    lines = []
    lines.append(f"$(TARGET): $(DEPS)")
    lines.append(f"\t$(CXX) $(CXXFLAGS) $(DEPS) $(LIBS) -o $@")
    deps_blocks.append("\n".join(lines))
    lines = []
    lines.append("clean:")
    lines.append("\trm -f $(BPATH)/*")
    deps_blocks.append("\n".join(lines))
    return deps_blocks, objs


def main():
    vars = [
        "CXX=g++ --std=c++20 -Wall -Wextra",
        "CXXFLAGS=-g  -DQT_NOT_WANTED",
        "LIBS=-lm -lz",
        "BPATH=build",
        "EXT=.o",
        "TARGET=$(BPATH)/sheet",
    ]

    deps_blocks, objs = get_deps_blocks()
    vars.append(f"DEPS={objs}")

    with open("makefile", "w") as tfile:
        tfile.write("\n".join(vars) + "\n\n")
        tfile.write("\n\n".join(deps_blocks))


main()
