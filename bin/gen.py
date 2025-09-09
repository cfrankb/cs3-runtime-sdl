#!/usr/bin/python
import glob
import ntpath
import os
import sys

EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def strip_ext(s):
    i = s.rfind(".")
    return s[0:i]


def prepare_deps(deps, fname):
    lines = []
    basename = ntpath.basename(fname)
    ref = fname
    if fname.endswith(".cpp"):
        name = strip_ext(basename)
        fname_h = strip_ext(fname) + ".h"
        if os.path.isfile(fname_h):
            ref += f" {fname_h}"
        lines.append(f"$(BPATH)/{name}$(EXT): {ref}")
        lines.append(f"\t$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@")
        deps.append(f"{name}$(EXT)")
    elif fname.endswith(".rc"):
        name = strip_ext(basename) + ".res"
        lines.append(f"$(BPATH)/{name}: {ref}")
        lines.append(f"\t$(WINDRES) $< -O coff -o $@")
        deps.append(f"{name}")
    else:
        print(f"no receipe for unhandled file: {fname}\n")
    return "\n".join(lines)


def get_deps_blocks(paths, excluded, run_cmd):
    deps_blocks = ["all: $(TARGET)"]
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            if ntpath.basename(f) not in excluded:
                deps_blocks.append(prepare_deps(deps, f))

    objs = " ".join(f"$(BPATH)/{x}" for x in deps)
    lines = []
    lines.append(f"$(TARGET): $(DEPS)")
    lines.append(
        f"\t$(PREBUILD) && $(CXX) $(CXXFLAGS) $(DEPS) $(LDFLAGS) $(LIBS) -o $@"
        f'&& echo "to run app: {run_cmd}"'
    )
    deps_blocks.append("\n".join(lines))
    lines = []
    lines.append("clean:")
    lines.append("\trm -rf $(BPATH)/*")
    lines.append("")
    lines.append("retry:")
    lines.append("\trm -rf $(TARGET)")
    lines.append("")
    lines.append("run:")
    lines.append(f"\t{run_cmd}")
    lines.append("")
    lines.append("count:")
    lines.append("\tfind src  -type f -print0 | xargs -0 wc -l")
    lines.append("\tcloc src")
    lines.append("")
    deps_blocks.append("\n".join(lines))
    return deps_blocks, objs


options = ["sdl3", "sdl2-static", "emsdl3", "mingw32-sdl2"]

help_text = f"""
makefile generator

possible values are:
    {', '.join(options)}

    -t     build the unit tests
    -s     strip debug symbols

examples:

python bin/gen.py sdl3
python bin/gen.py emsdl3
python bin/gen.py mingw32-sdl2

""".strip()


class Params:
    def __init__(self, argv):
        self.tests = False
        self.strip = False
        self.action = ""
        self.prefix = ""
        for i in range(1, len(argv)):
            if argv[i] == "-t":
                self.tests = True
            if argv[i] == "-s":
                self.strip = True
            elif self.action == "":
                self.action = argv[i]
            elif self.prefix == "":
                self.prefix = argv[i]
            else:
                print(f'extra arg at pos {i} ignored: "{argv[i]}"')

    def has_prefix(self):
        return self.prefix != ""


def strip_padding(strx):
    return strx.replace(" " * 12, "")


def main():
    params = Params(sys.argv)
    paths = ["src/*.cpp", "src/**/*.cpp", "src/**/**/*.cpp"]
    excluded = []
    bname = "cs3-runtime"
    strip = ""
    ext = ".o"
    run_cmd = "$(TARGET)"
    if params.tests:
        excluded += ["main.cpp"]
        paths += ["tests/*.cpp"]
        bname = "tests"
    if params.strip:
        strip = "-s "

    #################################################
    ##  SDL3
    if params.action == "sdl3":
        vars = [
            "CXX=g++",
            "INC=-Ilocal/std/SDL3/include -Ilocal/std/SDL3_mixer/include",
            f"LDFLAGS={strip}",
            "LIBS=-Llocal/std/lib -lSDL3_mixer -lz -lxmp -lSDL3",
            "CXXFLAGS=-O3 -Wall -Wextra",
            "BPATH=build",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            'PREBUILD=echo "building game"',
        ]
    ##################################################
    ##  EMSDL3
    elif params.action == "emsdl3":
        run_cmd = "emrun --hostname 0.0.0.0 $(TARGET)"
        if params.tests:
            print("tests unsupported for this config")
        vars = [
            "CXX=em++",
            "INC=-Ilocal/ems/include",
            "LIBS=-lopenal -lidbfs.js -Llocal/ems/lib -lSDL3 -lSDL3_mixer -lxmp-lite",
            strip_padding(
                """define CXXFLAGS
                -sUSE_SDL=0 \\
                -sUSE_ZLIB=1 \\
                -O3
            endef"""
            ),
            strip_padding(
                f"""define LDFLAGS
                --preload-file build/data@data \\
                --shell-file src/template/body.html \\
                --emrun -O3 {strip} \\
                -s ASSERTIONS=1 \\
                -s MODULARIZE=1 \\
                -s EXPORT_NAME="MyApp" \\
                -sWASM=1 \\
                -sALLOW_MEMORY_GROWTH  \\
                -sINITIAL_MEMORY=64MB \\
                -sASYNCIFY \\
                -sEXPORTED_RUNTIME_METHODS=ccall,cwrap \\
                -sEXPORTED_FUNCTIONS=_mute,_savegame,_main,_malloc,_free
            endef"""
            ),
            "BPATH=build",
            "BNAME=cs3v2.html",
            "TARGET=$(BPATH)/$(BNAME)",
            strip_padding(
                """define PREBUILD
                cp data/musics/*.ogg build && \\
                rm -rf build/data && \\
                mkdir -p build/data && \\
                cp -R data/* build/data && \\
                rm build/data/musics/*.ogg
            endef"""
            ),
        ]
    ############################
    ## SDL2-STATIC
    elif params.action == "sdl2-static":
        prefix = params.prefix if params.has_prefix() else "/usr/local"
        vars = [
            "CXX=g++",
            f"INC=-I{prefix}/include",
            f"LDFLAGS=-L{prefix}/lib {strip}",
            "LIBS=-static -lSDL2_mixer -lSDL2 -lSDL2main -lz -lxmp",
            "CXXFLAGS=-O3",
            "BPATH=build",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            'PREBUILD=echo "building game"',
        ]
    ###################################
    ## MINGW32-SDL2
    elif params.action == "mingw32-sdl2":
        arch = "x86_64-w64"
        paths += ["src/*.rc"]
        prefix = params.prefix if params.has_prefix() else "/sdl2-windows"
        vars = [
            f"WINDRES={arch}-mingw32-windres",
            f"CXX={arch}-mingw32-g++",
            f"INC=-I{prefix}/include",
            f"LDFLAGS=-L{prefix}/lib -Wl,-t {strip}",
            "LIBS=-static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -lmingw32 -lxmp -lSDL2main -lSDL2 -lSDL2_mixer "
            "-lvorbisfile -lvorbis -logg -lz -Wl,-Bdynamic -lm -ldinput8 -ldxguid -ldxerr8 -luser32 "
            "-lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lws2_32 -lsetupapi -lhid",
            "CXXFLAGS=-O3 -pthread -std=c++17",
            "BPATH=build",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            'PREBUILD=echo "building game"',
        ]
    ###################################################
    ## unhandled cases
    elif params.action not in options:
        print(help_text)
        return EXIT_FAILURE
    else:
        print(f"action {params.action} unhandled")
        print(help_text)
        return EXIT_FAILURE

    print("type `make` to generare binary.")
    print("type `make clean` to delete the content of the build folder.")

    deps_blocks, objs = get_deps_blocks(paths, excluded, run_cmd)
    vars.append(f"DEPS={objs}")
    vars.append(f"EXT={ext}")
    with open("Makefile", "w") as tfile:
        tfile.write("\n".join(vars) + "\n\n")
        tfile.write("\n\n".join(deps_blocks))

    return EXIT_SUCCESS


exit(main())
