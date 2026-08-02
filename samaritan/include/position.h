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

    Board()
    {
        std::memset(colorMailbox, NONE_COLOR, sizeof(colorMailbox));
        std::memset(pieceMailbox, NONE_PIECE, sizeof(pieceMailbox));
    }

    constexpr bool isEmpty(int loc) const
    {
        return pieceMailbox[loc] == NONE_PIECE;
    }

    bool isSquareAttacked(int square, PieceColor color, PieceColor baseColor) const
    {
        for (int start = 0; start < offsetsNum[10]; ++start)
        {
            int increment = offsets[10][start];
            for (int n = square + increment;; n += increment)
            {
                if (isInvalidLocation(n))
                {
                    break;
                }
                PieceType pie = pieceMailbox[n];
                PieceColor col = colorMailbox[n];
                if (baseColor != NONE_COLOR && pie != NONE_PIECE && getTeam(col) == baseColor)
                {
                    break;
                }
                if (pie != NONE_PIECE)
                {
                    if (getTeam(colorMailbox[n]) != getTeam(color))
                    {
                        if ((abs(increment) == 16 || abs(increment) == 1) && (pie == ROOK || pie == QUEEN || (pie == KING && n == square + increment)))
                        {
                            return true;
                        }
                        if (pie == PAWN && n == square + increment)
                        {
                            if (colorMailbox[n] == RED && (increment == 15 || increment == 17))
                                return true;
                            else if (colorMailbox[n] == BLUE && (increment == 15 || increment == -17))
                                return true;
                            else if (colorMailbox[n] == YELLOW && (increment == -15 || increment == -17))
                                return true;
                            else if (colorMailbox[n] == GREEN && (increment == -15 || increment == 17))
                                return true;
                        }
                        if ((abs(increment) == 15 || abs(increment) == 17) && (pie == BISHOP || pie == QUEEN || (pie == KING && n == square + increment)))
                        {
                            return true;
                        }
                        if ((abs(increment) == 18 || abs(increment) == 14 || abs(increment) == 31 || abs(increment) == 33) && pie == KNIGHT)
                        {
                            return true;
                        }
                    }
                    break;
                }
                if ((abs(increment) == 18 || abs(increment) == 14 || abs(increment) == 31 || abs(increment) == 33))
                {
                    break;
                }
            }
        }
        return false;
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
                    board.pieceMailbox[J14] = ROOK;
                    board.colorMailbox[J14] = RED;
                    board.pieceMailbox[L14] = NONE_PIECE;
                    board.colorMailbox[L14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    state.zobristKey ^= zobristPieces[board_table[E14]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[H14]][ROOK-1][movingColorIdx];
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
                    board.pieceMailbox[B7] = ROOK;
                    board.colorMailbox[B7] = BLUE;
                    board.pieceMailbox[B4] = NONE_PIECE;
                    board.colorMailbox[B4] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    state.zobristKey ^= zobristPieces[board_table[B11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[B9]][ROOK-1][movingColorIdx];
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
                    board.pieceMailbox[G1] = ROOK;
                    board.colorMailbox[G1] = YELLOW;
                    board.pieceMailbox[E1] = NONE_PIECE;
                    board.colorMailbox[E1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    state.zobristKey ^= zobristPieces[board_table[L1]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[I1]][ROOK-1][movingColorIdx];
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
                    board.pieceMailbox[O6] = ROOK;
                    board.colorMailbox[O6] = GREEN;
                    board.pieceMailbox[O4] = NONE_PIECE;
                    board.colorMailbox[O4] = NONE_COLOR;
                }
                else if (destination == O9)
                {
                    state.zobristKey ^= zobristPieces[board_table[O11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[O8]][ROOK-1][movingColorIdx];
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
                        board.pieceMailbox[loc + NORTH] = NONE_PIECE;
                        board.colorMailbox[loc + NORTH] = NONE_COLOR;
                        break;
                    case BLUE:
                        state.zobristKey ^= zobristPieces[board_table[loc + EAST]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + EAST])];
                        if(useEval) setFeat(loc + EAST, PAWN, board.colorMailbox[loc + EAST]);
                        board.pieceMailbox[loc + EAST] = NONE_PIECE;
                        board.colorMailbox[loc + EAST] = NONE_COLOR;
                        break;
                    case YELLOW:
                        state.zobristKey ^= zobristPieces[board_table[loc + SOUTH]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + SOUTH])];
                        if(useEval) setFeat(loc + SOUTH, PAWN, board.colorMailbox[loc + SOUTH]);
                        board.pieceMailbox[loc + SOUTH] = NONE_PIECE;
                        board.colorMailbox[loc + SOUTH] = NONE_COLOR;
                        break;
                    case GREEN:
                        state.zobristKey ^= zobristPieces[board_table[loc + WEST]][PAWN-1][__builtin_ctz((unsigned int)board.colorMailbox[loc + WEST])];
                        if(useEval) setFeat(loc + WEST, PAWN, board.colorMailbox[loc + WEST]);
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
                    board.pieceMailbox[L14] = ROOK;
                    board.colorMailbox[L14] = RED;
                    board.pieceMailbox[J14] = NONE_PIECE;
                    board.colorMailbox[J14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    board.pieceMailbox[E14] = ROOK;
                    board.colorMailbox[E14] = RED;
                    board.pieceMailbox[H14] = NONE_PIECE;
                    board.colorMailbox[H14] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == B6)
                {
                    board.pieceMailbox[B4] = ROOK;
                    board.colorMailbox[B4] = BLUE;
                    board.pieceMailbox[B7] = NONE_PIECE;
                    board.colorMailbox[B7] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    board.pieceMailbox[B11] = ROOK;
                    board.colorMailbox[B11] = BLUE;
                    board.pieceMailbox[B9] = NONE_PIECE;
                    board.colorMailbox[B9] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == F1)
                {
                    board.pieceMailbox[E1] = ROOK;
                    board.colorMailbox[E1] = YELLOW;
                    board.pieceMailbox[G1] = NONE_PIECE;
                    board.colorMailbox[G1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    board.pieceMailbox[L1] = ROOK;
                    board.colorMailbox[L1] = YELLOW;
                    board.pieceMailbox[I1] = NONE_PIECE;
                    board.colorMailbox[I1] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == O5)
                {
                    board.pieceMailbox[O4] = ROOK;
                    board.colorMailbox[O4] = GREEN;
                    board.pieceMailbox[O6] = NONE_PIECE;
                    board.colorMailbox[O6] = NONE_COLOR;
                }
                else if (destination == O9)
                {
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
                    }
                    else
                    {
                       board.pieceMailbox[loc + NORTH] = PAWN;
                       board.colorMailbox[loc + NORTH] = GREEN;
                    }
                    if(useEval) setFeat(loc + NORTH, PAWN, board.colorMailbox[loc + NORTH]);
                    break;
                case BLUE:
                    if(destination == loc + EAST + NORTH)
                    {
                        board.pieceMailbox[loc + EAST] = PAWN;
                        board.colorMailbox[loc + EAST] = YELLOW;
                    }
                    else
                    {
                       board.pieceMailbox[loc + EAST] = PAWN;
                       board.colorMailbox[loc + EAST] = RED;
                    }
                    if(useEval) setFeat(loc + EAST, PAWN, board.colorMailbox[loc + EAST]);
                    break;
                case YELLOW:
                    if(destination == loc + SOUTH + WEST)
                    {
                        board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = BLUE;
                    }
                    else
                    {
                       board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = GREEN;
                    }
                    if(useEval) setFeat(loc + SOUTH, PAWN, board.colorMailbox[loc + SOUTH]);
                    break;
                case GREEN:
                    if(destination == loc + WEST + NORTH)
                    {
                        board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = YELLOW;
                    }
                    else
                    {
                       board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = RED;
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
