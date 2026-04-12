#!/bin/bash
set -e

show_help () {
    printf '\navailable targets:\n\n'
    echo "start  "
    echo "build  "
    echo "run    "
}

if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "start" ]] ; then
    sudo systemctl start docker.service
elif [[ "$1" == "build" ]] ; then
    sudo docker build -t sniper-sdk-mingw -f  packages/docker/dockerfile-sniper-sdk-mingw .
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
      sniper-sdk-mingw \
      /bin/bash
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown target: $1"
    show_help
fi