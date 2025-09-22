#!/bin/bash

SHORT_HASH=$(git rev-parse --short HEAD)
VERSION=$(cat HISTORY | head -n 1)
printf "#define BUILD_HASH \"$SHORT_HASH\"\n" > src/build.h
printf "#define VERSION \"$VERSION\"\n" >> src/build.h
