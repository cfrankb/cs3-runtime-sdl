#!/usr/bin/python
import glob
import ntpath
import os
import sys

EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def prepare_deps(deps, fname):
    lines = []
    basename = ntpath.basename(fname)
    name = basename.replace('.cpp', '')
    fname_h = fname.replace('.cpp', '.h')
    ref = fname
    if os.path.isfile(fname_h):
        ref += f' {fname_h}'
    lines.append(f'$(BPATH)/{name}$(EXT): {ref}')
    lines.append(f'\t$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@')
    deps.append(f'{name}')
    return '\n'.join(lines)


def get_deps_blocks(extra_path):
    paths = ['src/*.cpp', "src/**/*.cpp", "src/**/**/*.cpp"] + extra_path
    deps_blocks = ["all: $(TARGET)"]
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            deps_blocks.append(prepare_deps(deps, f))

    objs = ' '.join(f'$(BPATH)/{x}$(EXT)' for x in deps)
    lines = []
    lines.append(f'$(TARGET): $(DEPS)')
    lines.append(
        f'\t$(CXX) $(CXXFLAGS) $(DEPS) $(LDFLAGS) $(LIBS) $(PARGS) -o $@ $(TEMPLATE)')
    deps_blocks.append('\n'.join(lines))
    lines = []
    lines.append('clean:')
    lines.append("\trm -rf $(BPATH)/*")
    lines.append("")
    lines.append('retry:')
    lines.append("\trm -rf $(TARGET)")
    lines.append("")
    lines.append('run:')
    lines.append("\t$(TARGET)")
    lines.append("")
    deps_blocks.append('\n'.join(lines))
    return deps_blocks, objs


options = ['sdl2', 'sdl2-static', 'emsdl2', 'mingw32-sdl2']

help_text = f'''
makefile generator

possible values are:
    {', '.join(options)}

examples:

python bin/gen.py sdl2
python bin/gen.py emsdl2
python bin/gen.py mingw32-sdl2

'''.strip()

# https://www.reddit.com/r/C_Programming/comments/18xgs92/fixing_a_github_action_to_build_my_c_project_for/
def main():
    extra_paths = []
    if (len(sys.argv) == 2 and sys.argv[1] not in options) or \
        (len(sys.argv) == 3 and sys.argv[1] not in ['sdl2-static', 'mingw32-sdl2']) or \
        (len(sys.argv) != 2 and len(sys.argv) != 3):
        print(help_text)
        exit(EXIT_FAILURE)
    elif sys.argv[1] == 'sdl2':
        vars = [
            'CXX=g++',
            'INC=',
            'LDFLAGS=',
            'LIBS=-lSDL2_mixer -lSDL2 -lSDL2main -lz -lxmp',
            'CXXFLAGS=-O3 -Wall',
            'PARGS=',
            'BPATH=build', 'BNAME=cs3-runtime', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif sys.argv[1] == 'sdl2-static':
        prefix = sys.argv[2] if len(sys.argv) == 3 else '/usr/local'
        vars = [
            'CXX=g++',
            f'INC=-I{prefix}/include',
            f'LDFLAGS=-L{prefix}/lib',
            'LIBS=-static -lSDL2_mixer -lSDL2 -lSDL2main -lz -lxmp',
            'CXXFLAGS=-O3',
            'PARGS=',
            'BPATH=build', 'BNAME=cs3-runtime', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif sys.argv[1] == 'mingw32-sdl2':
        prefix = sys.argv[2] if len(sys.argv) == 3 else '/sdl2-windows'
        vars = [
            'CXX=x86_64-w64-mingw32-g++',
            f'INC=-I{prefix}/include',
            f'LDFLAGS=-L{prefix}/lib -Wl,-t',
            'LIBS=-static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -lmingw32 -lxmp -lSDL2main -lSDL2 -lSDL2_mixer -lvorbisfile -lvorbis -logg -lz -Wl,-Bdynamic -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lws2_32 -lsetupapi -lhid',
            'CXXFLAGS=-O3 -pthread -std=c++17',
            'PARGS=',
            'BPATH=build', 'BNAME=cs3-runtime', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif sys.argv[1] == 'emsdl2':
        vars = [
            'CXX=em++',
            'INC=',
            'LIBS=-lopenal -lidbfs.js',
            'LDFLAGS=',
            '''CXXFLAGS=-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_ZLIB=1 -sSDL2_MIXER_FORMATS='["ogg","mod"]' -O3''',
            'PARGS=--preload-file data --emrun -O2 -sWASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sEXPORTED_FUNCTIONS=_mute,_savegame,_main',
            'BPATH=build', 'BNAME=cs3v2.html', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE=--shell-file src/template/body.html'
        ]
        print("type `emmake make` to generare binary.")
        ext = '.o'
    print("type `make clean` to delete the content of the build folder.")

    deps_blocks, objs = get_deps_blocks(extra_paths)
    vars.append(f'DEPS={objs}')
    vars.append(f'EXT={ext}')
    with open('Makefile', 'w') as tfile:
        tfile.write('\n'.join(vars) + "\n\n")
        tfile.write('\n\n'.join(deps_blocks))

    return EXIT_SUCCESS


exit(main())
