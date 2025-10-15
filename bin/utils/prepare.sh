#!/bin/bash
set -e

show_help () {
    printf '\navailable targets:\n\n'
    echo "local      "
    echo "itchio     "
}

APP_NAME=cs3-runtime
echo "package ${APP_NAME} for web distribution"

if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "local" ]] ; then
    BPATH=./build
    OUT_PATH=${BPATH}/local
    TARGET=../cs3-local.zip
    mkdir -p ${OUT_PATH}
    rm -f ${OUT_PATH}/*
    cp ${BPATH}/${APP_NAME}.* ${OUT_PATH}
    gzip  ${OUT_PATH}/*.data ${OUT_PATH}/*.wasm
    mv ${OUT_PATH}/${APP_NAME}.data.gz ${OUT_PATH}/${APP_NAME}.data
    mv ${OUT_PATH}/${APP_NAME}.wasm.gz ${OUT_PATH}/${APP_NAME}.wasm
    cp data/musics/*.ogg ${OUT_PATH}
    cd ${OUT_PATH}
    rm -f ${TARGET}
    zip ${TARGET} *
elif [[ "$1" == "itchio" ]] ; then
    BPATH=./build
    OUT_PATH=${BPATH}/itchio
    TARGET=../cs3-itchio.zip
    mkdir -p ${OUT_PATH}
    rm -f ${OUT_PATH}/*
    cp ${BPATH}/${APP_NAME}.* ${OUT_PATH}
    mv ${OUT_PATH}/${APP_NAME}.html ${OUT_PATH}/index.html
    cp data/musics/*.ogg ${OUT_PATH}
    cd ${OUT_PATH}
    rm -f ${TARGET}
    zip ${TARGET} *
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi