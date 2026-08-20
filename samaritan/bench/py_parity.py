import sys, struct
import numpy as np, torch
sys.path.insert(0, "/Users/martinvelitchkov/src/NNUE4pc")
import nnue as N, nnue_data as nd

netfile, cppfile, featfile = sys.argv[1], sys.argv[2], sys.argv[3]

with open(netfile, "rb") as f:
    assert f.read(4) == b"SNN1"
    nf, acc, ff = struct.unpack("<iii", f.read(12))
    ftw  = np.frombuffer(f.read(nf*acc*4), dtype="<f4").reshape(nf, acc)
    ftb  = np.frombuffer(f.read(acc*4),    dtype="<f4")
    ff1w = np.frombuffer(f.read(ff*acc*4*4), dtype="<f4").reshape(ff, acc*4)
    ff1b = np.frombuffer(f.read(ff*4),     dtype="<f4")
    ff2w = np.frombuffer(f.read(ff*4),     dtype="<f4").reshape(1, ff)
    ff2b = np.frombuffer(f.read(4),        dtype="<f4")
print(f"loaded net: {nf} features x {acc}, ff {ff}")

model = N.NNUE(feature_count=nf, acc_size=acc, ff_size=ff, sparse=False)
with torch.no_grad():
    model.ft.emb.weight.copy_(torch.from_numpy(ftw.copy()))
    model.ft.bias.copy_(torch.from_numpy(ftb.copy()))
    model.ff1.weight.copy_(torch.from_numpy(ff1w.copy()))
    model.ff1.bias.copy_(torch.from_numpy(ff1b.copy()))
    model.ff2.weight.copy_(torch.from_numpy(ff2w.copy()))
    model.ff2.bias.copy_(torch.from_numpy(ff2b.copy()))
model.eval()

# per-position feature bags, dumped by the C++ side
bags = {}
cur = None
for line in open(featfile):
    t = line.split()
    if t[0] == "POS": cur = t[1]; bags[cur] = ([], int(t[2]))
    elif t[0].startswith("P") and t[0][1:].isdigit(): bags[cur][0].append([int(x) for x in t[1:]])

cpp = {}
for line in open(cppfile):
    t = line.split()
    if t[0] == "EVAL": cpp[t[1]] = (int(t[2]), int(t[3]), float(t[4]))

print(f"{'pos':<8}{'C++ raw':>12}{'PyTorch raw':>14}{'abs diff':>12}")
worst = 0.0; n = 0
for lab, (perbags, stm) in bags.items():
    idx, off, o = [], [], 0
    for b in perbags:
        off.append(o); idx.extend(b); o += len(b)
    with torch.no_grad():
        out = model(torch.tensor(idx), torch.tensor(off), torch.tensor([stm]))
    py = float(out.item())
    c  = cpp[lab][2]
    worst = max(worst, abs(py - c)); n += 1
    print(f"{lab:<8}{c:>12.6f}{py:>14.6f}{abs(py-c):>12.2e}")
print(f"\n{n} positions, worst |C++ - PyTorch| = {worst:.3e}")
print('FORWARD PASS MATCHES TO FLOAT PRECISION' if worst < 1e-2 else '*** FORWARD PASS DIVERGES ***')
