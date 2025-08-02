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
    ref = fname
    if fname.endswith('.cpp'):
        name = basename[0:-4]#  '.cpp' >> ''
        fname_h = fname[0:-4] + '.h'# '.cpp' >> '.h'
        if os.path.isfile(fname_h):
            ref += f' {fname_h}'
        lines.append(f'$(BPATH)/{name}$(EXT): {ref}')
        lines.append(f'\t$(CXX) $(CXXFLAGS) -c $< $(INC) -o $@')
        deps.append(f'{name}$(EXT)')
    elif fname.endswith('.rc'):
        #windres icon.rc -O coff -o icon.res
        name = basename[0:-3]# '.rc' >> ''
        lines.append(f'$(BPATH)/{name}.res: {ref}')
        lines.append(f'\t$(WINDRES) $< -O coff -o $@')
        deps.append(f'{name}.res')
    else:
        print(f'no receipe for unhandled file: {fname}\n')
    return '\n'.join(lines)


def get_deps_blocks(extra_path, excluded):
    paths = ['src/*.cpp', "src/**/*.cpp", "src/**/**/*.cpp"] + extra_path
    deps_blocks = ["all: $(TARGET)"]
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            if ntpath.basename(f) not in excluded:
                deps_blocks.append(prepare_deps(deps, f))

    objs = ' '.join(f'$(BPATH)/{x}' for x in deps)
    lines = []
    lines.append(f'$(TARGET): $(DEPS)')
    lines.append(
        f'\t$(CXX) $(CXXFLAGS) $(DEPS) $(LDFLAGS) $(LIBS) $(PARGS) -o $@ $(TEMPLATE)' \
        f'&& echo "to run app: $(TARGET)"'
    )
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

class Params:
    def __init__(self, argv):
        self.tests = False
        self.action = ''
        self.prefix = ''
        for i in range(1, len(argv)):
            if argv[i] == '-t':
                self.tests = True
            elif self.action == '':
                self.action = argv[i]
            elif self.prefix == '':
                self.prefix = argv[i]
            else:
                print(f'extra arg at pos {i} ignored: "{argv[i]}"')

    def has_prefix(self):
        return self.prefix != ''

# https://www.reddit.com/r/C_Programming/comments/18xgs92/fixing_a_github_action_to_build_my_c_project_for/
def main():
    params = Params(sys.argv)
    extra_paths = []
    excluded = []
    bname = 'cs3-runtime'
    if params.tests:
        excluded += ['main.cpp']
        extra_paths += ['tests/*.cpp']
        bname = 'tests'

    if params.action not in options:
        print(help_text)
        return EXIT_FAILURE
    elif params.action == 'sdl2':
        vars = [
            'CXX=g++',
            'INC=',
            'LDFLAGS=',
            'LIBS=-lSDL2_mixer -lSDL2 -lSDL2main -lz -lxmp',
            'CXXFLAGS=-O3 -Wall',
            'PARGS=',
            'BPATH=build', f'BNAME={bname}', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif params.action == 'sdl2-static':
        prefix = params.prefix if params.has_prefix() else '/usr/local'
        vars = [
            'CXX=g++',
            f'INC=-I{prefix}/include',
            f'LDFLAGS=-L{prefix}/lib',
            'LIBS=-static -lSDL2_mixer -lSDL2 -lSDL2main -lz -lxmp',
            'CXXFLAGS=-O3',
            'PARGS=',
            'BPATH=build', f'BNAME={bname}', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif params.action == 'mingw32-sdl2':
        arch = 'x86_64-w64'
        extra_paths += ['src/*.rc']
        prefix = params.prefix if params.has_prefix() else '/sdl2-windows'
        vars = [
            f'WINDRES={arch}-mingw32-windres',
            f'CXX={arch}-mingw32-g++',
            f'INC=-I{prefix}/include',
            f'LDFLAGS=-L{prefix}/lib -Wl,-t',
            'LIBS=-static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -lmingw32 -lxmp -lSDL2main -lSDL2 -lSDL2_mixer -lvorbisfile -lvorbis -logg -lz -Wl,-Bdynamic -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -lws2_32 -lsetupapi -lhid',
            'CXXFLAGS=-O3 -pthread -std=c++17',
            'PARGS=',
            'BPATH=build', f'BNAME={bname}', 'TARGET=$(BPATH)/$(BNAME)', 'TEMPLATE='
        ]
        print("type `make` to generare binary.")
        ext = '.o'
    elif params.action == 'emsdl2':
        if params.tests:
            print("tests unsupported for this config")
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


    deps_blocks, objs = get_deps_blocks(extra_paths, excluded)
    vars.append(f'DEPS={objs}')
    vars.append(f'EXT={ext}')
    with open('Makefile', 'w') as tfile:
        tfile.write('\n'.join(vars) + "\n\n")
        tfile.write('\n\n'.join(deps_blocks))

    return EXIT_SUCCESS


exit(main())
