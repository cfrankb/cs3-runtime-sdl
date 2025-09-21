#!/bin/bash

set -e

SOURCE=packages/data/macos/icon512.png
echo "Resizing $SOURCE ..."
echo "Resized images to android-project/app/src/main/res/mipmap-*/ic_launcher2.png"

DEST=android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png
magick $SOURCE -resize 72x72 $DEST
DEST=android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png
magick $SOURCE -resize 48x48 $DEST
DEST=android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png
magick $SOURCE -resize 96x96 $DEST
DEST=android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png
magick $SOURCE -resize 144x144 $DEST
DEST=android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png
magick $SOURCE -resize 192x192 $DEST
echo "Done."