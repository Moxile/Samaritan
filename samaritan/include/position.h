#pragma once

#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "chess.h"
#include "nnue/nnue.h"
#include "transpositiontable.h"

class Board
{
public:
    alignas(64) PieceType pieceMailbox[224];
    PieceColor colorMailbox[224];
    int kingTracker[4] = {-1, -1, -1, -1};
    int nonPawnPieceCount[4] = {0, 0, 0, 0};

    // Occupied squares per colour, so move generation iterates ~16 entries
    // instead of scanning all 224 mailbox squares. pieceIdx maps a square back
    // to its slot, which makes removal an O(1) swap-and-pop.
    int pieceList[4][32] = {};
    int pieceCount[4] = {0, 0, 0, 0};
    int pieceIdx[224] = {};

    Board()
    {
        std::memset(colorMailbox, NONE_COLOR, sizeof(colorMailbox));
        std::memset(pieceMailbox, NONE_PIECE, sizeof(pieceMailbox));
    }

    inline void listAdd(int sq, PieceColor col)
    {
        const int c = __builtin_ctz((unsigned int)col);
        pieceIdx[sq] = pieceCount[c];
        pieceList[c][pieceCount[c]++] = sq;
    }

    inline void listRemove(int sq, PieceColor col)
    {
        const int c = __builtin_ctz((unsigned int)col);
        const int i = pieceIdx[sq];
        const int last = pieceList[c][--pieceCount[c]];
        pieceList[c][i] = last;
        pieceIdx[last] = i;
    }

    inline void listMove(int from, int to, PieceColor col)
    {
        const int c = __builtin_ctz((unsigned int)col);
        const int i = pieceIdx[from];
        pieceList[c][i] = to;
        pieceIdx[to] = i;
    }

    // Rebuild from the mailboxes. Used after FEN loading and by test helpers
    // that populate the board directly.
    void rebuildPieceList()
    {
        for (int c = 0; c < 4; ++c) pieceCount[c] = 0;
        for (int sq = 0; sq < 224; ++sq)
            if (pieceMailbox[sq] != NONE_PIECE && colorMailbox[sq] != NONE_COLOR)
                listAdd(sq, colorMailbox[sq]);
    }

    // Debug helper: does the incremental list still agree with the mailboxes?
    bool pieceListConsistent() const
    {
        int seen[4] = {0, 0, 0, 0};
        for (int sq = 0; sq < 224; ++sq)
            if (pieceMailbox[sq] != NONE_PIECE && colorMailbox[sq] != NONE_COLOR)
                seen[__builtin_ctz((unsigned int)colorMailbox[sq])]++;
        for (int c = 0; c < 4; ++c)
        {
            if (seen[c] != pieceCount[c]) return false;
            for (int i = 0; i < pieceCount[c]; ++i)
            {
                const int sq = pieceList[c][i];
                if (pieceMailbox[sq] == NONE_PIECE) return false;
                if (__builtin_ctz((unsigned int)colorMailbox[sq]) != c) return false;
                if (pieceIdx[sq] != i) return false;
            }
        }
        return true;
    }

    constexpr bool isEmpty(int loc) const
    {
        return pieceMailbox[loc] == NONE_PIECE;
    }

    // Is `square` attacked by anyone not on `color`'s team?
    //
    // Structured as three cheap phases instead of one generic 16-direction ray
    // walk: knights are direct probes, and king/pawn attacks can only come from
    // an adjacent square, so they are folded into the first step of each ray.
    // `baseColor` is accepted for call-site compatibility but is redundant --
    // a friendly piece blocks a ray either way, since the walk stops at the
    // first occupied square regardless of its colour.
    bool isSquareAttacked(int square, PieceColor color, PieceColor /*baseColor*/ = NONE_COLOR) const
    {
        const PieceColor myTeam = getTeam(color);

        // --- knights: eight direct probes, no walking ---
        for (int i = 0; i < 8; ++i)
        {
            const int n = square + knightOffsets[i];
            if (isInvalidLocation(n)) continue;
            if (pieceMailbox[n] == KNIGHT && !isOnTeam(colorMailbox[n], myTeam))
                return true;
        }

        // --- eight rays: sliders, plus king and pawn on the first step only ---
        for (int i = 0; i < 8; ++i)
        {
            const int dir = kingOffsets[i];
            const bool diag = (dir == -17 || dir == -15 || dir == 15 || dir == 17);
            const PieceType slider = diag ? BISHOP : ROOK;

            const int endSq = rayEndTable[square][i];
            if (endSq == square) continue;          // no room in this direction

            int n = square + dir;
            PieceType pie = pieceMailbox[n];
            if (pie != NONE_PIECE)
            {
                const PieceColor col = colorMailbox[n];
                if (!isOnTeam(col, myTeam))
                {
                    if (pie == QUEEN || pie == slider || pie == KING) return true;
                    if (pie == PAWN)
                    {
                        // A pawn on `n` attacks `square` if one of its own two
                        // capture offsets lands there.
                        const int c = __builtin_ctz((unsigned int)col) + 1;
                        if (n + offsets[c][1] == square || n + offsets[c][2] == square)
                            return true;
                    }
                }
                continue;   // ray is blocked at the first square
            }

            while (n != endSq)
            {
                n += dir;
                pie = pieceMailbox[n];
                if (pie == NONE_PIECE) continue;
                if (!isOnTeam(colorMailbox[n], myTeam) && (pie == QUEEN || pie == slider))
                    return true;
                break;
            }
        }
        return false;
    }
};

// Check and pin state for the side to move, derived from the board and cached
// per position. Computing this once per node is what lets move generation skip
// the make/unmake legality test on the overwhelming majority of moves.
//
// A king has exactly 8 sliding rays, so at most 8 of our pieces can be pinned
// and a compact list beats any per-square table we would have to copy or clear.
struct CheckInfo
{
    int  checkerSq   = -1;   // square of the checker when exactly one gives check
    int  checkerDir  = 0;    // ray direction ksq -> checkerSq, 0 if not a slider
    int  checkerCount = 0;   // 0, 1, or 2 (2 meaning "two or more" -> king must move)
    int  pinSq[8]    = {};   // our own pieces that are pinned to the king
    int  pinDir[8]   = {};   // the ray each pinned piece is stuck on (both signs legal)
    int  pinCount    = 0;
    bool valid       = false;

    // Returns the pin ray for a piece, or 0 when it is free to move anywhere.
    constexpr int pinRayOf(int sq) const
    {
        for (int i = 0; i < pinCount; ++i)
            if (pinSq[i] == sq) return pinDir[i];
        return 0;
    }
};

class GameState
{
public:
    PieceColor curTurn;
    int posScore = 0;
    Move bestMove;
    int castleRights = NO_CASTLING;
    int enpassants[4] = {-1 ,-1 ,-1, -1};
    PieceType lastCapturedPiece = NONE_PIECE;
    PieceColor lastCapturedPieceColor = NONE_COLOR;
    uint64_t zobristKey = 0;

    // Computed on first use by movegen, not by move(): a node that gets cut off
    // before it generates anything never pays for this.
    CheckInfo checkInfo;
};

class Position
{
public:
    Board board;
    std::vector<GameState> gameStates;
    const bool useEval;
    NNUE nnue;

    Position(bool eval = true) : board(), nnue(), useEval(eval) {}

    void refreshNNUE()
    {
        for (auto &acc : nnue.accumulators)
            std::fill(acc.input.begin(), acc.input.end(), 0);
        for (int sq = 0; sq < 224; sq++)
        {
            if (board.pieceMailbox[sq] == NONE_PIECE || board.pieceMailbox[sq] == KING) continue;
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    if (board.kingTracker[k] != -1)
                        acc.input[acc.get_board_feat(sq, board.pieceMailbox[sq], board.colorMailbox[sq], board.kingTracker[k], static_cast<PieceColor>(1 << k))] = 1;
        }
    }

    void makeNullMove()
    {
        GameState state = GameState();
        state.castleRights = gameStates.back().castleRights;
        state.curTurn = gameStates.back().curTurn;
        state.curTurn++;

        state.zobristKey = gameStates.back().zobristKey;
        state.zobristKey ^= zobristTurn[__builtin_ctz((unsigned int)state.curTurn)];
        state.zobristKey ^= zobristTurn[__builtin_ctz((unsigned int)gameStates.back().curTurn)];


        // en passant
        std::memcpy(state.enpassants,
            gameStates.back().enpassants,
            sizeof(state.enpassants));
        state.enpassants[__builtin_ctz((unsigned int)state.curTurn)] = -1;
        gameStates.push_back(state);
    }

    void undoNullMove()
    {
        gameStates.pop_back();
    }

    void move(const Move &move)
    {
        const auto loc = move.from();
        const auto destination = move.to();
        const auto movingPiece = board.pieceMailbox[loc];
        // Colour of the mover, captured before the board is updated below.
        const auto movingColor = board.colorMailbox[loc];
        const auto movingColorIdx = __builtin_ctz((unsigned int)movingColor);
        // Live view of the destination square: the captured piece before the
        // move, the moving piece (or its promotion) afterwards.
        auto &destPiece = board.pieceMailbox[destination];

        auto setFeat = [&](int sq, PieceType pie, PieceColor col) {
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    acc.set(acc.get_board_feat(sq, pie, col, board.kingTracker[k], static_cast<PieceColor>(1 << k)));
        };

        // Create the new game state
        GameState oldState = gameStates.back();
        GameState state = GameState();
        state.castleRights = oldState.castleRights;
        state.curTurn = oldState.curTurn;
        state.curTurn++;
        state.lastCapturedPiece = board.pieceMailbox[destination];
        state.lastCapturedPieceColor = board.colorMailbox[destination];

        state.zobristKey = oldState.zobristKey;

        // turn
        state.zobristKey ^= zobristTurn[__builtin_ctz((unsigned int)state.curTurn)];
        state.zobristKey ^= zobristTurn[__builtin_ctz((unsigned int)oldState.curTurn)];

        if (useEval)
        {
            if(state.lastCapturedPiece != NONE_PIECE)
            {
                if(state.lastCapturedPiece != KING)
                {
                    for(auto &accumulator : nnue.accumulators)
                    {
                        for (int k = 0; k < 4; k++)
                        {
                            accumulator.set(accumulator.get_board_feat(destination, state.lastCapturedPiece, state.lastCapturedPieceColor, board.kingTracker[k], static_cast<PieceColor>(1 << k)));
                        }
                    }
                }

                // for null move pruning
                if(state.lastCapturedPiece != PAWN)
                {
                    board.nonPawnPieceCount[__builtin_ctz((unsigned int) state.lastCapturedPieceColor)]--;
                }
            }
        }

        std::memcpy(state.enpassants,
            oldState.enpassants,
            sizeof(state.enpassants));
        state.enpassants[__builtin_ctz((unsigned int)state.curTurn)] = -1;

        // Piece list: drop the captured piece, then relocate the mover. Both
        // must happen while the mailboxes still describe the old position.
        if (state.lastCapturedPiece != NONE_PIECE)
            board.listRemove(destination, state.lastCapturedPieceColor);
        board.listMove(loc, destination, movingColor);

        // Move the piece
        board.pieceMailbox[destination] = board.pieceMailbox[loc];
        board.colorMailbox[destination] = board.colorMailbox[loc];
        board.pieceMailbox[loc] = NONE_PIECE;
        board.colorMailbox[loc] = NONE_COLOR;

        auto special = move.special_move();
        // Handle Promotion
        if(special == 1 || special == 3)
        {
            destPiece = static_cast<PieceType>(move.promotion());
        }
        // Check castling
        else if (special == 4)
        {
            switch (movingColor)
            {
            case RED:
                if (destination == K14)
                {
                    state.zobristKey ^= zobristPieces[board_table[L14]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[J14]][ROOK-1][movingColorIdx];
                    board.listMove(L14, J14, RED);
                    board.pieceMailbox[J14] = ROOK;
                    board.colorMailbox[J14] = RED;
                    board.pieceMailbox[L14] = NONE_PIECE;
                    board.colorMailbox[L14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    state.zobristKey ^= zobristPieces[board_table[E14]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[H14]][ROOK-1][movingColorIdx];
                    board.listMove(E14, H14, RED);
                    board.pieceMailbox[H14] = ROOK;
                    board.colorMailbox[H14] = RED;
                    board.pieceMailbox[E14] = NONE_PIECE;
                    board.colorMailbox[E14] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == B6)
                {
                    state.zobristKey ^= zobristPieces[board_table[B4]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[B7]][ROOK-1][movingColorIdx];
                    board.listMove(B4, B7, BLUE);
                    board.pieceMailbox[B7] = ROOK;
                    board.colorMailbox[B7] = BLUE;
                    board.pieceMailbox[B4] = NONE_PIECE;
                    board.colorMailbox[B4] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    state.zobristKey ^= zobristPieces[board_table[B11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[B9]][ROOK-1][movingColorIdx];
                    board.listMove(B11, B9, BLUE);
                    board.pieceMailbox[B9] = ROOK;
                    board.colorMailbox[B9] = BLUE;
                    board.pieceMailbox[B11] = NONE_PIECE;
                    board.colorMailbox[B11] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == F1)
                {
                    state.zobristKey ^= zobristPieces[board_table[E1]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[G1]][ROOK-1][movingColorIdx];
                    board.listMove(E1, G1, YELLOW);
                    board.pieceMailbox[G1] = ROOK;
                    board.colorMailbox[G1] = YELLOW;
                    board.pieceMailbox[E1] = NONE_PIECE;
                    board.colorMailbox[E1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    state.zobristKey ^= zobristPieces[board_table[L1]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[I1]][ROOK-1][movingColorIdx];
                    board.listMove(L1, I1, YELLOW);
                    board.pieceMailbox[I1] = ROOK;
                    board.colorMailbox[I1] = YELLOW;
                    board.pieceMailbox[L1] = NONE_PIECE;
                    board.colorMailbox[L1] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == O5)
                {
                    state.zobristKey ^= zobristPieces[board_table[O4]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[O6]][ROOK-1][movingColorIdx];
                    board.listMove(O4, O6, GREEN);
                    board.pieceMailbox[O6] = ROOK;
                    board.colorMailbox[O6] = GREEN;
                    board.pieceMailbox[O4] = NONE_PIECE;
                    board.colorMailbox[O4] = NONE_COLOR;
                }
                else if (destination == O9)
                {
                    state.zobristKey ^= zobristPieces[board_table[O11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[O8]][ROOK-1][movingColorIdx];
                    board.listMove(O11, O8, GREEN);
                    board.pieceMailbox[O8] = ROOK;
                    board.colorMailbox[O8] = GREEN;
                    board.pieceMailbox[O11] = NONE_PIECE;
                    board.colorMailbox[O11] = NONE_COLOR;
                }
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Update castling rights when king is moved
        if (destPiece == KING)
        {
            switch (movingColor)
            {
            case RED:
                state.castleRights = oldState.castleRights & ~RED_CASTLING;
                board.kingTracker[0] = destination;
                break;
            case BLUE:
                state.castleRights = oldState.castleRights & ~BLUE_CASTLING;
                board.kingTracker[1] = destination;
                break;
            case YELLOW:
                state.castleRights = oldState.castleRights & ~YELLOW_CASTLING;
                board.kingTracker[2] = destination;
                break;
            case GREEN:
                state.castleRights = oldState.castleRights & ~GREEN_CASTLING;
                board.kingTracker[3] = destination;
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }
        // Update castling right when rook is moved
        else if (destPiece == ROOK || state.lastCapturedPiece == ROOK)
        {
            int rook = destination;
            if(destPiece == ROOK)
            {
                rook = loc;
            }
            switch (movingColor)
            {
            case RED:
                if (rook == L14)
                    state.castleRights = oldState.castleRights & ~RED_OO;
                else if (rook == E14)
                    state.castleRights = oldState.castleRights & ~RED_OOO;
                break;
            case BLUE:
                if (rook == B4)
                    state.castleRights = oldState.castleRights & ~BLUE_OO;
                else if (rook == B11)
                    state.castleRights = oldState.castleRights & ~BLUE_OOO;
                break;
            case YELLOW:
                if (rook == E1)
                    state.castleRights = oldState.castleRights & ~YELLOW_OO;
                else if (rook == L1)
                    state.castleRights = oldState.castleRights & ~YELLOW_OOO;
                break;
            case GREEN:
                if (rook == O4)
                    state.castleRights = oldState.castleRights & ~GREEN_OO;
                else if (rook == O11)
                    state.castleRights = oldState.castleRights & ~GREEN_OOO;
                break;
            default:
                    throw std::runtime_error("Unknown color given");
            }
        }
        // Handle en passant
        else if(destPiece == PAWN)
        {
            if (special == 2 || special == 3)
            {
                switch(movingColor)
                {
                    case RED:
                        state.zobristKey ^= zobristPieces[board_table[loc + NORTH]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + NORTH])];
                        if(useEval) setFeat(loc + NORTH, PAWN, board.colorMailbox[loc + NORTH]);
                        board.listRemove(loc + NORTH, board.colorMailbox[loc + NORTH]);
                        board.pieceMailbox[loc + NORTH] = NONE_PIECE;
                        board.colorMailbox[loc + NORTH] = NONE_COLOR;
                        break;
                    case BLUE:
                        state.zobristKey ^= zobristPieces[board_table[loc + EAST]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + EAST])];
                        if(useEval) setFeat(loc + EAST, PAWN, board.colorMailbox[loc + EAST]);
                        board.listRemove(loc + EAST, board.colorMailbox[loc + EAST]);
                        board.pieceMailbox[loc + EAST] = NONE_PIECE;
                        board.colorMailbox[loc + EAST] = NONE_COLOR;
                        break;
                    case YELLOW:
                        state.zobristKey ^= zobristPieces[board_table[loc + SOUTH]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + SOUTH])];
                        if(useEval) setFeat(loc + SOUTH, PAWN, board.colorMailbox[loc + SOUTH]);
                        board.listRemove(loc + SOUTH, board.colorMailbox[loc + SOUTH]);
                        board.pieceMailbox[loc + SOUTH] = NONE_PIECE;
                        board.colorMailbox[loc + SOUTH] = NONE_COLOR;
                        break;
                    case GREEN:
                        state.zobristKey ^= zobristPieces[board_table[loc + WEST]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + WEST])];
                        if(useEval) setFeat(loc + WEST, PAWN, board.colorMailbox[loc + WEST]);
                        board.listRemove(loc + WEST, board.colorMailbox[loc + WEST]);
                        board.pieceMailbox[loc + WEST] = NONE_PIECE;
                        board.colorMailbox[loc + WEST] = NONE_COLOR;
                        break;
                    default:
                        throw std::runtime_error("Unknown color given");
                }
            }
            else
            {
                switch(destination - loc)
                {
                    case 2*NORTH:
                        state.enpassants[0] = loc + NORTH;
                        break;
                    case 2*EAST:
                        state.enpassants[1] = loc + EAST;
                        break;
                    case 2*SOUTH:
                        state.enpassants[2] = loc + SOUTH;
                        break;
                    case 2*WEST:
                        state.enpassants[3] = loc + WEST;
                        break;
                }
            }
        }

        if(useEval) 
        {
            if (destPiece == KING)
            {
                refreshNNUE();
                nnue.init_eval(state.curTurn);
            }
            else
            {
                setFeat(loc, movingPiece, movingColor);
                setFeat(destination, destPiece, movingColor);
            }
        }

        // Zobrist: moving piece off origin, onto destination (promotion handled automatically)
        state.zobristKey ^= zobristPieces[board_table[loc]][movingPiece-1][movingColorIdx];
        state.zobristKey ^= zobristPieces[board_table[destination]][destPiece-1][movingColorIdx];

        // Zobrist: captured piece at destination
        if (state.lastCapturedPiece != NONE_PIECE)
        {
            int capIdx = __builtin_ctz((unsigned int)state.lastCapturedPieceColor);
            state.zobristKey ^= zobristPieces[board_table[destination]][state.lastCapturedPiece-1][capIdx];
        }

        // Zobrist: castling rights — XOR only bits that changed
        int changedRights = oldState.castleRights ^ state.castleRights;
        for (int i = 0; i < 8; i++)
            if (changedRights & (1 << i))
                state.zobristKey ^= zobristCastle[i];

        // Zobrist: en passant squares — XOR out old, XOR in new
        for (int player = 0; player < 4; player++)
        {
            if (oldState.enpassants[player] != -1)
                state.zobristKey ^= zobristEnPassant[player][board_table[oldState.enpassants[player]]];
            if (state.enpassants[player] != -1)
                state.zobristKey ^= zobristEnPassant[player][board_table[state.enpassants[player]]];
        }

        gameStates.push_back(state);
    }

    void undoMove(const Move &move)
    {
        const auto loc = move.from();
        const auto destination = move.to();
        const auto pieceAtDest = board.pieceMailbox[destination];
        const auto colorAtDest = board.colorMailbox[destination];
        // Live view of the origin square, which the mover is restored onto below.
        auto &fromPiece = board.pieceMailbox[loc];
        auto &fromColor = board.colorMailbox[loc];

        auto setFeat = [&](int sq, PieceType pie, PieceColor col) {
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    acc.set(acc.get_board_feat(sq, pie, col, board.kingTracker[k], static_cast<PieceColor>(1 << k)));
        };

        // Restore the old piece position
        GameState last = gameStates.back();
        gameStates.pop_back();
        // Piece list: walk the mover back, then restore anything it captured.
        board.listMove(destination, loc, board.colorMailbox[destination]);
        if (last.lastCapturedPiece != NONE_PIECE)
            board.listAdd(destination, last.lastCapturedPieceColor);

        fromPiece = board.pieceMailbox[destination];
        fromColor = board.colorMailbox[destination];
        board.pieceMailbox[destination] = last.lastCapturedPiece;
        board.colorMailbox[destination] = last.lastCapturedPieceColor;
        if(useEval)
        {
            if (last.lastCapturedPiece != NONE_PIECE)
            {
                if(last.lastCapturedPiece != KING)
                {
                    setFeat(destination, last.lastCapturedPiece, last.lastCapturedPieceColor);
                }

                // for null move pruning
                if(last.lastCapturedPiece != PAWN)
                {
                    board.nonPawnPieceCount[__builtin_ctz((unsigned int) last.lastCapturedPieceColor)]++;
                }
            }
        }
        // Restore promotion
        if (move.special_move() == 1 || move.special_move() == 3)
        {
            fromPiece = PAWN;
        }

        // Restore rook when castling
        if (move.special_move() == 4)
        {
            switch (fromColor)
            {
            case RED:
                if (destination == K14)
                {
                    board.listMove(J14, L14, RED);
                    board.pieceMailbox[L14] = ROOK;
                    board.colorMailbox[L14] = RED;
                    board.pieceMailbox[J14] = NONE_PIECE;
                    board.colorMailbox[J14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    board.listMove(H14, E14, RED);
                    board.pieceMailbox[E14] = ROOK;
                    board.colorMailbox[E14] = RED;
                    board.pieceMailbox[H14] = NONE_PIECE;
                    board.colorMailbox[H14] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == B6)
                {
                    board.listMove(B7, B4, BLUE);
                    board.pieceMailbox[B4] = ROOK;
                    board.colorMailbox[B4] = BLUE;
                    board.pieceMailbox[B7] = NONE_PIECE;
                    board.colorMailbox[B7] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    board.listMove(B9, B11, BLUE);
                    board.pieceMailbox[B11] = ROOK;
                    board.colorMailbox[B11] = BLUE;
                    board.pieceMailbox[B9] = NONE_PIECE;
                    board.colorMailbox[B9] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == F1)
                {
                    board.listMove(G1, E1, YELLOW);
                    board.pieceMailbox[E1] = ROOK;
                    board.colorMailbox[E1] = YELLOW;
                    board.pieceMailbox[G1] = NONE_PIECE;
                    board.colorMailbox[G1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    board.listMove(I1, L1, YELLOW);
                    board.pieceMailbox[L1] = ROOK;
                    board.colorMailbox[L1] = YELLOW;
                    board.pieceMailbox[I1] = NONE_PIECE;
                    board.colorMailbox[I1] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == O5)
                {
                    board.listMove(O6, O4, GREEN);
                    board.pieceMailbox[O4] = ROOK;
                    board.colorMailbox[O4] = GREEN;
                    board.pieceMailbox[O6] = NONE_PIECE;
                    board.colorMailbox[O6] = NONE_COLOR;
                }
                else if (destination == O9)
                {
                    board.listMove(O8, O11, GREEN);
                    board.pieceMailbox[O11] = ROOK;
                    board.colorMailbox[O11] = GREEN;
                    board.pieceMailbox[O8] = NONE_PIECE;
                    board.colorMailbox[O8] = NONE_COLOR;
                }
                break;
            
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Restore en passant
        else if(move.special_move() == 2 || move.special_move() == 3)
        {
            switch(fromColor)
            {
                case RED:
                    if(destination == loc + NORTH + WEST)
                    {
                        board.pieceMailbox[loc + NORTH] = PAWN;
                        board.colorMailbox[loc + NORTH] = BLUE;
                        board.listAdd(loc + NORTH, BLUE);
                    }
                    else
                    {
                       board.pieceMailbox[loc + NORTH] = PAWN;
                       board.colorMailbox[loc + NORTH] = GREEN;
                       board.listAdd(loc + NORTH, GREEN);
                    }
                    if(useEval) setFeat(loc + NORTH, PAWN, board.colorMailbox[loc + NORTH]);
                    break;
                case BLUE:
                    if(destination == loc + EAST + NORTH)
                    {
                        board.pieceMailbox[loc + EAST] = PAWN;
                        board.colorMailbox[loc + EAST] = YELLOW;
                        board.listAdd(loc + EAST, YELLOW);
                    }
                    else
                    {
                       board.pieceMailbox[loc + EAST] = PAWN;
                       board.colorMailbox[loc + EAST] = RED;
                       board.listAdd(loc + EAST, RED);
                    }
                    if(useEval) setFeat(loc + EAST, PAWN, board.colorMailbox[loc + EAST]);
                    break;
                case YELLOW:
                    if(destination == loc + SOUTH + WEST)
                    {
                        board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = BLUE;
                        board.listAdd(loc + SOUTH, BLUE);
                    }
                    else
                    {
                       board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = GREEN;
                        board.listAdd(loc + SOUTH, GREEN);
                    }
                    if(useEval) setFeat(loc + SOUTH, PAWN, board.colorMailbox[loc + SOUTH]);
                    break;
                case GREEN:
                    if(destination == loc + WEST + NORTH)
                    {
                        board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = YELLOW;
                        board.listAdd(loc + WEST, YELLOW);
                    }
                    else
                    {
                       board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = RED;
                        board.listAdd(loc + WEST, RED);
                    }
                    if(useEval) setFeat(loc + WEST, PAWN, board.colorMailbox[loc + WEST]);
                    break;
                default:
                    throw std::runtime_error("Unknown color given");
            }
        }

        if (fromPiece == KING)
            board.kingTracker[__builtin_ctz((unsigned int)(fromColor))] = loc;

        if(useEval) 
        {
            if (fromPiece == KING)
            {
                refreshNNUE();
                nnue.init_eval(last.curTurn);
            }
            else
            {
                setFeat(loc, fromPiece, fromColor);
                setFeat(destination, pieceAtDest, colorAtDest);
            }
        }
    }
};
