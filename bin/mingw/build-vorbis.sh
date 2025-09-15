#!/bin/bash
set -e
cd external/vorbis
./autogen.sh
mkdir -p build-mingw
cd build-mingw
PREFIX=$(pwd)/../../../local/mingw
../configure --host=x86_64-w64-mingw32 \
    --prefix=$PREFIX \
    PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
make -j$(nproc)
make install