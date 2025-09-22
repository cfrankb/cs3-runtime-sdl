#!/bin/bash
mkdir -p ./build/local
rm -f ./build/local/*
cp ./build/cs3v2.* ./build/local
gzip  ./build/local/*.data ./build/local/*.wasm
mv ./build/local/cs3v2.data.gz ./build/local/cs3v2.data
mv ./build/local/cs3v2.wasm.gz ./build/local/cs3v2.wasm
