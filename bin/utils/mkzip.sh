#!/bin/bash
mkdir -p build/cs3
cp build/cs3v2.* build/cs3
mv build/cs3/cs3v2.html build/cs3/index.html
cd build/cs3
zip cs3.zip cs3v2* index.html