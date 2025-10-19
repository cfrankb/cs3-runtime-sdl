# Build Steps

If your system uses `python3` rather than `python`, update
the snippets accordingly.

## New build chain

You should first refresh the `CMakeLists.txt` for the game modules and download the git submodules.

```
$ git submodule update --init --recursive
$ python bin/refresh.py
```

### Wasm/Web version

First install [emscripten](https://emscripten.org/index.html) before running the
patcher and build script.

```
$ python bin/ems/patch.py
$ bin/build.sh emsdl3
```

Run game

```
$ emrun --hostname 0.0.0.0 build/ems/cs3-runtime.html
```

### Arch Linux / Ubuntu

This version requires that libXMP and zlib be installed on your system.

Build the game

```
$ bin/build.sh sdl3
```

Run game

```
$ build/std/cs3-runtime
```

### Mingw (linux)

Build the game

```
$ bin/build.sh mingw
$ python packages/mksetup.sh
```

Run game

```
$ wine setup/cs3-runtime
```

## Old Build Chain (deprecated)

### Web browser version

This version requires SDL3, SDL3-mixer, libXMP, zlib and Emscripten.

First install [emscripten](https://emscripten.org/index.html)

#### Run these commands

```
$ python bin/gen.py emsdl3
$ make build_libs
$ make
```

#### Launch the application

```
$ emrun build/cs3v2.html
```

### Arch Linux / Ubuntu

This version requires that libXMP and zlib be installed on your system.

#### Run these commands

```
$ python bin/gen.py sdl3
$ make build_libs
$ make
```

Run the game

```
$ make run
```

### Mingw32 on Linux (manual)

Install build dependencies

```
$ apt install mingw-w64 autoconf automake libtool pkg-config git make
```

#### Building cs3 runtime

```
$ python bin/gen.py mingw32-sdl3
$ make build_libs
$ make
```

Run the game

```
$ make run
```
