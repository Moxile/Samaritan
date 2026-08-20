// Validates computeCheckInfo against the brute-force isSquareAttacked logic at
// every node of a perft walk. Pins are checked by physically lifting the piece.
#include "utility.h"
#include "movegen.h"
#include <cstdio>
#include <fstream>
#include <sstream>

static long nodes = 0, failures = 0;

static void validate(Position &pos)
{
    const PieceColor us = pos.gameStates.back().curTurn;
    Board &b = pos.board;
    const int ksq = b.kingTracker[ctz((unsigned int)us)];
    if (ksq < 0) return;

    CheckInfo info;
    computeCheckInfo(pos, us, info);
    nodes++;

    // 1. check detection must agree with the brute-force scan
    const bool brute = inCheck(pos, us);
    if ((info.checkerCount > 0) != brute) {
        printf("MISMATCH check: computeCheckInfo=%d brute=%d  fen=%s\n",
               info.checkerCount, (int)brute, positionToFEN(pos).c_str());
        failures++;
    }

    // 2. every one of our pieces: pinned iff lifting it exposes the king
    for (int sq = 0; sq < 224; sq++) {
        if (b.colorMailbox[sq] != us || sq == ksq) continue;
        const PieceType pie = b.pieceMailbox[sq];
        const PieceColor col = b.colorMailbox[sq];
        b.pieceMailbox[sq] = NONE_PIECE; b.colorMailbox[sq] = NONE_COLOR;
        const bool exposed = b.isSquareAttacked(ksq, us, getTeam(us));
        b.pieceMailbox[sq] = pie; b.colorMailbox[sq] = col;

        const bool claimed = info.pinRayOf(sq) != 0;
        // A piece that is already blocking while we are IN check can read as
        // "exposed" for the checker itself; only compare when not in check.
        if (!brute && exposed != claimed) {
            printf("MISMATCH pin at %d: lifted=%d claimed=%d  fen=%s\n",
                   sq, (int)exposed, (int)claimed, positionToFEN(pos).c_str());
            failures++;
        }
    }

    // 3. single checker square must really attack the king
    if (info.checkerCount == 1) {
        const PieceType pie = b.pieceMailbox[info.checkerSq];
        const PieceColor col = b.colorMailbox[info.checkerSq];
        if (pie == NONE_PIECE || getTeam(col) == getTeam(us)) {
            printf("MISMATCH checkerSq %d holds no enemy piece  fen=%s\n",
                   info.checkerSq, positionToFEN(pos).c_str());
            failures++;
        }
    }
}

static void walk(int d, Position &pos)
{
    validate(pos);
    if (d == 0) return;
    MoveList m(pos);
    for (Move x : m) { pos.move(x); walk(d - 1, pos); pos.undoMove(x); }
}

int main(int argc, char **argv)
{
    const char *corpus = argc > 1 ? argv[1] : "bench/positions.tsv";
    const int depth = argc > 2 ? atoi(argv[2]) : 3;
    initZobrist();
    Position pos(false);
    std::ifstream in(corpus); std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string lab, pc, mv, fen;
        std::getline(ss, lab, '\t'); std::getline(ss, pc, '\t');
        std::getline(ss, mv, '\t'); std::getline(ss, fen);
        loadFEN(pos, fen);
        long before = failures;
        walk(depth, pos);
        printf("%-6s %s\n", lab.c_str(), failures == before ? "ok" : "FAILED");
    }
    printf("\n%ld nodes validated, %ld failures\n", nodes, failures);
    return failures ? 1 : 0;
}
