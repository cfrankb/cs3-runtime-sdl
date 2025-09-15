#!/bin/bash
set -e
cd external/SDL3_mixer
rm -rf build-mingw
mkdir -p build-mingw && cd build-mingw
LIBXMP_PATH=../../../local/mingw/lib/libxmp.dll.a
ls -l $LIBXMP_PATH

cmake .. \
  -G Ninja \
  -Dlibxmp_INCLUDE_PATH=../../libxmp/include \
  -Dlibxmp_LIBRARY=$LIBXMP_PATH \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DSDL3_DIR="$(pwd)/../../../local/mingw/lib/cmake/SDL3" \
  -DCMAKE_PREFIX_PATH="$(pwd)/../../../local/mingw/lib" \
  -DCMAKE_INSTALL_PREFIX=../../../local/mingw \
  -DSDLMIXER_FLAC_LIBFLAC=OFF \
  -DSDLMIXER_FLAC_DRFLAC=OFF \
  -DSDLMIXER_MP3=OFF \
  -DSDLMIXER_MP3_MPG123=OFF \
  -DSDLMIXER_MP3_DRMP3=OFF \
  -DSDLMIXER_MIDI_FLUIDSYNTH=OFF \
  -DSDLMIXER_MIDI=OFF

cmake --build . --parallel  --verbose
cmake --install . --prefix ../../../local/mingw