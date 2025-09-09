#!/bin/bash
set -e
cd external/SDL3_mixer
rm -rf build-ems
mkdir -p build-ems && cd build-ems
ls -l ../../libxmp/include
LIBXMP_PATH=../../../local/ems/lib/libxmp-lite.a
echo $LIBXMP_PATH
ls -l $LIBXMP_PATH
emcmake cmake .. \
  -Dlibxmp_INCLUDE_PATH=../../libxmp/include \
  -Dlibxmp_LIBRARY=$LIBXMP_PATH \
  -DSDL3MIXER_VORBIS=ON \
  -DSDL3MIXER_MP3=OFF \
  -DSDL3MIXER_FLAC=OFF \
  -DSDL3MIXER_MOD=ON \
  -DSDL3MIXER_MIDI=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL3_DIR="$(pwd)/../../../local/ems/lib/cmake/SDL3" \
  -DCMAKE_PREFIX_PATH=$(pwd)/../../../local/ems/lib \
  -DCMAKE_INSTALL_PREFIX=../../../local/ems
emmake make -j$(nproc)
emmake make install