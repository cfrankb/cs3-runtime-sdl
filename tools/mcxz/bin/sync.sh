#!/bin/bash

#TARGET_IMAGE=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/spiffs_image/
#TARGET_MAIN=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/main/
#TARGET_IMAGE=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/spiffs_image/
TARGET_MAIN=/disks/salmon/cfrankb/toolkit/gcc/cs3-runtime-sdl
TARGET_EDITOR=/disks/salmon/cfrankb/toolkit/qt/cs3-map-edit

set -e

make && ./build/mcxz -p data.ini

cp out/tiles*.h $TARGET_MAIN/src
cp out/animz*.h $TARGET_MAIN/src
#cp out/annie*.h $TARGET_MAIN/src
cp out/tiles*.cpp $TARGET_MAIN/src
cp out/animzdata*.h $TARGET_MAIN/src
cp out/sprtypes.h $TARGET_MAIN/src
cp out/tiles.png $TARGET_MAIN/data/pixels/tiles.obl
cp out/animz.png $TARGET_MAIN/data/pixels/animz.obl
#cp out/annie.png $TARGET_MAIN/data/annie.obl
cp out/users.png $TARGET_MAIN/data/pixels/users.obl
cp ../chars/out/chars.h $TARGET_MAIN/src
cp ../chars/out/chars.cpp $TARGET_MAIN/src

if [[ "$1" == "-a" ]] ; then
    echo "copy metadata to editor"
    cp out/tiles*.h $TARGET_EDITOR/src
    cp out/tiles*.cpp $TARGET_EDITOR/src
    cp out/animzdata*.h $TARGET_EDITOR/src
    cp out/sprtypes.h $TARGET_EDITOR/src
    #cp out/anniedata*.h $TARGET_EDITOR/src
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
    cp $TARGET_MAIN/src/attr.h $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/gameui.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/logger.h $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/recorder.* $TARGET_EDITOR/src
    cp $TARGET_MAIN/src/shared/FileMem.* $TARGET_EDITOR/src/shared
    cp $TARGET_MAIN/src/shared/FileWrap.* $TARGET_EDITOR/src/shared
    cp $TARGET_MAIN/src/shared/FrameSet.* $TARGET_EDITOR/src/shared
    cp $TARGET_MAIN/src/shared/PngMagic.* $TARGET_EDITOR/src/shared
    cp $TARGET_MAIN/src/shared/helper.* $TARGET_EDITOR/src/shared
    cp ../chars/out/chars.h $TARGET_EDITOR/src
    cp ../chars/out/chars.cpp $TARGET_EDITOR/src
elif [[ "$1" == "-m" ]] ; then
    file=$TARGET_MAIN/data/levels.mapz
    if [[ -f "$file" ]]; then
        timestamp=$(date +"%Y%m%d_%H%M%S")
        cp "$file" "$file.bak_$timestamp"
        echo "Backed up $file to $file.bak_$timestamp"
    fi
    cp $TARGET_MAIN/tests/in/levels2.mapz  $TARGET_MAIN/data/levels.mapz
elif [[ "$1" == "-h" ]] ; then
    echo "-a        copy metadata to editor"
    echo "-g        copy gamefiles to editor"
    echo "-m        update mapfile in runtime"
elif [ -z "$1" ]; then
    echo "all done"
else
    echo "unknown switch: $1"
fi

cp out/tiles.png $TARGET_EDITOR/src/data/tiles.obl
cp out/animz.png $TARGET_EDITOR/src/data/animz.obl
cp out/annie.png $TARGET_EDITOR/src/data/annie.obl
