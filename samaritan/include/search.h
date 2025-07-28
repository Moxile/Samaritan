#pragma once
#include "movegen.h"

static int negaMax(Position& pos, int depth, int alpha=-1000000, int beta=1000000) {
    if (depth == 0)
    {
        pos.nnue.incremental_update();
        return pos.nnue.evaluation;
    }
    MoveList moves = MoveList(pos);
    for (Move move : moves) {
        pos.move(move);
        int score = -negaMax(pos, depth - 1, -beta, -alpha);
        pos.undoMove(move);
        if(score >= beta)
        {
            return beta;
        }
        if(score > alpha)
        {
            pos.gameStates.back().bestMove = move;
            alpha = score;
        }
    }
    return alpha;
}