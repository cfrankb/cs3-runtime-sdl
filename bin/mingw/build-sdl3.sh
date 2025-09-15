#!/bin/bash
set -e
cd external/SDL3
#rm -rf build-mingw
mkdir -p build-mingw/
cd build-mingw
cmake .. \
  -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
cmake --install . --prefix ../../../local/mingw
