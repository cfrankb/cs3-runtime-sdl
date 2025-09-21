#!/bin/bash

SHORT_HASH=$(git rev-parse --short HEAD)
printf "#define BUILD_HASH \"$SHORT_HASH\"\n" > src/build.h