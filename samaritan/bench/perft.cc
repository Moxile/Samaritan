// Standalone perft driver.
//
//   perft <depth> [fen] [--split] [--no-bulk]
//
// With no FEN it uses the standard opening position. --split prints the node
// count under each root move, which is how you bisect a perft mismatch against
// another engine. --no-bulk makes every leaf move for real instead of counting
// the move list, which exercises make/unmake at the deepest ply.
#include "utility.h"
#include "movegen.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

static const std::string START_FEN =
    "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
    "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/3,yP,yP,yP,yP,yP,yP,yP,yP,3/14/"
    "bR,bP,10,gP,gR/bN,bP,10,gP,gN/bB,bP,10,gP,gB/bQ,bP,10,gP,gK/"
    "bK,bP,10,gP,gQ/bB,bP,10,gP,gB/bN,bP,10,gP,gN/bR,bP,10,gP,gR/14/"
    "3,rP,rP,rP,rP,rP,rP,rP,rP,3/3,rR,rN,rB,rQ,rK,rB,rN,rR,3";

static bool noBulk = false;

static uint64_t perft(int depth, Position &pos)
{
    if (depth == 0) return 1;
    MoveList moves(pos);
    if (depth == 1 && !noBulk) return moves.size();

    uint64_t nodes = 0;
    for (Move m : moves)
    {
        pos.move(m);
        nodes += perft(depth - 1, pos);
        pos.undoMove(m);
    }
    return nodes;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr,
            "usage: %s <depth> [fen] [--split] [--no-bulk]\n", argv[0]);
        return 2;
    }

    int depth = std::atoi(argv[1]);
    std::string fen = START_FEN;
    bool split = false;

    for (int i = 2; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--split")        split = true;
        else if (a == "--no-bulk") noBulk = true;
        else                       fen = a;
    }

    initZobrist();
    Position pos(false);          // false: no NNUE, movegen only
    loadFEN(pos, fen);

    auto start = std::chrono::steady_clock::now();
    uint64_t total = 0;

    if (split)
    {
        MoveList moves(pos);
        for (Move m : moves)
        {
            pos.move(m);
            const uint64_t n = perft(depth - 1, pos);
            pos.undoMove(m);
            std::printf("%s: %llu\n", m.toUCI().c_str(), (unsigned long long)n);
            total += n;
        }
    }
    else
    {
        total = perft(depth, pos);
    }

    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    std::printf("\nNodes searched: %llu\n", (unsigned long long)total);
    std::printf("Time taken: %.6fs\n", secs);
    std::printf("Nodes per second (NPS): %.0f\n", secs > 0 ? total / secs : 0.0);
    return 0;
}
