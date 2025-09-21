#!/usr/bin/python
import glob
import ntpath
import os
import sys
from pathlib import Path

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


def get_deps_blocks(paths, excluded, run_cmd, libs_steps):
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
    lines.append("")
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
    if libs_steps:
        lines.append("build_libs:")
        lines.append("\t" + "\n\t".join(libs_steps) + "\n")

    deps_blocks.append("\n".join(lines))
    return deps_blocks, objs


options = ["sdl3", "emsdl3", "mingw32-sdl3"]

help_text = f"""
makefile generator

possible values are:
    {', '.join(options)}

    -t     build the unit tests
    -s     strip debug symbols

examples:

python bin/gen.py sdl3
python bin/gen.py emsdl3
python bin/gen.py mingw32-sdl3

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
    libs_steps = []
    python_cmd = ntpath.basename(sys.executable)
    Path("build").mkdir(parents=True, exist_ok=True)

    #################################################
    ##  SDL3
    if params.action == "sdl3":
        prefix = "local/std"
        run_cmd = f"LD_LIBRARY_PATH={prefix}/lib:$LD_LIBRARY_PATH " + run_cmd
        vars = [
            "CXX=g++",
            f"INC=-I{prefix}/include",
            f"LDFLAGS={strip}",
            f"LIBS=-L{prefix}/lib -lSDL3_mixer -lz -lxmp -lSDL3",
            "CXXFLAGS=-O3 -Wall -Wextra",
            "BPATH=build",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            'PREBUILD=echo "building game"',
        ]
        libs_steps = [
            "git submodule update --init --recursive",
            "bin/std/build-sdl3.sh",
            "bin/std/build-sdl3-mixer.sh",
        ]
    ##################################################
    ##  EMSDL3
    elif params.action == "emsdl3":
        bname = "cs3v2.html"
        prefix = "local/ems"
        run_cmd = "emrun --hostname 0.0.0.0 $(TARGET)"
        if params.tests:
            print("tests unsupported for this config")
        vars = [
            "CXX=em++",
            f"INC=-I{prefix}/include",
            f"LIBS=-lopenal -lidbfs.js -L{prefix}/lib -lSDL3 -lSDL3_mixer -lxmp-lite",
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
                -sEXPORTED_FUNCTIONS=_malloc,_free,_main
            endef"""
            ),
            "BPATH=build",
            f"BNAME={bname}",
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
        libs_steps = [
            "git submodule update --init --recursive",
            f"{python_cmd} bin/ems/patch-xmp.py",
            "bin/ems/build-xmp.sh",
            "bin/ems/build-sdl3.sh",
            "bin/ems/build-sdl3-mixer.sh",
        ]
    ###################################
    ## MINGW32-SDL3
    elif params.action == "mingw32-sdl3":
        run_cmd += ".exe"
        arch = "x86_64-w64"
        paths += ["src/*.rc"]
        prefix = params.prefix if params.has_prefix() else "local/mingw"
        vars = [
            f"WINDRES={arch}-mingw32-windres",
            f"CXX={arch}-mingw32-g++",  # -fno-exceptions -fno-rtti
            f"INC=-I{prefix}/include",
            f"LDFLAGS=-L{prefix}/lib -Wl,-t -mwindows {strip}",
            # "LIBS=-static-libstdc++ -static-libgcc -Wl,-Bstatic "
            "LIBS=-static-libstdc++ -static-libgcc -lwinpthread -lmingw32 "
            "-lvorbisfile -lvorbis -logg -lz "
            # "-Wl,-Bdynamic "
            "-lxmp -lSDL3 -lSDL3_mixer "
            "-lm -ldinput8 -ldxguid -ldxerr8 -luser32 "
            "-lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 "
            "-lversion -luuid -lws2_32 -lsetupapi -lhid",
            "CXXFLAGS=-O3 -pthread -std=c++17",
            "BPATH=build",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            'PREBUILD=echo "building game"',
        ]
        libs_steps = [
            "git submodule update --init --recursive",
            "bin/mingw/build-ogg.sh",
            "bin/mingw/build-vorbis.sh",
            "bin/mingw/build-zlib.sh",
            "bin/mingw/build-xmp.sh",
            "bin/mingw/build-sdl3.sh",
            "bin/mingw/build-sdl3-mixer.sh",
            "bin/mingw/copydll.sh",
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

    print("type `make build_libs` to create the libraries.")
    print("type `make` to generare the executable.")
    print("type `make clean` to delete the content of the build folder.")

    deps_blocks, objs = get_deps_blocks(paths, excluded, run_cmd, libs_steps)
    vars.append(f"DEPS={objs}")
    vars.append(f"EXT={ext}")
    with open("Makefile", "w") as tfile:
        tfile.write("\n".join(vars) + "\n\n")
        tfile.write("\n\n".join(deps_blocks))

    return EXIT_SUCCESS


exit(main())
