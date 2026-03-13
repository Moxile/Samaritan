#pragma once
#include "movegen.h"

static constexpr int MAX_PLY = 64;

struct SearchInfo {
    Move pv_table[MAX_PLY][MAX_PLY];
    int pv_length[MAX_PLY];
};

static int negaMax(Position& pos, int depth, int ply, SearchInfo& info, int alpha=-1000000, int beta=1000000) {
    info.pv_length[ply] = 0;
    if (depth == 0)
    {
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

static SearchInfo iterativeDeepening(Position& pos, int ply) {
    SearchInfo info;
    for(int depth = 1; depth <= ply; depth++) {
        info.pv_length[0] = 0;
        int score = negaMax(pos, depth, 0, info);
        std::cout << "info depth " << depth << " score cp " << score << " pv ";
            for (int i = 0; i < info.pv_length[0]; i++)
                std::cout << info.pv_table[0][i].toUCI() << " ";
            std::cout << std::endl;
    }

    return info;
}