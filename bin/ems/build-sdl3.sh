#!/bin/bash
set -e
cd external/SDL3
rm -rf build-ems
mkdir -p build-ems/
cd build-ems
emcmake cmake .. \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../../../local/ems
emmake make -j$(nproc)
emmake make install

