#!/bin/bash
set -e
git submodule add https://github.com/libsdl-org/SDL.git external/SDL3
git submodule add -b sdl2-api-on-sdl2 https://github.com/libsdl-org/SDL_mixer.git external/SDL3_mixer
git submodule add https://github.com/libxmp/libxmp.git external/libxmp
git submodule add https://github.com/madler/zlib.git external/zlib
git submodule add https://github.com/xiph/ogg.git external/ogg
git submodule add https://github.com/xiph/vorbis.git external/vorbis
git submodule update --init --recursive


