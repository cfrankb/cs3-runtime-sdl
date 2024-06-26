#!/bin/bash
mkdir -p ./build/final
rm ./build/final/*
cp ./build/cs3v2.* ./build/final
gzip  ./build/final/*.data ./build/final/*.wasm
mv ./build/final/cs3v2.data.gz ./build/final/cs3v2.data
mv ./build/final/cs3v2.wasm.gz ./build/final/cs3v2.wasm
cat src/template/footer.html >> ./build/final/cs3v2.html
