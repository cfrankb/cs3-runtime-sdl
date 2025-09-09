#!/bin/bash
set -e
cd external/SDL3_mixer
rm -rf build-std
mkdir -p build-std && cd build-std
cmake .. \
  -DSDL3MIXER_VORBIS=ON \
  -DSDL3MIXER_MP3=OFF \
  -DSDL3MIXER_FLAC=OFF \
  -DSDL3MIXER_MOD=ON \
  -DSDL3MIXER_MIDI=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --prefix ../../../local/std