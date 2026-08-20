#!/bin/sh
# Verifies that Samaritan's NNUE feature encoder produces exactly the same
# indices as the Python trainer in ../../NNUE4pc, for every corpus position.
#
# This is the guard against trainer/engine feature drift, which is the classic
# way an NNUE ends up quietly evaluating a different game than it was trained on.
# Run it whenever either side's encoding changes.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SAM=$(dirname "$HERE")
NN=${NNUE4PC:-$SAM/../../NNUE4pc}
PY=${PYTHON:-$NN/.venv/bin/python}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

[ -x "$PY" ] || { echo "no python at $PY (set PYTHON=)"; exit 1; }

# The dumper is a CMake target; build it rather than re-deriving the compile
# line here, so it is always built the same way as the engine it is speaking for.
BUILD=${BUILD_DIR:-$SAM/build}
cmake -S "$SAM" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD" --target dump_features -j >/dev/null
DUMP="$BUILD/bench/dump_features"

pass=0; fail=0
while IFS='	' read -r lab pieces moves fen; do
    case "$lab" in \#*|"") continue;; esac
    "$DUMP" "$fen" > "$TMP/c.txt"
    (cd "$NN" && "$PY" "$HERE/py_features.py" "$TMP/c.txt" > "$TMP/p.txt" 2>/dev/null)
    if python3 -c "
import sys
c={l.split()[0]:l.split()[1:] for l in open('$TMP/c.txt') if l[0]=='P' and l[1].isdigit()}
p={l.split()[0]:l.split()[1:] for l in open('$TMP/p.txt') if l[0]=='P' and l[1].isdigit()}
sys.exit(0 if (c and c==p) else 1)"; then
        pass=$((pass+1))
    else
        fail=$((fail+1)); echo "  FEATURE MISMATCH: $lab"
    fi
done < "$HERE/positions.tsv"

echo "feature parity: $pass position(s) match the trainer, $fail diverging"
[ "$fail" -eq 0 ]
