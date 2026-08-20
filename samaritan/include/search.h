#pragma once
#include "movegen.h"
#include "evaluate.h"
#include <chrono>
#include <cstring>

static constexpr int MAX_PLY = 30;

static constexpr int VALUE_INFINITE = 1000000;
static constexpr int VALUE_MATE     =  999999;
// Anything at least this large is a mate score whose distance depends on the
// ply it was found at, and so must be adjusted when it crosses the TT.
static constexpr int VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;

// A mate score is stored relative to the position it was found in, and read
// back relative to the position probing it. Without this a mate found at ply 8
// is reported as the same distance when the same position is reached at ply 2.
static constexpr int valueToTT(int v, int ply) {
    if (v >=  VALUE_MATE_IN_MAX_PLY) return v + ply;
    if (v <= -VALUE_MATE_IN_MAX_PLY) return v - ply;
    return v;
}
static constexpr int valueFromTT(int v, int ply) {
    if (v >=  VALUE_MATE_IN_MAX_PLY) return v - ply;
    if (v <= -VALUE_MATE_IN_MAX_PLY) return v + ply;
    return v;
}

// Terminal states that hold *before* a single move is generated, shared by
// alpha-beta and quiescence. Quiescence used to miss the king capture entirely:
// it generated no moves (movegen correctly refuses to move for a side with a
// missing king), found no captures, and returned the static evaluation -- so a
// queen taking a king at the horizon scored as a queen's worth of material
// instead of mate.
//
// Returns true and writes the score when the node is over.
static bool terminalScore(const Position& pos, int ply, int& score)
{
    // A captured king ends the game for that team immediately: it is scored
    // from the side to move's view, and the side to move is the team that just
    // lost its king. Ply-relative so that a mate found deeper is worth less.
    if (pos.gameStates.back().lastCapturedPiece == KING)
    {
        score = -VALUE_MATE + ply;
        return true;
    }
    if (ply > 0 && isDraw(pos))
    {
        score = 0;
        return true;
    }
    return false;
}

// The score of a node with no legal moves: checkmate for the side to move, or
// a stalemate draw. Also shared, so the two searches cannot drift apart.
static constexpr int noMovesScore(bool check, int ply)
{
    return check ? -VALUE_MATE + ply : 0;
}

struct SearchInfo {
    // Killers live here rather than in a header-level `static`, which gave every
    // translation unit its own copy -- the killers written were not necessarily
    // the ones read.
    Move killers[MAX_PLY][2] = {};
    // One extra row: negaMax reads pv_table[ply + 1] / pv_length[ply + 1]
    // at every node, including the deepest one it is allowed to reach.
    Move pv_table[MAX_PLY + 1][MAX_PLY + 1] = {};
    int pv_length[MAX_PLY + 1] = {};
    long long nodes = 0;
    int seldepth = 0;
};

static int qSearch(Position& pos, int ply, SearchInfo& info, TranspositionTable& tt, int alpha, int beta)
{
    info.nodes++;
    info.seldepth = std::max(info.seldepth, ply);

    int terminal;
    if (terminalScore(pos, ply, terminal)) return terminal;

    // Check evasions below extend the line, so quiescence needs the same hard
    // ceiling alpha-beta has: mate scores are ply-relative and MAX_PLY is the
    // deepest ply for which they stay distinguishable from ordinary scores.
    if (ply >= MAX_PLY) return evaluate(pos);

    const PieceColor curTurn = pos.gameStates.back().curTurn;
    const bool check = inCheck(pos, curTurn);

    // Standing pat means "I could just stop here", which a checked side cannot
    // do -- it has to answer the check, and its best answer may well be quiet.
    // So in check we skip the stand-pat bound and search every legal move.
    if (!check)
    {
        const int standPat = evaluate(pos);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    }

    MoveList moves = MoveList(pos);
    if (moves.size() == 0) return noMovesScore(check, ply);

    for(auto move : moves)
    {
        if(!check && move.gen_type != CAPTURES) continue;

        pos.move(move);
        int score = -qSearch(pos, ply+1, info, tt, -beta, -alpha);
        pos.undoMove(move);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negaMax(Position& pos, int depth, int ply, SearchInfo& info, TranspositionTable& tt, int alpha=-VALUE_INFINITE, int beta=VALUE_INFINITE, bool allowNullMove=true) {
    // Hard ply ceiling: killermoves and the PV table are both sized by MAX_PLY,
    // and extensions could otherwise walk past them.
    if (ply >= MAX_PLY)
    {
        // The parent still reads pv_length[ply], so it has to be defined here.
        info.pv_length[ply] = 0;
        return qSearch(pos, ply, info, tt, alpha, beta);
    }

    info.pv_length[ply] = 0;
    info.seldepth = std::max(info.seldepth, ply);
    info.nodes++;

    // Copied by value, not bound by reference: pos.move() below push_backs onto
    // gameStates, which would invalidate any reference into it.
    const auto curTurn = pos.gameStates.back().curTurn;

    // Terminal states are decided before any cached score is trusted.
    int terminal;
    if (terminalScore(pos, ply, terminal)) return terminal;

    // A PV node must not take a table cutoff: its exact score and its principal
    // variation are the point of searching it.
    const bool isPV = (beta - alpha) > 1;

    uint64_t ttKey = pos.gameStates.back().zobristKey;
    int origAlpha = alpha;
    TTEntry* ttEntry = tt.probe(ttKey);
    if (!isPV && ttEntry && ttEntry->depth >= depth)
    {
        const int ttScore = valueFromTT(ttEntry->score, ply);
        if (ttEntry->flag == TT_EXACT) return ttScore;
        if (ttEntry->flag == TT_LOWER && ttScore >= beta)  return ttScore;
        if (ttEntry->flag == TT_UPPER && ttScore <= alpha) return ttScore;
    }

    if (depth <= 0)
    {
        return qSearch(pos, ply, info, tt, alpha, beta);
    }

    bool check = inCheck(pos, curTurn);
    MoveList moves = MoveList(pos);

    // Checkmate loses instantly for this team; stalemate is scored as a draw.
    if (moves.size() == 0)
        return noMovesScore(check, ply);

    // null move pruning
    if (depth >= 3 && allowNullMove && !check && pos.board.nonPawnPieceCount[ctz((unsigned int)curTurn)] > 1)
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

        if(info.killers[ply][0] == move)
            move.value += 500;
        if(info.killers[ply][1] == move)
            move.value += 500;
    }

    moves.sort();

    Move bestMove = MOVE_NONE;
    for (ExtMove& move : moves) {
        pos.move(move);
        int score = -negaMax(pos, depth - 1, ply + 1, info, tt, -beta, -alpha, true);
        pos.undoMove(move);

        if(score >= beta)
        {
            tt.store(ttKey, valueToTT(beta, ply), move, depth, TT_LOWER);
            if(move.gen_type == QUIETS && info.killers[ply][0] != move && info.killers[ply][1] != move)
            {
                info.killers[ply][1] = info.killers[ply][0];
                info.killers[ply][0] = move;
            }
            return beta;
        }
        if(score > alpha)
        {
            bestMove = move;
            alpha = score;

            info.pv_table[ply][ply] = move;
            std::memcpy(info.pv_table[ply] + ply + 1, info.pv_table[ply + 1] + ply + 1, info.pv_length[ply + 1] * sizeof(Move));
            info.pv_length[ply] = info.pv_length[ply + 1] + 1;
        }
    }

    TTFlag flag = (alpha == origAlpha) ? TT_UPPER : TT_EXACT;
    tt.store(ttKey, valueToTT(alpha, ply), bestMove, depth, flag);

    return alpha;
}

static SearchInfo iterativeDeepening(Position& pos, TranspositionTable& tt, int maxDepth, bool silent = false) {
    SearchInfo info;
    auto start = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= maxDepth; depth++) {
        info.pv_length[0] = 0;
        info.seldepth = 0;
        // `nodes` is cumulative for the whole search, so that it covers the same
        // interval as the elapsed time it is divided by.

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
