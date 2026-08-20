#!/bin/bash

rm build/msvc/SDL3/CMakeFiles/SDL3-static.dir/cmake_pch.h.pch
rm build/msvc/SDL3/CMakeFiles/SDL3-static.dir/cmake_pch.hxx.pch
rm build/msvc/SDL3/CMakeFiles/SDL3-static.dir/stdlib/SDL_string.c.obj
cmake --build build/msvc --target SDL3-static -- VERBOSE=1 2>&1 | grep -E "SDL_string|pch|error"