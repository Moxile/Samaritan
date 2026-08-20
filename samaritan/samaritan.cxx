#include "engine.h"

// Google Benchmark is optional: it powers `samaritan --perft` only, and making
// it mandatory would put a third package between a fresh checkout (a Windows
// one in particular) and a working engine.
#ifdef SAMARITAN_HAS_BENCHMARK
#include <benchmark/benchmark.h>

static void BM_Perft(benchmark::State& state) {
    const std::string modern_fen = "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
                                   "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
                                   "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
                                   "14/"
                                   "bR,bP,10,gP,gR/"
                                   "bN,bP,10,gP,gN/"
                                   "bB,bPP,10,gP,gB/"
                                   "bQ,bP,10,gP,gK/"
                                   "bK,bP,10,gP,gQ/"
                                   "bB,bP,10,gP,gB/"
                                   "bN,bP,10,gP,gN/"
                                   "bR,bP,10,gP,gR/"
                                   "14/"
                                   "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
                                   "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";
    Position pos = Position(false);
    loadFEN(pos, modern_fen);

    int nodes = 0;
    int depth = state.range(0);
    for (auto _ : state) {
        nodes += fullsearch(depth, pos);
    }
    state.SetItemsProcessed(nodes);
    state.counters["NPS"] = benchmark::Counter(nodes, benchmark::Counter::kIsRate);
}
BENCHMARK(BM_Perft)->Unit(benchmark::kMillisecond)->DenseRange(1, 6, 1)->ArgNames({"Depth"});
#endif // SAMARITAN_HAS_BENCHMARK

int main(int argc, char* argv[])
{
    // Check for --perft flag first
    bool run_benchmarks = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--perft") {
            run_benchmarks = true;
            // Remove --perft from argv by shifting remaining args
            for (int j = i; j < argc - 1; ++j) {
                argv[j] = argv[j + 1];
            }
            argc--; // Reduce argument count
            break;
        }
    }

    if (run_benchmarks) {
#ifdef SAMARITAN_HAS_BENCHMARK
        benchmark::Initialize(&argc, argv);
        if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        return 0;
#else
        std::cerr << "this build has no Google Benchmark; use bench/perft or "
                     "the engine's own `perft <depth>` command instead\n";
        return 1;
#endif
    }
    else 
    {
        samaritan::Engine engine;
        engine.launch();
    }

    return 0;
}