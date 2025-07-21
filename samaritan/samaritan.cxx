#include "engine.h"

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
    Position pos = Position();
    loadFEN(pos, modern_fen);

    int nodes = 0;
    int depth = state.range(0);
    for (auto _ : state) {
        nodes += fullsearch(depth, pos);
    }
    state.SetItemsProcessed(nodes);
}
BENCHMARK(BM_Perft)->Unit(benchmark::kMillisecond)->RangeMultiplier(2)->Range(1, 4)->ArgNames({"Depth"});

int main(int argc, char* argv[])
{
    // Check for --perf flag first
    bool run_benchmarks = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--perf") {
            run_benchmarks = true;
            // Remove --perf from argv by shifting remaining args
            for (int j = i; j < argc - 1; ++j) {
                argv[j] = argv[j + 1];
            }
            argc--; // Reduce argument count
            break;
        }
    }

    if (run_benchmarks) {
        benchmark::Initialize(&argc, argv);
        if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        return 0;
    }
    else 
    {
        samaritan::Engine engine;
        engine.launch();
    }

    return 0;
}