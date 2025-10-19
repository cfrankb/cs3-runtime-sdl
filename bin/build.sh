#!/bin/bash
set -e

show_help () {
    printf '\navailable targets:\n\n'
    echo "sdl3      "
    echo "emsdl3    "
    echo "mingw     "
}

APP_NAME=cs3-runtime
if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "sdl3" ]] ; then
    BPATH=build/std
    echo "BUILD PATH: ${BPATH}"
    cmake -B ${BPATH} -DCMAKE_BUILD_TYPE=Release
    cmake --build ${BPATH}
elif [[ "$1" == "emsdl3" ]] ; then
    BPATH=build/ems
    TARGET=${BPATH}/${APP_NAME}.html
    echo "BUILD PATH: ${BPATH}"
    # preload only xm files/ download ogg files from web
    mkdir -p ${BPATH}/ems_data && cp -r data/* ${BPATH}/ems_data && mv ${BPATH}/ems_data/musics/*.ogg ${BPATH}
    ls ${BPATH}/ems_data -l
    emcmake cmake -B ${BPATH} -DCMAKE_BUILD_TYPE=Release
    cmake --build ${BPATH}  -- VERBOSE=1   && echo "To run: emrun --hostname 0.0.0.0 ${TARGET}"
elif [[ "$1" == "mingw" ]] ; then
    BPATH=build/mingw
    cmake -DCMAKE_TOOLCHAIN_FILE=packages/cmake/mingw.toolchain.cmake -DIS_MINGW=ON -B ${BPATH} -DCMAKE_BUILD_TYPE=Release
    cmake --build ${BPATH} -- VERBOSE=1
elif [[ "$1" == "run_ems" ]] ; then
    emrun --hostname 0.0.0.0 build/ems/cs3-runtime.html
elif [[ "$1" == "clean" ]] ; then
    rm -rf build
elif [[ "$1" == "tests" ]] ; then
    # rm -rf build
    cmake -S . -B build
    cmake --build build --target cs3-tests
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi