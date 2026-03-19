#pragma once
#include "movegen.h"
#include <chrono>

static constexpr int MAX_PLY = 30;
static Move killermoves[MAX_PLY][2];

struct SearchInfo {
    Move pv_table[MAX_PLY][MAX_PLY];
    int pv_length[MAX_PLY];
    long long nodes = 0;
    int seldepth = 0;
};

static int qSearch(Position& pos, int ply, SearchInfo& info, TranspositionTable& tt, int alpha, int beta)
{
    info.nodes++;
    pos.nnue.incremental_update(pos.gameStates.back().curTurn);
    int standPat = pos.nnue.evaluation;

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    MoveList moves = MoveList(pos);
    for(auto move : moves)
    {
        if(move.gen_type != CAPTURES) continue;

        pos.move(move);
        int score = -qSearch(pos, ply+1, info, tt, -beta, -alpha);
        pos.undoMove(move);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negaMax(Position& pos, int depth, int ply, SearchInfo& info, TranspositionTable& tt, int alpha=-1000000, int beta=1000000, bool allowNullMove=true) {
    info.pv_length[ply] = 0;
    info.seldepth = std::max(info.seldepth, ply);
    info.nodes++;

    uint64_t ttKey = pos.gameStates.back().zobristKey;
    int origAlpha = alpha;
    TTEntry* ttEntry = tt.probe(ttKey);
    if (ttEntry && ttEntry->depth >= depth)
    {
        if (ttEntry->flag == TT_EXACT) return ttEntry->score;
        if (ttEntry->flag == TT_LOWER) alpha = std::max(alpha, ttEntry->score);
        if (ttEntry->flag == TT_UPPER) beta  = std::min(beta,  ttEntry->score);
        if (alpha >= beta) return ttEntry->score;
    }

    if (depth == 0)
    {
        return qSearch(pos, ply, info, tt, alpha, beta);
    }

    PieceColor curTurn = pos.gameStates.back().curTurn;
    bool check = inCheck(pos, curTurn);
    MoveList moves = MoveList(pos);

    // check for mate or stalemate
    if(moves.size() == 0)
    {
        if(check)
        {
            return -999999 + ply;
        }
        return 0;
    }

    // null move pruning
    if (depth >= 3 && allowNullMove && !check && pos.board.nonPawnPieceCount[__builtin_ctz((unsigned int)pos.gameStates.back().curTurn)] > 1)
    {
       pos.makeNullMove();
       int score = -negaMax(pos, depth - 3, ply + 1, info, tt, -beta, -beta+1, false);
       pos.undoNullMove();
       if (score >= beta) return beta;
    }

    // apply move scores: TT best move, killer moves
    for(ExtMove& move : moves)
    {
        if (ttEntry && move == ttEntry->bestMove)
            move.value += 10000;

        if(killermoves[ply][0] == move)
            move.value += 500;
        if(killermoves[ply][1] == move)
            move.value += 500;
    }

    moves.sort();

    Move bestMove;
    for (ExtMove& move : moves) {
        pos.move(move);
        int score = -negaMax(pos, depth - 1, ply + 1, info, tt, -beta, -alpha, true);
        pos.undoMove(move);

        if(score >= beta)
        {
            tt.store(ttKey, beta, move, depth, TT_LOWER);
            if(move.gen_type == QUIETS && killermoves[ply][0] != move && killermoves[ply][1] != move)
            {
                killermoves[ply][1] = killermoves[ply][0];
                killermoves[ply][0] = move;
            }
            return beta;
        }
        if(score > alpha)
        {
            bestMove = move;
            pos.gameStates.back().bestMove = move;
            alpha = score;

            info.pv_table[ply][ply] = move;
            memcpy(info.pv_table[ply] + ply + 1, info.pv_table[ply + 1] + ply + 1, info.pv_length[ply + 1] * sizeof(Move));
            info.pv_length[ply] = info.pv_length[ply + 1] + 1;
        }
    }

    TTFlag flag = (alpha == origAlpha) ? TT_UPPER : TT_EXACT;
    tt.store(ttKey, alpha, bestMove, depth, flag);

    return alpha;
}

static SearchInfo iterativeDeepening(Position& pos, TranspositionTable& tt, int maxDepth, bool silent = false) {
    SearchInfo info;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < MAX_PLY; i++)
    {
        killermoves[i][0] = killermoves[i][1] = Move(0, 0, 0, 0);
    }

    for (int depth = 1; depth <= maxDepth; depth++) {
        info.pv_length[0] = 0;
        info.seldepth = 0;
        info.nodes = 0;

        int score = negaMax(pos, depth, 0, info, tt);

        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        long long nps = ms > 0 ? info.nodes * 1000 / ms : 0;

        if (!silent) {
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
    }

    return info;
}
