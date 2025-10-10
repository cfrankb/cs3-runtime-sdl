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


def prepare_deps(deps, fpath):
    lines = []
    basename = ntpath.basename(fpath)
    ref = fpath
    if fpath.endswith(".cpp"):
        name = strip_ext(basename)
        fname_h = strip_ext(fpath) + ".h"
        if os.path.isfile(fname_h):
            ref += f" {fname_h}"
        lines.append(f"$(BPATH)/{name}$(EXT): {ref}")
        lines.append(f"\t$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@")
        deps.append(f"{name}$(EXT)")
    elif fpath.endswith(".rc"):
        name = strip_ext(basename) + ".res"
        lines.append(f"$(BPATH)/{name}: {ref}")
        lines.append(f"\t$(WINDRES) $< -O coff -o $@")
        deps.append(f"{name}")
    else:
        print(f"no receipe for unhandled file: {fpath}\n")
    return "\n".join(lines)


def get_deps_blocks(paths, excluded, run_cmd, app, suffix=""):

    deps_blocks = []
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            deps_temp = []
            deps_blocks.append(prepare_deps(deps_temp, f))
            if (
                ntpath.basename(f) not in excluded
                and os.path.dirname(f) + "/" not in excluded
            ):
                deps += deps_temp

    objs = " ".join(f"$(BPATH)/{x}" for x in deps)
    lines = []
    lines.append(f"$(TARGET{suffix}): $(DEPS{suffix})")
    lines.append(
        f"\t$(PREBUILD{suffix}) && $(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@"
        f'&& echo "to run {app}: {run_cmd}"'
    )

    return ["\n".join(lines)] + deps_blocks, objs


options = ["sdl3", "emsdl3", "mingw32-sdl3"]

help_text = f"""
makefile generator

possible values are:
    {', '.join(options)}

    -s     strip debug symbols

examples:

python bin/gen.py sdl3
python bin/gen.py emsdl3
python bin/gen.py mingw32-sdl3

""".strip()


class Params:
    def __init__(self, argv):
        self.strip = False
        self.action = ""
        self.prefix = ""
        for i in range(1, len(argv)):
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


def make_auxilary_targets(test_cmd, libs_steps, help_info):
    lines = []
    lines.append("")
    lines.append("clean:")
    lines.append("\trm -rf $(BPATH)/*$(EXT)")
    lines.append("")
    lines.append("retry:")
    lines.append("\trm -rf $(TARGET) && make")
    lines.append("")
    lines.append("run:")
    lines.append(f"\t$(RUN_PREFIX) $(RUN) $(RUN_ARGS)")
    lines.append("")
    lines.append("count:")
    lines.append("\tfind src  -type f -print0 | xargs -0 wc -l")
    lines.append("\tcloc src")
    lines.append("")
    lines.append("rebuild:")
    lines.append("\trm -rf $(TARGET) && rm -rf $(BPATH)/*$(EXT) && make")
    lines.append("")
    if libs_steps:
        lines.append("build_libs:")
        lines.append("\t" + "\n\t".join(libs_steps) + "\n")
    if test_cmd:
        lines.append("run_tests:")
        lines.append(f"\t$(RUN_PREFIX) {test_cmd}\n")
        lines.append("retry_tests:")
        lines.append("\trm -rf $(TARGET_TEST) && make tests\n")
        lines.append("rebuild_tests:")
        lines.append(
            "\trm -rf $(TARGET_TEST) && rm -rf $(BPATH)/*$(EXT) && make tests\n"
        )

    if help_info:
        lines.append("help:")
        for line in help_info:
            lines.append(f'\t@echo "{line}"'.replace("`", "'"))

    return "\n".join(lines) + "\n"


def main():
    params = Params(sys.argv)
    paths = ["src/*.cpp", "src/**/*.cpp", "src/**/**/*.cpp"]
    paths += ["tests/*.cpp"]
    excluded = []
    excluded += ["tests/"]
    bname = "cs3-runtime"
    strip = ""
    ext = ".o"
    run_prefix = ""
    run_cmd = "$(TARGET)"
    test_cmd = ""
    if params.strip:
        strip = "-s "
    libs_steps = []
    python_cmd = ntpath.basename(sys.executable)
    build_path = "build"
    Path(build_path).mkdir(parents=True, exist_ok=True)

    #################################################
    ##  SDL3
    if params.action == "sdl3":
        prefix = "local/std"
        run_prefix = f"LD_LIBRARY_PATH={prefix}/lib:$LD_LIBRARY_PATH "
        test_cmd = "$(BPATH)/tests"
        vars = [
            f"RUN_PREFIX={run_prefix}",
            f"RUN={run_cmd}",
            "CXX=g++",
            f"INC=-I{prefix}/include",
            f"LDFLAGS={strip}",
            f"LIBS=-L{prefix}/lib -lSDL3_mixer -lz -lxmp -lSDL3",
            "CXXFLAGS=-O3 -Wall -Wextra",
            f"BPATH={build_path}",
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
        vars = [
            f"RUN_PREFIX=",
            f"RUN={run_cmd}",
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
            f"BPATH={build_path}",
            f"BNAME={bname}",
            "TARGET=$(BPATH)/$(BNAME)",
            strip_padding(
                """define PREBUILD
                cp packages/data/win32/cs3-runtime.ico build/favicon.ico
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
            f"RUN_PREFIX=",
            f"RUN={run_cmd}",
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
            f"BPATH={build_path}",
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

    help_info = [
        "type `make build_libs` to create the libraries.",
        "type `make` to build the executable.",
        "type `make clean` to delete the content of the build folder.",
    ]
    if test_cmd:
        help_info += [
            "type `make tests` to build tests.",
            "type `make run_tests` to run tests.",
        ]
    print("\n".join(help_info))

    deps_blocks_main, deps_main = get_deps_blocks(
        paths, excluded, run_cmd="make run", app="app", suffix=""
    )
    deps_blocks = ["all: $(TARGET)"]
    if deps_blocks_main:
        deps_blocks += deps_blocks_main[1:] + deps_blocks_main[0:1]
    vars.append(f"DEPS={deps_main}")
    if test_cmd:
        deps_blocks += ["tests: $(TARGET_TEST)"]
        deps_blocks_test, objs_test = get_deps_blocks(
            paths, ["main.cpp"], "make run_tests", app="tests", suffix="_TEST"
        )
        deps_blocks += deps_blocks_test[0:1]
        vars.append(f"DEPS_TEST={objs_test}")
        vars.append(f"TARGET_TEST=$(BPATH)/tests")
        vars.append("PREBUILD_TEST=echo 'building tests'")

        vars.append(f"EXT={ext}")
        with open("Makefile", "w") as tfile:
            tfile.write("\n".join(vars) + "\n\n")
            tfile.write("\n\n".join(deps_blocks) + "\n")
            tfile.write(make_auxilary_targets(test_cmd, libs_steps, help_info))

    return EXIT_SUCCESS


exit(main())
