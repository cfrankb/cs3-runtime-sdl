#!/bin/bash
set -e

show_help () {
    printf '\navailable targets:\n\n'
    echo "start  "
    echo "build  "
    echo "rebuild  "
    echo "run    "
    echo "run-root "
}

IMAGE_PATH=packages/docker/dockerfile-clang-xwin
IMAGE_NAME=build-clang-xwin

if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "start" ]] ; then
    sudo systemctl start docker.service
elif [[ "$1" == "rebuild" ]] ; then
    sudo docker build --no-cache -t ${IMAGE_NAME} -f ${IMAGE_PATH} .
elif [[ "$1" == "build" ]] ; then
    sudo docker build -t ${IMAGE_NAME} -f ${IMAGE_PATH} .
elif [[ "$1" == "run" ]] ; then
    sudo docker run \
      --rm \
      --init \
      -v "$PWD":/cs3-runtime-sdl \
      -v /home:/home \
      -v /etc/passwd:/etc/passwd:ro \
      -v /etc/group:/etc/group:ro \
      -w /cs3-runtime-sdl \
      -e HOME="$HOME" \
      -u "$(id -u):$(id -g)" \
      -v /tmp:/tmp \
      -it \
      ${IMAGE_NAME} \
      /bin/bash
elif [[ "$1" == "run-root" ]] ; then
    sudo docker run \
      --rm \
      --init \
      -v "$PWD":/cs3-runtime-sdl \
      -v /home:/home \
      -w /cs3-runtime-sdl \
      -e HOME="$HOME" \
      -v /tmp:/tmp \
      -it \
      ${IMAGE_NAME} \
      /bin/bash
elif [[ "$1" == "help" ]] ; then
    cmake --build build/msvc --target help
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi