#!/bin/bash
TARGET_MAIN=/disks/salmon/cfrankb/toolkit/gcc/cs3-runtime-sdl
TARGET_EDITOR=/disks/salmon/cfrankb/toolkit/qt/cs3-map-edit
set -e

show_help () {
    printf 'available options:\n\n'
    echo "-a        copy metadata to editor"
    echo "-s        copy statedata to editor"
    echo "-g        copy runtime to editor"
    echo "-m        updating map in runtime"
}

if [[ "$1" == "-h" ]] ; then
    show_help
elif [[ "$1" == "-a" ]] ; then
    echo "copy metadata to editor"
    cp src/tiles*.h $TARGET_EDITOR/src
    cp src/tiles*.cpp $TARGET_EDITOR/src
    cp src/animzdata*.h $TARGET_EDITOR/src
elif [[ "$1" == "-s" ]] ; then
    echo "copy statedata.*"
    cp $TARGET_MAIN/src/statedata.* $TARGET_EDITOR/src
elif [[ "$1" == "-g" ]] ; then
    echo "copy runtime to editor"
    cp $TARGET_MAIN/src/sounds.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/actor.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/animator.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/game.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/gamestats.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/gamesfx.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/gamemixin.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/map.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/states.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/statedata.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/sprtypes.h $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/events.h $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/colormap.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/strhelper.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/chars.h $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/chars.cpp $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/attr.h $TARGET_EDITOR/src
elif [[ "$1" == "-m" ]] ; then
    echo "updating map in runtime"
    file=data/levels.mapz
    if [[ -f "$file" ]]; then
        timestamp=$(date +"%Y%m%d_%H%M%S")
        cp "$file" "$file.bak_$timestamp"
        echo "Backed up $file to $file.bak_$timestamp"
    fi
    cp tests/in/levels2.mapz  data/levels.mapz
elif [ -z "$1" ]; then
    echo "nothing to do"
    show_help
else
    echo "unknown switch: $1"
fi
