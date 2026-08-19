// Correctness gate: perft every corpus position at depths 1..N and compare
// against reference counts produced by stockfish_4pc. Run after every change.
#include "utility.h"
#include "movegen.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

static uint64_t perft(int d, Position& p) {
    MoveList m(p);
    if (d == 1) return m.size();
    uint64_t n = 0;
    for (Move x : m) { p.move(x); n += perft(d - 1, p); p.undoMove(x); }
    return n;
}
// No bulk counting: every leaf move is really made and unmade.
static uint64_t perftNoBulk(int d, Position& p) {
    if (d == 0) return 1;
    MoveList m(p);
    uint64_t n = 0;
    for (Move x : m) { p.move(x); n += perftNoBulk(d - 1, p); p.undoMove(x); }
    return n;
}

int main(int argc, char** argv) {
    const char* corpus = argc > 1 ? argv[1] : "bench/positions.tsv";
    const char* refFile = argc > 2 ? argv[2] : nullptr;
    const int depth = argc > 3 ? atoi(argv[3]) : 4;

    std::map<std::string, uint64_t> ref;
    if (refFile) {
        std::ifstream rf(refFile); std::string l;
        while (std::getline(rf, l)) {
            std::stringstream ss(l); std::string lab, fen, cnt;
            std::getline(ss, lab, '\t'); std::getline(ss, fen, '\t'); std::getline(ss, cnt);
            if (!cnt.empty()) ref[lab] = std::stoull(cnt);
        }
    }

    initZobrist();
    Position pos(false);
    std::ifstream in(corpus); std::string line;
    int fail = 0, checked = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string lab, pc, mv, fen;
        std::getline(ss, lab, '\t'); std::getline(ss, pc, '\t');
        std::getline(ss, mv, '\t'); std::getline(ss, fen);

        loadFEN(pos, fen);
        uint64_t n  = perft(depth, pos);
        uint64_t nb = perftNoBulk(depth, pos);

        bool bulkOk = (n == nb);
        bool refOk  = true;
        auto it = ref.find(lab);
        if (it != ref.end()) { refOk = (n == it->second); checked++; }

        printf("%-6s %2s pieces  perft%d = %-10llu", lab.c_str(), pc.c_str(), depth,
               (unsigned long long)n);
        if (it != ref.end()) printf("  ref %-10llu %s", (unsigned long long)it->second,
                                    refOk ? "MATCH" : "*** DIFF ***");
        if (!bulkOk) printf("  *** BULK/NO-BULK DIVERGE (%llu) ***", (unsigned long long)nb);
        printf("\n");
        if (!refOk || !bulkOk) fail++;
    }
    printf("\n%d position(s) compared against the reference, %d failure(s)\n", checked, fail);
    return fail ? 1 : 0;
}
