#!/bin/bash
set -e
cd external/libxmp
./autogen.sh
./configure --host=x86_64-w64-mingw32 --prefix=$(pwd)../../../local/mingw
make -j$(nproc)
make install