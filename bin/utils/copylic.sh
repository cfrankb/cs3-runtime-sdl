#!/bin/bash
mkdir -p docs/legal/zlib
cp external/zlib/LICENSE docs/legal/zlib
mkdir -p docs/legal/SDL3
cp external/SDL3/LICENSE.txt docs/legal/SDL3
mkdir -p docs/legal/SDL3_mixer
cp external/SDL3/LICENSE.txt docs/legal/SDL3_mixer

mkdir -p docs/legal/libxmp
cp external/libxmp/README external/libxmp/docs/COPYING docs/legal/libxmp
mkdir -p docs/legal/ogg
cp external/ogg/README.md external/ogg/COPYING docs/legal/ogg
mkdir -p docs/legal/vorbis
cp external/vorbis/README.md external/vorbis/COPYING docs/legal/vorbis

cp LICENSE docs/legal

