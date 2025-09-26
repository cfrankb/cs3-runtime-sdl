#!/bin/bash
make && build/makebin && python bin/makesrc.py
cp out/*.h /home/cfrankb/toolkit/gcc/cs3-runtime-sdl/src/
cp out/*.cpp /home/cfrankb/toolkit/gcc/cs3-runtime-sdl/src/