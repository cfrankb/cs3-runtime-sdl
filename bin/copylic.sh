#!/bin/bash
mkdir -p techdocs/legal/zlib
cp external/zlib/LICENSE techdocs/legal/zlib
mkdir -p techdocs/legal/SDL3
cp external/SDL3/LICENSE.txt techdocs/legal/SDL3
mkdir -p techdocs/legal/SDL3_mixer
cp external/SDL3/LICENSE.txt techdocs/legal/SDL3_mixer

mkdir -p techdocs/legal/libxmp
cp external/libxmp/README external/libxmp/docs/COPYING techdocs/legal/libxmp
mkdir -p techdocs/legal/ogg
cp external/ogg/README.md external/ogg/COPYING techdocs/legal/ogg
mkdir -p techdocs/legal/vorbis
cp external/vorbis/README.md external/vorbis/COPYING techdocs/legal/vorbis

cp LICENSE techdocs/legal

