#!/bin/bash
# Manually test if adding -msse2 fixes the compile

clang-19 --target=x86_64-pc-windows-msvc \
    -msse2 \
    -I/opt/xwin/crt/include \
    -I/opt/xwin/sdk/include/ucrt \
    -Iexternal/SDL3_mixer/external/vorbis/include \
    -Iexternal/SDL3_mixer/external/ogg/include \
    -c external/SDL3_mixer/external/vorbis/lib/vorbisfile.c \
    -o /tmp/vorbisfile_test.obj 2>&1

llvm-nm-19 /tmp/vorbisfile_test.obj | grep "_mm_"


clang-19 --target=x86_64-pc-windows-msvc \
    -msse2 \
    -I/opt/xwin/crt/include \
    -I/opt/xwin/sdk/include/ucrt \
    -Iexternal/SDL3_mixer/external/vorbis/include \
    -Iexternal/SDL3_mixer/external/ogg/include \
    -v \
    -c external/SDL3_mixer/external/vorbis/lib/vorbisfile.c \
    -o /tmp/vorbisfile_test.obj 2>&1 | grep -i "emmintrin\|sse\|include"
    
    
#cat /tmp/vorbisfile_test.obj     | grep -i "emmintrin\|sse\|include"

