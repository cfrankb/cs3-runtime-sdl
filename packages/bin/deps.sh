#!/bin/bash

BIN=./buildx/cs3-runtime  # path to your built executable

ldd "$BIN" \
  | awk '{print $1}' \
  | while read lib; do
      [[ -z "$lib" || "$lib" == "linux-vdso.so"* ]] && continue
      dpkg -S "$(readlink -f "$(ldconfig -p | grep -m1 $lib | awk '{print $NF}')")" 2>/dev/null
    done \
  | cut -d: -f1 \
  | sort -u
