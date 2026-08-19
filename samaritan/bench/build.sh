#!/bin/sh
# Builds the movegen tools. Run from the samaritan/ directory.
set -e
CXX="${CXX:-clang++}"
FLAGS="-std=c++23 -O3 -march=native -I include"
SRC="src/movegen.cpp src/utility.cpp"
for tool in perft verify bench; do
    echo "building bench/$tool"
    $CXX $FLAGS "bench/$tool.cc" $SRC -o "bench/$tool"
done
echo "done"
