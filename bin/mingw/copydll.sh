#!/bin/bash
set -e
PTHREAD_DLL=$(find /usr/x86_64-w64-mingw32/ | grep libwinpthread-1.dll | head -n 1)
cp $PTHREAD_DLL build
cp local/mingw/bin/*.dll build

#SEH_DLL=$(find /usr/x86_64-w64-mingw32/ | grep libgcc_s_seh-1| head -n 1)
#cp $SEH_DLL build
#LIBSTDC_DLL=$(find /usr/x86_64-w64-mingw32/ | grep libstdc++-6.dll| head -n 1)
#cp $LIBSTDC_DLL build
