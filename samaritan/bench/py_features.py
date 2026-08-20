import sys, os
sys.path.insert(0, "/Users/martinvelitchkov/src/NNUE4pc")
import nnue_data as nd

# nnue_data builds LIVE_SQ/ROT lazily; find and call the initialiser.
for name in ("_init", "init", "_initialise", "_initialize", "init_tables"):
    if hasattr(nd, name):
        try: getattr(nd, name)(); break
        except TypeError: pass

line = open(sys.argv[1]).readline().split()
assert line[0] == "PIECES"
pieces, squares = [], []
for tok in line[1:]:
    p, sq = tok.split(":")
    pieces.append(int(p)); squares.append(int(sq))

print("num_features(py) =", nd.NUM_FEATURES)
for p in range(4):
    idx = []
    nd.emit_features(bytes(pieces), bytes(squares), len(pieces), p, idx)
    print("P%d" % p, " ".join(str(i) for i in sorted(idx)))
