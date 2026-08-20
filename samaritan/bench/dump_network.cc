// Writes the current network to disk and prints its evaluation of each corpus
// position, so the Python side can load the same weights and compare.
//
// The file is written by nnue::Network::save and read back by
// nnue::Network::load -- see network.h for the SNN1 layout.
#include "utility.h"
#include "movegen.h"
#include "evaluate.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    const char *corpus = argv[1];
    const char *out    = argv[2];

    initZobrist();
    nnue::network().randomize(42);
    const auto &net = nnue::network();

    if (!net.save(out))
    {
        std::fprintf(stderr, "cannot write %s\n", out);
        return 1;
    }

    // Round-trip immediately: the loader is what the engine will use, so a
    // format drift between writer and reader should fail here, not in a game.
    {
        nnue::Network check;
        std::string error;
        if (!check.load(out, &error))
        {
            std::fprintf(stderr, "written network does not load back: %s\n", error.c_str());
            return 1;
        }
    }

    Position pos(false);
    std::ifstream in(corpus); std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line); std::string lab, pc, mv, fen;
        std::getline(ss, lab, '\t'); std::getline(ss, pc, '\t');
        std::getline(ss, mv, '\t'); std::getline(ss, fen);
        loadFEN(pos, fen);
        nnue::Accumulators a;
        a.refresh(pos.board);
        const PieceColor stm = pos.gameStates.back().curTurn;
        std::printf("EVAL %s %d %d %.6f\n", lab.c_str(),
                    ctz((unsigned)stm),
                    nnue::forward(a, stm), nnue::forwardRaw(a, stm));
    }
    return 0;
}
