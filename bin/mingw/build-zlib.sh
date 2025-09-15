#!/bin/bash
set -e
cd external/zlib
PREFIX=../../local/mingw
ls -l $PREFIX
make -f win32/Makefile.gcc \
  PREFIX=x86_64-w64-mingw32- \
  BINARY_PATH=$PREFIX/bin \
  INCLUDE_PATH=$PREFIX/include \
  LIBRARY_PATH=$PREFIX/lib

cp zlib.h zconf.h $PREFIX/include
cp libz.a libz.dll.a $PREFIX/lib
cp *.dll $PREFIX/bin