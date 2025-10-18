#!/bin/bash
ls data/ > data/list.txt
make && build/sheet $@