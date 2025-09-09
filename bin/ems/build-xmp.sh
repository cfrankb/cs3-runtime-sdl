#!/bin/bash
set -e
cd external/libxmp
emmake make -f Makefile.lite
LIBXMP_PATH=$(find . | grep libxmp-lite.a | head -n 1)
DEST_LIB=../../local/ems/lib
mkdir -p $DEST_LIB
cp $LIBXMP_PATH $DEST_LIB

#autoconf