# Build Steps

### Web browser version

This version requires SDL3, SDL3-mixer, libXMP, zlib and Emscripten.

First install [emscripten](https://emscripten.org/index.html)

#### Build SDL3 and other external dependencies

```
$ git submodule update --init --recursive
$ python bin/ems/patch-xmp.py
$ bin/ems/build-xmp.sh
$ bin/ems/build-sdl3.sh
$ bin/ems/build-sdl3-mixer.sh
```

#### Run these commands

```
$ python bin/gen.py emsdl3
$ make
```

#### Launch the application

```
$ emrun build/cs3v2.html
```

## Arch Linux

#### Build SDL3 and other dependencies

```
$ git submodule update --init --recursive
$ bin/std/build-xmp.sh
$ bin/std/build-sdl3.sh
$ bin/std/build-sdl3-mixer.sh
```

#### Run these commands

```
$ python bin/gen.py sdl3
$ make
```

## Mingw32 on Linux (manual)

This build is outdated

#### Building the docker image

```
$ sudo docker build -t sdl2-mingw32-static . -f packages/docker/dockerfile-mingw32-static
```

#### Deleting an existing image

```
$ sudo docker rmi sdl2-mingw32-static

# also delete all caches
$ docker system prune -a
```

#### Building cs3 runtime

```
$ sudo docker run -it --rm -v "$(pwd)":/workspace sdl2-mingw32-static
$ python3 bin/gen.py mingw32-sdl2
$ make
```
