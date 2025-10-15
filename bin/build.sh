#!/bin/bash
set -e

show_help () {
    printf '\navailable targets:\n\n'
    echo "sdl3      "
    echo "emsdl3    "
}

APP_NAME=cs3-runtime
if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "sdl3" ]] ; then
    BPATH=build/std
    echo "BUILD PATH: ${BPATH}"
    #exit 0
    cmake -B ${BPATH} -DCMAKE_BUILD_TYPE=Release
    cmake --build ${BPATH}
elif [[ "$1" == "emsdl3" ]] ; then
    BPATH=build/ems
    TARGET=${BPATH}/${APP_NAME}.html
    echo "BUILD PATH: ${BPATH}"
    #exit 0
    # preload only xm files/ download ogg files from web
    mkdir -p ${BPATH}/ems_data && cp -r data/* ${BPATH}/ems_data && mv ${BPATH}/ems_data/musics/*.ogg ${BPATH}
    ls ${BPATH}/ems_data -l
    emcmake cmake -S external/libxmp -B ${BPATH}/build_libxmp -DBUILD_SHARED_LIBS=OFF
    cmake --build ${BPATH}/build_libxmp
    emcmake cmake -B ${BPATH} -DCMAKE_BUILD_TYPE=Release
    cmake --build ${BPATH} && echo "To run: emrun --hostname 0.0.0.0 ${TARGET}"
elif [[ "$1" == "run_ems" ]] ; then
    emrun --hostname 0.0.0.0 build/cs3-runtime.html
elif [[ "$1" == "clean" ]] ; then
    rm -rf build
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi