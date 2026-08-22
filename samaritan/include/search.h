#pragma once
#include "movegen.h"
#include "evaluate.h"
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <vector>

static constexpr int MAX_PLY = 30;

static constexpr int VALUE_INFINITE = 1000000;
static constexpr int VALUE_MATE     =  999999;
// Anything at least this large is a mate score whose distance depends on the
// ply it was found at, and so must be adjusted when it crosses the TT.
static constexpr int VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;
static constexpr int HISTORY_SQUARES = 224;
static constexpr int HISTORY_MAX = 32767;
static constexpr int TT_MOVE_BONUS = 2000000;
static constexpr int KILLER_MOVE_BONUS = 50000;
static constexpr int QS_DELTA_MARGIN = 500;

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
    long long qnodes = 0;
    int seldepth = 0;

    // Quiet-move success persists across iterative-deepening iterations. The
    // colour dimension matters in 4PC: the same coordinates can be sensible
    // for one player and nonsense for another.
    std::vector<int> history = std::vector<int>(4 * HISTORY_SQUARES * HISTORY_SQUARES);

    int& historyScore(int colour, const Move& move)
    {
        return history[(colour * HISTORY_SQUARES + move.from()) * HISTORY_SQUARES + move.to()];
    }
};

static void updateHistory(int& entry, int bonus)
{
    bonus = std::clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
    entry += bonus - entry * std::abs(bonus) / HISTORY_MAX;
}

static int qSearch(Position& pos, int ply, SearchInfo& info, TranspositionTable& tt,
                   int alpha, int beta, int previousTo = -1)
{
    info.nodes++;
    info.qnodes++;
    info.seldepth = std::max(info.seldepth, ply);

    int terminal;
    if (terminalScore(pos, ply, terminal)) return terminal;

    // Check evasions below extend the line, so quiescence needs the same hard
    // ceiling alpha-beta has: mate scores are ply-relative and MAX_PLY is the
    // deepest ply for which they stay distinguishable from ordinary scores.
    if (ply >= MAX_PLY) return evaluate(pos);

    const PieceColor curTurn = pos.gameStates.back().curTurn;
    const bool check = inCheck(pos, curTurn);
    const uint64_t ttKey = pos.gameStates.back().zobristKey;
    const int origAlpha = alpha;
    TTEntry* ttEntry = tt.probe(ttKey);
    const bool canStore = !ttEntry || ttEntry->depth <= 0;

    // A depth-zero table result is exactly a previous quiescence result. A
    // deeper entry is at least as informative and can be used as well.
    if (ttEntry && ttEntry->depth >= 0)
    {
        const int ttScore = valueFromTT(ttEntry->score, ply);
        if (ttEntry->flag == TT_EXACT) return ttScore;
        if (ttEntry->flag == TT_LOWER && ttScore >= beta)  return ttScore;
        if (ttEntry->flag == TT_UPPER && ttScore <= alpha) return ttScore;
    }

    // Standing pat means "I could just stop here", which a checked side cannot
    // do -- it has to answer the check, and its best answer may well be quiet.
    // So in check we skip the stand-pat bound and search every legal move.
    int standPat = -VALUE_INFINITE;
    if (!check)
    {
        standPat = evaluate(pos);
        if (standPat >= beta)
        {
            if (canStore)
                tt.store(ttKey, valueToTT(beta, ply), MOVE_NONE, 0, TT_LOWER);
            return beta;
        }
        if (standPat > alpha) alpha = standPat;
    }

    MoveList moves = MoveList(pos);
    if (moves.size() == 0) return noMovesScore(check, ply);

    // Captures already carry MVV-LVA scores from move generation. Previously
    // qsearch ignored them and searched captures in piece-list order, so a
    // forcing king capture or queen capture could be visited last.
    if (ttEntry && !isNone(ttEntry->bestMove))
        for (ExtMove& move : moves)
            if (move == ttEntry->bestMove) move.value += TT_MOVE_BONUS;
    moves.sort();

    Move bestMove = MOVE_NONE;
    int tacticalCount = 0;
    for(auto move : moves)
    {
        const bool promotion = move.special_move() == 1 || move.special_move() == 3;
        if (!check && move.gen_type != CAPTURES && !promotion) continue;
        ++tacticalCount;

        PieceType captured = NONE_PIECE;

        // If even the captured material plus a generous allowance for the HCE
        // mobility swing cannot reach alpha, this capture cannot affect the
        // bound. King captures are terminal and are never pruned.
        if (!check)
        {
            captured = pos.board.pieceMailbox[move.to()];
            if (move.special_move() == 2 || move.special_move() == 3)
                captured = PAWN;

            if (captured != KING)
            {
                int gain = PIECE_VALUE[captured];
                if (promotion)
                    gain += PIECE_VALUE[move.promotion()] - PIECE_VALUE[PAWN];
                if (standPat + gain + QS_DELTA_MARGIN <= alpha) continue;
            }
        }

        pos.move(move);

        // After the two best-ordered captures, only forcing moves and immediate
        // recaptures justify extending the tactical tree. This is deliberately
        // narrower than full SEE pruning: Samaritan does not yet have a 4PC
        // static-exchange evaluator, and a two-player SEE approximation would
        // mishandle the delayed recaptures of alternating four-player turns.
        if (!check && tacticalCount > 2 && captured != KING && !promotion &&
            move.to() != previousTo && alpha > -VALUE_MATE_IN_MAX_PLY)
        {
            const PieceColor nextTurn = pos.gameStates.back().curTurn;
            if (!inCheck(pos, nextTurn))
            {
                pos.undoMove(move);
                continue;
            }
        }

        int score = -qSearch(pos, ply+1, info, tt, -beta, -alpha, move.to());
        pos.undoMove(move);
        if (score >= beta)
        {
            if (canStore)
                tt.store(ttKey, valueToTT(beta, ply), move, 0, TT_LOWER);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            bestMove = move;
        }
    }

    if (canStore)
    {
        const TTFlag flag = alpha == origAlpha ? TT_UPPER : TT_EXACT;
        tt.store(ttKey, valueToTT(alpha, ply), bestMove, 0, flag);
    }
    return alpha;
}

static int negaMax(Position& pos, int depth, int ply, SearchInfo& info,
                   TranspositionTable& tt, int alpha=-VALUE_INFINITE,
                   int beta=VALUE_INFINITE, bool allowNullMove=true,
                   int previousTo=-1) {
    // Hard ply ceiling: killermoves and the PV table are both sized by MAX_PLY,
    // and extensions could otherwise walk past them.
    if (ply >= MAX_PLY)
    {
        // The parent still reads pv_length[ply], so it has to be defined here.
        info.pv_length[ply] = 0;
        return qSearch(pos, ply, info, tt, alpha, beta, previousTo);
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
        return qSearch(pos, ply, info, tt, alpha, beta, previousTo);
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
       int score = -negaMax(pos, depth - 3, ply + 1, info, tt,
                            -beta, -beta+1, false, -1);
       pos.undoNullMove();
       if (score >= beta) return beta;
    }

    // Apply move scores: TT move, tactical MVV-LVA values from movegen,
    // killers, then the history learned by earlier iterations.
    const int colourIndex = ctz(static_cast<unsigned int>(curTurn));
    for(ExtMove& move : moves)
    {
        if (ttEntry && move == ttEntry->bestMove)
            move.value += TT_MOVE_BONUS;

        if(info.killers[ply][0] == move)
            move.value += KILLER_MOVE_BONUS;
        if(info.killers[ply][1] == move)
            move.value += KILLER_MOVE_BONUS / 2;

        if (move.gen_type == QUIETS)
            move.value += info.historyScore(colourIndex, move);
    }

    moves.sort();

    Move bestMove = MOVE_NONE;
    Move quietsTried[MAX_MOVES];
    int quietCount = 0;
    int moveCount = 0;
    for (ExtMove& move : moves) {
        const bool quiet = move.gen_type == QUIETS;
        const bool ttMove = ttEntry && move == ttEntry->bestMove;
        const bool killer = info.killers[ply][0] == move || info.killers[ply][1] == move;

        pos.move(move);
        int score;
        if (moveCount == 0)
        {
            // The best ordered move defines the PV and needs the full window.
            score = -negaMax(pos, depth - 1, ply + 1, info, tt,
                             -beta, -alpha, true, move.to());
        }
        else
        {
            // Late quiet moves at non-PV nodes are unlikely to refute the
            // current bound. Search them one ply shallower first, but always
            // restore full depth if they improve alpha.
            int reduction = 0;
            if (depth >= 3 && moveCount >= 4 && !isPV && !check &&
                quiet && !ttMove && !killer)
            {
                reduction = 1;
                if (depth >= 6 && moveCount >= 8) ++reduction;
                if (depth >= 8 && moveCount >= 16) ++reduction;
                reduction = std::min(reduction, depth - 2);
            }

            score = -negaMax(pos, depth - 1 - reduction, ply + 1, info, tt,
                             -alpha - 1, -alpha, true, move.to());

            if (reduction && score > alpha)
                score = -negaMax(pos, depth - 1, ply + 1, info, tt,
                                 -alpha - 1, -alpha, true, move.to());

            // PVS: only a move that improves a PV node earns a full-window
            // re-search. At a zero-window node beta is alpha+1 already.
            if (isPV && score > alpha && score < beta)
                score = -negaMax(pos, depth - 1, ply + 1, info, tt,
                                 -beta, -alpha, true, move.to());
        }
        pos.undoMove(move);

        if(score >= beta)
        {
            tt.store(ttKey, valueToTT(beta, ply), move, depth, TT_LOWER);
            if(move.gen_type == QUIETS && info.killers[ply][0] != move && info.killers[ply][1] != move)
            {
                info.killers[ply][1] = info.killers[ply][0];
                info.killers[ply][0] = move;

                const int bonus = 16 * depth * depth;
                updateHistory(info.historyScore(colourIndex, move), bonus);
                for (int i = 0; i < quietCount; ++i)
                    updateHistory(info.historyScore(colourIndex, quietsTried[i]), -bonus);
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

        if (quiet) quietsTried[quietCount++] = move;
        ++moveCount;
    }

    TTFlag flag = (alpha == origAlpha) ? TT_UPPER : TT_EXACT;
    tt.store(ttKey, valueToTT(alpha, ply), bestMove, depth, flag);

    return alpha;
}

static SearchInfo iterativeDeepening(Position& pos, TranspositionTable& tt, int maxDepth, bool silent = false) {
    SearchInfo info;
    auto start = std::chrono::steady_clock::now();
    int previousScore = 0;

    for (int depth = 1; depth <= maxDepth; depth++) {
        info.seldepth = 0;
        // `nodes` is cumulative for the whole search, so that it covers the same
        // interval as the elapsed time it is divided by.

        int alpha = -VALUE_INFINITE;
        int beta = VALUE_INFINITE;
        int delta = 75;
        if (depth >= 4 && std::abs(previousScore) < VALUE_MATE_IN_MAX_PLY)
        {
            alpha = std::max(-VALUE_INFINITE, previousScore - delta);
            beta = std::min(VALUE_INFINITE, previousScore + delta);
        }

        int score;
        while (true)
        {
            info.pv_length[0] = 0;
            score = negaMax(pos, depth, 0, info, tt, alpha, beta);

            if (score <= alpha && alpha > -VALUE_INFINITE)
            {
                alpha = std::max(-VALUE_INFINITE, alpha - delta);
                delta *= 2;
                continue;
            }
            if (score >= beta && beta < VALUE_INFINITE)
            {
                beta = std::min(VALUE_INFINITE, beta + delta);
                delta *= 2;
                continue;
            }
            break;
        }
        previousScore = score;

        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        long long nps = ms > 0 ? info.nodes * 1000 / ms : 0;

        if (!silent) {
            std::cout << "info depth " << depth
                      << " time " << ms
                      << " seldepth " << info.seldepth
                      << " nodes " << info.nodes
                      << " qnodes " << info.qnodes
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
