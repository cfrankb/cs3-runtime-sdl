#!/bin/bash

rm -rf build/msvc/SDL3
cmake --build build/msvc --target SDL3-static -- VERBOSE=1 2>&1 | tail -20