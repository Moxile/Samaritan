#pragma once
#include "movegen.h"

int negaMax(Position& pos, int depth) {
    if (depth == 0)
    {
        pos.nnue.incremental_update();
        return pos.nnue.evaluation;
    } 
    int max = -10000000;
    MoveList moves = MoveList(pos);
    for (Move move : moves) {
        pos.move(move);
        int score = -negaMax(pos, depth - 1);  // Calculate score only once
        pos.undoMove(move);
        
        if (score > max)  // Compare with previously stored max
        {
            max = score;
        }
    }
    return max;
}