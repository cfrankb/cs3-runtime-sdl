#!/bin/bash
set -e
cd external/SDL3
rm -rf build-std
mkdir -p build-std/
cd build-std
cmake .. \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --prefix ../../../local/std

