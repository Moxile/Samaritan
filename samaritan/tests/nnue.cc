#include <benchmark/benchmark.h>
#include "utility.h"
#include "movegen.h"

static const std::string START_FEN =
    "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
    "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
    "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
    "14/"
    "bR,bP,10,gP,gR/"
    "bN,bP,10,gP,gN/"
    "bB,bP,10,gP,gB/"
    "bQ,bP,10,gP,gK/"
    "bK,bP,10,gP,gQ/"
    "bB,bP,10,gP,gB/"
    "bN,bP,10,gP,gN/"
    "bR,bP,10,gP,gR/"
    "14/"
    "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
    "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";

// Benchmark full NNUE refresh from scratch
static void BM_InitEval(benchmark::State& state)
{
    Position pos;
    loadFEN(pos, START_FEN);

    for (auto _ : state)
    {
        pos.nnue.init_eval(pos.gameStates.back().curTurn);
        benchmark::DoNotOptimize(pos.nnue.evaluation);
    }
}
BENCHMARK(BM_InitEval);

// Benchmark a single incremental update after one move
static void BM_IncrementalUpdate(benchmark::State& state)
{
    Position pos;
    loadFEN(pos, START_FEN);
    pos.nnue.init_eval(pos.gameStates.back().curTurn);

    MoveList moves(pos);
    Move m = *moves.begin();

    for (auto _ : state)
    {
        pos.move(m);
        pos.nnue.incremental_update(pos.gameStates.back().curTurn);
        benchmark::DoNotOptimize(pos.nnue.evaluation);
        pos.undoMove(m);
    }
}
BENCHMARK(BM_IncrementalUpdate);

// Benchmark incremental updates across a sequence of moves
static void BM_MoveSequence(benchmark::State& state)
{
    Position pos;
    loadFEN(pos, START_FEN);
    pos.nnue.init_eval(pos.gameStates.back().curTurn);

    // Pre-generate a sequence of 8 moves to play/undo each iteration
    std::vector<Move> sequence;
    Position tmp;
    loadFEN(tmp, START_FEN);
    for (int i = 0; i < 8; i++)
    {
        MoveList moves(tmp);
        if (moves.size() == 0) break;
        sequence.push_back(*moves.begin());
        tmp.move(*moves.begin());
    }

    for (auto _ : state)
    {
        for (const Move& m : sequence)
        {
            pos.move(m);
            pos.nnue.incremental_update(pos.gameStates.back().curTurn);
        }
        benchmark::DoNotOptimize(pos.nnue.evaluation);
        for (int i = (int)sequence.size() - 1; i >= 0; i--)
            pos.undoMove(sequence[i]);
    }
}
BENCHMARK(BM_MoveSequence);

BENCHMARK_MAIN();
