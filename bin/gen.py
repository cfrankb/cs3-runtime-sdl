#!/usr/bin/python
import glob
import ntpath

def prepare_deps(deps, fname):
    lines = []
    basename = ntpath.basename(fname)
    name = basename.replace('.cpp', '')
    lines.append(f'$(BPATH)/{name}.o: {fname}')
    lines.append(f'\t$(CC) -c {fname} $(INC) -o $@')
    deps.append(f'{name}')
    return '\n'.join(lines)


def get_deps_blocks():
    paths = ['src/*.cpp', "src/**/*.cpp"]
    deps_blocks = ["all: $(TARGET)"]
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            if 'main.cpp' not in f:
                deps_blocks.append(prepare_deps(deps, f))

    objs = ' '.join(f'$(BPATH)/{x}.o' for x in deps)
    lines = []
    lines.append(f'$(TARGET): src/main.cpp $(DEPS)')
    lines.append(f'\t$(CC) $(ARGS) src/main.cpp $(DEPS) $(LIBS) $(PARGS) -o $@')
    deps_blocks.append('\n'.join(lines))
    lines = []
    lines.append('clean:')
    lines.append("\trm -f $(BPATH)/*")
    deps_blocks.append('\n'.join(lines))
    return deps_blocks, objs


def main():
    vars = [
        'CC=g++', 'INC=', 'LIBS=-lSDL2 -lz',
        'ARGS=-O3',
        'PARGS=',
        'BPATH=build', 'BNAME=cs3-runtime', 'TARGET=$(BPATH)/$(BNAME)'
        ]
    vars = [
        'CC=emcc', 'INC=-Isrc/zlib/', 'LIBS=', 
        'ARGS=-s USE_SDL=2 -s USE_ZLIB=1 -s WASM=1 --emrun -DWASM -O2',
        'PARGS=--preload-file data',
        'BPATH=build', 'BNAME=cs3v2.html', 'TARGET=$(BPATH)/$(BNAME)'
        ]
    # emmake make

    deps_blocks, objs = get_deps_blocks()
    vars.append(f'DEPS={objs}')

    with open('Makefile', 'w') as tfile:
        tfile.write('\n'.join(vars) + "\n\n")
        tfile.write('\n\n'.join(deps_blocks))


main()