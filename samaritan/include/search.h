#pragma once
#include "movegen.h"
#include <chrono>

static constexpr int MAX_PLY = 64;

struct SearchInfo {
    Move pv_table[MAX_PLY][MAX_PLY];
    int pv_length[MAX_PLY];
    int seldepth = 0;
    long long nodes = 0;
};

static int negaMax(Position& pos, int depth, int ply, SearchInfo& info, int alpha=-1000000, int beta=1000000) {
    info.pv_length[ply] = 0;
    info.seldepth = std::max(info.seldepth, ply);
    if (depth == 0)
    {
        info.nodes++;
        pos.nnue.incremental_update(pos.gameStates.back().curTurn);
        return pos.nnue.evaluation;
    }
    MoveList moves = MoveList(pos);
    moves.sort();
    for (Move move : moves) {
        pos.move(move);
        int score = -negaMax(pos, depth - 1, ply + 1, info, -beta, -alpha);
        pos.undoMove(move);
        if(score >= beta)
        {
            return beta;
        }
        if(score > alpha)
        {
            pos.gameStates.back().bestMove = move;
            alpha = score;

            info.pv_table[ply][ply] = move;
            memcpy(info.pv_table[ply] + ply + 1, info.pv_table[ply + 1] + ply + 1, info.pv_length[ply + 1] * sizeof(Move));
            info.pv_length[ply] = info.pv_length[ply + 1] + 1;
        }
    }
    return alpha;
}

static SearchInfo iterativeDeepening(Position& pos, int maxDepth) {
    SearchInfo info;
    auto start = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= maxDepth; depth++) {
        info.pv_length[0] = 0;
        info.seldepth = 0;
        info.nodes = 0;

        int score = negaMax(pos, depth, 0, info);

        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        long long nps = ms > 0 ? info.nodes * 1000 / ms : 0;

        std::cout << "info depth " << depth
                  << " time " << ms
                  << " seldepth " << info.seldepth
                  << " nodes " << info.nodes
                  << " pv ";
        for (int i = 0; i < info.pv_length[0]; i++)
            std::cout << info.pv_table[0][i].toUCI() << " ";
        std::cout << "score " << score
                  << " nps " << nps
                  << std::endl;
    }

    return info;
}