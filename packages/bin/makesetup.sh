#!/bin/bash
BPATH=build/mingw
DEST=setup

rm -rf setup/*
mkdir -p setup/data
mkdir -p setup/legal
cp -R data/* setup/data
cp ${BPATH}/*.exe setup
cp -R techdocs/legal/* setup/legal
cp techdocs/*.md setup/legal
cp LICENSE setup
PTHREAD_DLL=$(find /usr/x86_64-w64-mingw32/ | grep libwinpthread-1.dll | head -n 1)
echo PTHREAD_DLL=$PTHREAD_DLL
cp $PTHREAD_DLL setup
#cp local/mingw/bin/*.dll setup
cp ${BPATH}/SDL3/*.dll ${DEST}
cp ${BPATH}/SDL3_mixer/*.dll ${DEST}
cp ${BPATH}/SDL3/*.dll ${DEST}
cp ${BPATH}/zlib/*.dll ${DEST}
#cp ${BPATH}/build_libxmp/*.dll ${DEST}
ls -l ${DEST}