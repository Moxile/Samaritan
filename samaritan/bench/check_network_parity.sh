#!/bin/sh
# Verifies Samaritan's C++ forward pass against the PyTorch model in ../../NNUE4pc.
#
# Dumps the current network in the SNN1 format, loads those exact weights into
# the trainer's own NNUE class, and compares the raw (pre-rounding) output on
# every corpus position. Expect agreement to ~1e-5, i.e. float32 noise.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SAM=$(dirname "$HERE")
NN=${NNUE4PC:-$SAM/../../NNUE4pc}
PY=${PYTHON:-$NN/.venv/bin/python}
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

[ -x "$PY" ] || { echo "no python at $PY (set PYTHON=)"; exit 1; }

BUILD=${BUILD_DIR:-$SAM/build}
cmake -S "$SAM" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD" --target dump_network dump_bags -j >/dev/null

"$BUILD/bench/dump_network" "$HERE/positions.tsv" "$TMP/net.bin" > "$TMP/cpp_eval.txt"
"$BUILD/bench/dump_bags"    "$HERE/positions.tsv"                > "$TMP/bags.txt"
cd "$NN" && "$PY" "$HERE/py_parity.py" "$TMP/net.bin" "$TMP/cpp_eval.txt" "$TMP/bags.txt"
