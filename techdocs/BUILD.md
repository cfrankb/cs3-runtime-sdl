# Build Steps

### Web browser version

This version requires SDL3, SDL3-mixer, libXMP, zlib and Emscripten.

First install [emscripten](https://emscripten.org/index.html)

If your system uses `python3` rather than `python`, update
the snippets accordingly.

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

## Arch Linux / Ubuntu

This version requires that libXMP and zlib be installed on your system.

#### Run these commands

```
$ python bin/gen.py sdl3
$ make build_libs
$ make
```

## Mingw32 on Linux (manual)

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
