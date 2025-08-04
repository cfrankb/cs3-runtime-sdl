#!/bin/bash
cd /sdl2-windows
rm -f sdl2-mingw.zip
zip -r sdl2-mingw.zip bin include lib
mv sdl2-mingw.zip $1