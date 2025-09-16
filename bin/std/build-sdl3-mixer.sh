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
  -DSDLMIXER_FLAC_LIBFLAC=OFF \
  -DSDLMIXER_FLAC_DRFLAC=OFF \
  -DSDLMIXER_MP3=OFF \
  -DSDLMIXER_MP3_MPG123=OFF \
  -DSDLMIXER_MP3_DRMP3=OFF \
  -DSDLMIXER_MIDI_FLUIDSYNTH=OFF \
  -DSDLMIXER_MIDI=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL3_DIR="$(pwd)/../../../local/std/lib/cmake/SDL3"
cmake --build . --config Release
cmake --install . --prefix ../../../local/std