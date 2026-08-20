// The incremental accumulator must agree with a full refresh at every node,
// after every make AND every undo. The full refresh is the oracle.
#include "utility.h"
#include "movegen.h"
#include "evaluate.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>

static long nodes = 0, fails = 0, tick = 0, invalidated = 0, skipped = 0;
static double worst = 0.0;

static void check(Position &p, const char *when, const char *mv)
{
    nodes++;
    // The engine trusts the stack only while accValid() holds; otherwise it
    // full-refreshes, so there is no incremental value to compare against. That
    // is the state right after a deliberate invalidation below, and it is also
    // why reading accStack.back() unconditionally would be testing something
    // the engine never does.
    if (!p.accValid()) { skipped++; return; }
    // Compare the raw pre-rounding values: an integer comparison cannot tell a
    // real divergence from a value that happens to sit on a rounding boundary.
    nnue::Accumulators fullAcc;
    fullAcc.refresh(p.board);
    const PieceColor stm = p.gameStates.back().curTurn;
    const float incRaw  = nnue::forwardRaw(p.accStack.back(), stm);
    const float fullRaw = nnue::forwardRaw(fullAcc, stm);
    const double d = std::fabs(double(incRaw) - double(fullRaw));
    if (d > worst) worst = d;
    if (d > 1e-2) {
        if (fails < 8)
            printf("  MISMATCH %s %s: incremental %.6f, full refresh %.6f\n", when, mv, incRaw, fullRaw);
        fails++;
    }
}

static void walk(int d, Position &p)
{
    check(p, "at node", "");
    if (d == 0) return;
    MoveList moves(p);
    for (Move m : moves) {
        // Periodically simulate the two ways a stack stops being usable: the
        // evaluator being switched on with a position already loaded (no stack
        // at all), and the network being replaced under a live stack (a stack
        // built from weights that are gone). Both must be detected and rebuilt
        // by the next make; neither may be quietly used as a parent.
        if (++tick % 7 == 0)
        {
            if (tick % 14 == 0) p.accStack.clear();   // evaluator switched on
            else                p.accGen = ~p.accGen; // network replaced
            invalidated++;
        }

        p.move(m);
        walk(d - 1, p);
        p.undoMove(m);
        check(p, "after undo of", m.toUCI().c_str());
    }
}

int main(int argc, char **argv)
{
    initZobrist();
    nnue::network().randomize(42);
    Position pos(true);                    // evaluation enabled
    std::ifstream in(argv[1]); std::string line;
    const int depth = argc > 2 ? atoi(argv[2]) : 3;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string lab, pc, mv, fen;
        std::getline(ss, lab, '\t'); std::getline(ss, pc, '\t');
        std::getline(ss, mv, '\t'); std::getline(ss, fen);
        loadFEN(pos, fen);
        long before = fails;
        walk(depth, pos);
        printf("%-8s %s\n", lab.c_str(), fails == before ? "ok" : "FAILED");
    }
    printf("\n%ld nodes visited, %ld compared, %ld skipped as not-yet-rebuilt, "
           "%ld invalidations forced\n", nodes, nodes - skipped, skipped, invalidated);
    printf("%ld mismatches, worst raw |incremental - full| = %.3e\n", fails, worst);
    return fails ? 1 : 0;
}
