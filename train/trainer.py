import argparse
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader

import time
import struct
import numpy as np

FEATURE_COUNT = 160*6*4*160*4
HIDDEN_SIZE = 16

# Copied from accumulator.h — maps mailbox location to board index (0-159), -1 = invalid
board_table = [
    -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7, -1, -1, -1, -1,
    -1, -1, -1, -1,  8,  9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1,
    -1, -1, -1, -1, 16, 17, 18, 19, 20, 21, 22, 23, -1, -1, -1, -1,
    -1, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, -1,
    -1, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1,
    -1, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, -1,
    -1, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, -1,
    -1, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, -1,
    -1, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107, -1,
    -1,108,109,110,111,112,113,114,115,116,117,118,119,120,121, -1,
    -1,122,123,124,125,126,127,128,129,130,131,132,133,134,135, -1,
    -1, -1, -1, -1,136,137,138,139,140,141,142,143, -1, -1, -1, -1,
    -1, -1, -1, -1,144,145,146,147,148,149,150,151, -1, -1, -1, -1,
    -1, -1, -1, -1,152,153,154,155,156,157,158,159, -1, -1, -1, -1,
]

# Copied from accumulator.h — relative king relationship per perspective
relative_table = [
    [0, 2, 1, 3],  # from RED's perspective
    [3, 0, 2, 1],  # from BLUE's perspective
    [1, 3, 0, 2],  # from YELLOW's perspective
    [2, 1, 3, 0],  # from GREEN's perspective
]

COLOR_IDX = {'r': 0, 'b': 1, 'y': 2, 'g': 3}
PIECE_IDX = {'P': 1, 'N': 2, 'B': 3, 'R': 4, 'Q': 5, 'K': 6}
TURN_IDX  = {'R': 0, 'B': 1, 'Y': 2, 'G': 3}

def load_weights(model, path):
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic != b'NNUE':
            raise ValueError(f"Not a valid NNUE model file: {path}")
        version     = struct.unpack('<i', f.read(4))[0]
        hidden_size = struct.unpack('<i', f.read(4))[0]
        if hidden_size != HIDDEN_SIZE:
            raise ValueError(f"Model hidden size {hidden_size} doesn't match HIDDEN_SIZE={HIDDEN_SIZE}")

        # L1 weights [FEATURE_COUNT, HIDDEN_SIZE]
        w = np.frombuffer(f.read(FEATURE_COUNT * hidden_size * 4), dtype='float32')
        model.hidden.weight.data = torch.from_numpy(w.reshape(FEATURE_COUNT, hidden_size))

        # L1 biases (written as zeros, skip)
        f.read(hidden_size * 4)

        # L2 weights [HIDDEN_SIZE]
        ow = np.frombuffer(f.read(hidden_size * 4), dtype='float32')
        model.output.weight.data = torch.from_numpy(ow.reshape(1, hidden_size))

        # L2 bias
        ob = struct.unpack('<f', f.read(4))[0]
        model.output.bias.data = torch.tensor([ob])

    print(f"Loaded weights from {path}")


def save_weights(model, path):
    with open(path, 'wb') as f:
        f.write(b'NNUE')
        f.write(struct.pack('<i', 1))            # version
        f.write(struct.pack('<i', HIDDEN_SIZE))  # hidden size

        # L1 weights [FEATURE_COUNT, HIDDEN_SIZE]
        w = model.hidden.weight.detach().cpu().numpy().flatten().astype('float32')
        f.write(w.tobytes())

        # L1 biases — EmbeddingBag has none, write zeros
        f.write(np.zeros(HIDDEN_SIZE, dtype='float32').tobytes())

        # L2 weights [HIDDEN_SIZE]
        ow = model.output.weight.detach().cpu().numpy().flatten().astype('float32')
        f.write(ow.tobytes())

        # L2 bias — scalar
        f.write(struct.pack('<f', model.output.bias.detach().cpu().item()))

def fen_to_features(fen):
    parts       = fen.split('-')
    perspective = TURN_IDX[parts[0]]
    board_fen   = parts[6]

    pieces    = []           # list of (loc, piece_idx, color_idx)
    king_locs = [-1, -1, -1, -1]

    for row_num, row in enumerate(board_fen.split('/')):
        col_num = 1
        for cell in row.split(','):
            try:
                col_num += int(cell)
            except ValueError:
                if len(cell) < 2:
                    col_num += 1
                    continue
                loc       = 16 * row_num + col_num
                color_idx = COLOR_IDX[cell[0]]
                piece_idx = PIECE_IDX[cell[1]]
                pieces.append((loc, piece_idx, color_idx))
                if piece_idx == 6:           # KING
                    king_locs[color_idx] = loc
                col_num += 1

    indices = []
    for loc, piece_idx, color_idx in pieces:
        if board_table[loc] == -1:
            continue
        for king_color_idx in range(4):
            king_loc = king_locs[king_color_idx]
            if king_loc == -1 or board_table[king_loc] == -1:
                continue
            feat = (
                relative_table[perspective][king_color_idx] * (160 * 160 * 6 * 4)
                + board_table[king_loc] * (160 * 6 * 4)
                + board_table[loc]      * (6 * 4)
                + piece_idx             * 4
                + relative_table[perspective][color_idx]
            )
            indices.append(feat)

    return torch.tensor(indices, dtype=torch.long)


class NNUE(nn.Module):
    def __init__(self):
        super().__init__()
        self.hidden = nn.EmbeddingBag(FEATURE_COUNT, HIDDEN_SIZE, mode='sum')
        self.output = nn.Linear(HIDDEN_SIZE, 1)

    def forward(self, x, offsets):
        x = self.hidden(x, offsets)
        x = torch.clamp(x, 0.0, 1.0)
        x = self.output(x)
        return x


parser = argparse.ArgumentParser(description="Train the Samaritan NNUE")
parser.add_argument("--data",   default="trainsample.txt", help="Training data file (FEN;score lines)")
parser.add_argument("--output", default="model.bin",       help="Output weights file")
parser.add_argument("--load",   default=None,              help="Optional: load existing weights to continue training")
parser.add_argument("--epochs", type=int, default=3,       help="Number of training epochs")
parser.add_argument("--lr",     type=float, default=0.001, help="Learning rate")
args = parser.parse_args()

model = NNUE()
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print("Using: ", device)
model = model.to(device)
print(model)

if args.load is not None:
    load_weights(model, args.load)
    model = model.to(device)

WDL_K = 1.0 / 400.0  # sigmoid(800 * WDL_K) ≈ 0.88 win probability at 800cp advantage

criterion = nn.MSELoss()
optimizer = optim.Adam(model.parameters(), lr=args.lr)


class Positions(torch.utils.data.Dataset):
    def __init__(self, path):
        with open(path) as f:
            self.lines = [l for l in f if ';' in l]

    def __len__(self):
        return len(self.lines)

    def __getitem__(self, i):
        fen, eval_str = self.lines[i].strip().split(';')
        features = fen_to_features(fen)
        return features, torch.tensor(float(eval_str))


def collate_fn(batch):
    features_list, targets = zip(*batch)
    lengths = torch.tensor([len(f) for f in features_list], dtype=torch.long)
    offsets = torch.cat([torch.zeros(1, dtype=torch.long), lengths[:-1]]).cumsum(0)
    return torch.cat(features_list), offsets, torch.stack(targets).unsqueeze(1)


dataset = Positions(args.data)
loader  = DataLoader(dataset, batch_size=256, shuffle=True, collate_fn=collate_fn)

for epoch in range(args.epochs):
    t0 = time.time()
    for features, offsets, target in loader:
        features, offsets, target = features.to(device), offsets.to(device), target.to(device)
        optimizer.zero_grad()
        outputs = model(features, offsets)
        win_prob = torch.sigmoid(outputs * WDL_K)
        loss = criterion(win_prob, target)
        loss.backward()
        optimizer.step()

    print(f"Epoch {epoch+1}, Loss: {loss.item():.4f}, Time: {time.time()-t0:.1f}s")


save_weights(model, args.output)
