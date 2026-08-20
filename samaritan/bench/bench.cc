// Movegen throughput across the corpus: ns per legal MoveList construction,
// and ns per generated move. Print with --csv for machine-readable output.
#include "utility.h"
#include "movegen.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

int main(int argc, char** argv) {
    const char* corpus = argc > 1 ? argv[1] : "bench/positions.tsv";
    bool csv = argc > 2 && std::string(argv[2]) == "--csv";

    initZobrist();
    Position pos(false);
    std::ifstream in(corpus); std::string line;
    if (!csv) printf("%-6s %7s %7s %12s %12s\n", "pos", "pieces", "moves", "ns/movelist", "ns/move");

    double totalNs = 0; long totalMoves = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string lab, pc, mv, fen;
        std::getline(ss, lab, '\t'); std::getline(ss, pc, '\t');
        std::getline(ss, mv, '\t'); std::getline(ss, fen);
        loadFEN(pos, fen);

        // warm up, then take the best of 5 runs to suppress scheduler noise
        // volatile sink instead of an inline-asm barrier: same effect (the
        // optimiser cannot drop the loop), but it also compiles on MSVC.
        volatile size_t sink = 0;
        for (int i = 0; i < 20000; i++) { MoveList m(pos); sink = m.size(); }
        (void)sink;
        double best = 1e18; size_t n = 0;
        for (int rep = 0; rep < 5; rep++) {
            const int N = 200000;
            auto t0 = std::chrono::steady_clock::now();
            size_t s = 0;
            for (int i = 0; i < N; i++) { MoveList m(pos); s += m.size(); }
            auto t1 = std::chrono::steady_clock::now();
            double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9 / N;
            best = std::min(best, ns); n = s / N;
        }
        totalNs += best; totalMoves += n;
        if (csv) printf("%s,%s,%zu,%.1f\n", lab.c_str(), pc.c_str(), n, best);
        else printf("%-6s %7s %7zu %12.1f %12.2f\n", lab.c_str(), pc.c_str(), n, best, best / n);
    }
    if (!csv) printf("%-6s %7s %7ld %12.1f %12.2f\n", "TOTAL", "-", totalMoves, totalNs,
                     totalNs / totalMoves);
    return 0;
}
