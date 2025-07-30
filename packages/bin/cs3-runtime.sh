#!/bin/sh
BASE_DIR=$(dirname "$(readlink -f "$0")")

pwd
ls -l

"$BASE_DIR/cs3-runtime" "$@"