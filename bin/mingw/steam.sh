#!/bin/bash
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