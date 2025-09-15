#!/bin/bash
set -e
cd external/ogg
./autogen.sh
mkdir -p build-mingw
cd build-mingw
PREFIX=$(pwd)/../../../local/mingw
../configure --host=x86_64-w64-mingw32 --prefix=$PREFIX
make -j$(nproc)
make install