#!/bin/bash

#TARGET_IMAGE=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/spiffs_image/
#TARGET_MAIN=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/main/
#TARGET_IMAGE=/disks/salmon/cfrankb/toolkit/hardware/esp32/esp/dev/vlamits_esp32/spiffs_image/
TARGET_MAIN=/disks/salmon/cfrankb/toolkit/gcc/cs3-runtime-sdl
TARGET_EDITOR=/disks/salmon/cfrankb/toolkit/qt/cs3-map-edit
#PARAMS="-zvh  --checksum -ignore-times"
PARAMS="-zhi  --checksum "

set -e

make && ./build/mcxz -p data.ini

rsync $PARAMS out/tiles*.h $TARGET_MAIN/src
rsync $PARAMS out/animz*.h $TARGET_MAIN/src
rsync $PARAMS out/tiles*.cpp $TARGET_MAIN/src
rsync $PARAMS out/animzdata*.h $TARGET_MAIN/src
rsync $PARAMS out/sprtypes.h $TARGET_MAIN/src
rsync $PARAMS out/tiles.png $TARGET_MAIN/data/pixels/tiles.obl
rsync $PARAMS out/animz.png $TARGET_MAIN/data/pixels/animz.obl
rsync $PARAMS out/users.png $TARGET_MAIN/data/pixels/users.obl
rsync $PARAMS ../chars/out/chars.h $TARGET_MAIN/src
rsync $PARAMS ../chars/out/chars.cpp $TARGET_MAIN/src

if [[ "$1" == "-a" ]] ; then
    echo "copy metadata to editor"
    rsync $PARAMS out/tiles*.h $TARGET_EDITOR/src/runtime
    rsync $PARAMS out/tiles*.cpp $TARGET_EDITOR/src/runtime
    rsync $PARAMS out/animzdata*.h $TARGET_EDITOR/src/runtime
    rsync $PARAMS out/sprtypes.h $TARGET_EDITOR/src/runtime
elif [[ "$1" == "-g" ]] ; then
    echo "unsupported. please use sync.py"
    exit 1
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
    echo "-g        copy gamefiles to editor (deprecated)"
    echo "-m        update mapfile in runtime"
elif [ -z "$1" ]; then
    echo "all done"
else
    echo "unknown switch: $1"
fi

rsync $PARAMS out/tiles.png $TARGET_EDITOR/src/data/tiles.obl
rsync $PARAMS out/animz.png $TARGET_EDITOR/src/data/animz.obl
rsync $PARAMS out/annie.png $TARGET_EDITOR/src/data/annie.obl
