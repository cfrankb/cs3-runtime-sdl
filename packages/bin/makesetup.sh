#!/bin/bash
set -e

DEST=setup

show_help () {
    printf '\navailable targets:\n\n'
    echo "mingw         Windows/mingw"
    echo "msvc          Windows/clang/msvc)"
    echo "setup         make setup application"
}

run_common() {
    rm -rf setup/*
    mkdir -p setup/data
    mkdir -p setup/legal
    cp -R data/* setup/data
    cp ${BPATH}/*.exe setup
    cp -R techdocs/legal/* setup/legal
    rm -f setup/legal/*.odt
    cp techdocs/*.md setup/legal
    #cp LICENSE setup
    cp techdocs/legal/eula.txt setup/LICENSE
}

make_setup() {
    ls -l ${DEST}
    makensis packages/data/win32/cs3-setup.nsi
    ls -l build
}


if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "mingw" ]] ; then
    BPATH=build/mingw
    run_common
    PTHREAD_DLL=$(find /usr/x86_64-w64-mingw32/ | grep libwinpthread-1.dll | head -n 1)
    echo PTHREAD_DLL=$PTHREAD_DLL
    cp $PTHREAD_DLL ${DEST}
    ls -l ${DEST}
    #cp local/mingw/bin/*.dll setup
    ##cp ${BPATH}/SDL3/*.dll ${DEST}
    ##cp ${BPATH}/SDL3_mixer/*.dll ${DEST}
    ##cp ${BPATH}/SDL3/*.dll ${DEST}
    ##cp ${BPATH}/zlib/*.dll ${DEST}
    #cp ${BPATH}/build_libxmp/*.dll ${DEST}
elif [[ "$1" == "msvc" ]] ; then
    BPATH=build/msvc
    run_common
    cp ${BPATH}/SDL3_mixer/external/ogg-build/*.dll ${DEST}
    cp ${BPATH}/SDL3_mixer/external/vorbis-build/lib/*.dll ${DEST}
    ls -l ${DEST}
elif [[ "$1" == "setup" ]] ; then
    make_setup
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi