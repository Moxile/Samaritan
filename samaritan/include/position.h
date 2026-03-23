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
    PieceColor colorMailbox[224];
    PieceType pieceMailbox[224];
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
        int loc = move.from();
        int destination = move.to();
        PieceType movingPiece = board.pieceMailbox[loc];

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

        // Save moving color before board is updated
        PieceColor movingColor = board.colorMailbox[loc];
        int movingColorIdx = __builtin_ctz((unsigned int)movingColor);

        // Move the piece
        board.pieceMailbox[destination] = board.pieceMailbox[loc];
        board.colorMailbox[destination] = board.colorMailbox[loc];
        board.pieceMailbox[loc] = NONE_PIECE;
        board.colorMailbox[loc] = NONE_COLOR;

        auto special = move.special_move();
        // Handle Promotion
        if(special == 1 || special == 3)
        {
            board.pieceMailbox[destination] = static_cast<PieceType>(move.promotion());
        }
        // Check castling
        else if (special == 4)
        {
            switch (board.colorMailbox[destination])
            {
            case RED:
                if (destination == 218)
                {
                    state.zobristKey ^= zobristPieces[board_table[219]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[217]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[217] = ROOK;
                    board.colorMailbox[217] = RED;
                    board.pieceMailbox[219] = NONE_PIECE;
                    board.colorMailbox[219] = NONE_COLOR;
                }
                else if (destination == 214)
                {
                    state.zobristKey ^= zobristPieces[board_table[212]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[215]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[215] = ROOK;
                    board.colorMailbox[215] = RED;
                    board.pieceMailbox[212] = NONE_PIECE;
                    board.colorMailbox[212] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    state.zobristKey ^= zobristPieces[board_table[49]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[97]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[97] = ROOK;
                    board.colorMailbox[97] = BLUE;
                    board.pieceMailbox[49] = NONE_PIECE;
                    board.colorMailbox[49] = NONE_COLOR;
                }
                else if (destination == 145)
                {
                    state.zobristKey ^= zobristPieces[board_table[161]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[129]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[129] = ROOK;
                    board.colorMailbox[129] = BLUE;
                    board.pieceMailbox[161] = NONE_PIECE;
                    board.colorMailbox[161] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    state.zobristKey ^= zobristPieces[board_table[4]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[6]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[6] = ROOK;
                    board.colorMailbox[6] = YELLOW;
                    board.pieceMailbox[4] = NONE_PIECE;
                    board.colorMailbox[4] = NONE_COLOR;
                }
                else if (destination == 9)
                {
                    state.zobristKey ^= zobristPieces[board_table[11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[8]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[8] = ROOK;
                    board.colorMailbox[8] = YELLOW;
                    board.pieceMailbox[11] = NONE_PIECE;
                    board.colorMailbox[11] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    state.zobristKey ^= zobristPieces[board_table[62]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[94]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[94] = ROOK;
                    board.colorMailbox[94] = GREEN;
                    board.pieceMailbox[62] = NONE_PIECE;
                    board.colorMailbox[62] = NONE_COLOR;
                }
                else if (destination == 142)
                {
                    state.zobristKey ^= zobristPieces[board_table[174]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[126]][ROOK-1][movingColorIdx];
                    board.pieceMailbox[126] = ROOK;
                    board.colorMailbox[126] = GREEN;
                    board.pieceMailbox[174] = NONE_PIECE;
                    board.colorMailbox[174] = NONE_COLOR;
                }
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Update castling rights when king is moved
        if (board.pieceMailbox[destination] == KING)
        {
            switch (board.colorMailbox[destination])
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
        else if (board.pieceMailbox[destination] == ROOK || state.lastCapturedPiece == ROOK)
        {
            int rook = destination;
            if(board.pieceMailbox[destination] == ROOK)
            {
                rook = loc;
            }
            switch (board.colorMailbox[destination])
            {
            case RED:
                if (rook == 220)
                    state.castleRights = oldState.castleRights & ~RED_OO;
                else if (rook == 212)
                    state.castleRights = oldState.castleRights & ~RED_OOO;
                break;
            case BLUE:
                if (rook == 49)
                    state.castleRights = oldState.castleRights & ~BLUE_OO;
                else if (rook == 161)
                    state.castleRights = oldState.castleRights & ~BLUE_OOO;
                break;
            case YELLOW:
                if (rook == 4)
                    state.castleRights = oldState.castleRights & ~YELLOW_OO;
                else if (rook == 12)
                    state.castleRights = oldState.castleRights & ~YELLOW_OOO;
                break;
            case GREEN:
                if (rook == 62)
                    state.castleRights = oldState.castleRights & ~GREEN_OO;
                else if (rook == 174)
                    state.castleRights = oldState.castleRights & ~GREEN_OOO;
                break;
            default:
                    throw std::runtime_error("Unknown color given");
            }
        }
        // Handle en passant
        else if(board.pieceMailbox[destination] == PAWN)
        {
            if (special == 2 || special == 3)
            {
                switch(board.colorMailbox[destination])
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
            if (board.pieceMailbox[destination] == KING)
            {
                refreshNNUE();
                nnue.init_eval(state.curTurn);
            }
            else
            {
                setFeat(loc, movingPiece, board.colorMailbox[destination]);
                setFeat(destination, board.pieceMailbox[destination], board.colorMailbox[destination]);
            }
        }

        // Zobrist: moving piece off origin, onto destination (promotion handled automatically)
        state.zobristKey ^= zobristPieces[board_table[loc]][movingPiece-1][movingColorIdx];
        state.zobristKey ^= zobristPieces[board_table[destination]][board.pieceMailbox[destination]-1][movingColorIdx];

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
        int loc = move.from();
        int destination = move.to();
        PieceType pieceAtDest = board.pieceMailbox[destination];
        PieceColor colorAtDest = board.colorMailbox[destination];

        auto setFeat = [&](int sq, PieceType pie, PieceColor col) {
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    acc.set(acc.get_board_feat(sq, pie, col, board.kingTracker[k], static_cast<PieceColor>(1 << k)));
        };

        // Restore the old piece position
        GameState last = gameStates.back();
        gameStates.pop_back();
        board.pieceMailbox[loc] = board.pieceMailbox[destination];
        board.colorMailbox[loc] = board.colorMailbox[destination];
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
            board.pieceMailbox[loc] = PAWN;
        }

        // Restore rook when castling
        if (move.special_move() == 4)
        {
            switch (board.colorMailbox[loc])
            {
            case RED:
                if (destination == 218)
                {
                    board.pieceMailbox[219] = ROOK;
                    board.colorMailbox[219] = RED;
                    board.pieceMailbox[217] = NONE_PIECE;
                    board.colorMailbox[217] = NONE_COLOR;
                }
                else if (destination == 214)
                {
                    board.pieceMailbox[212] = ROOK;
                    board.colorMailbox[212] = RED;
                    board.pieceMailbox[215] = NONE_PIECE;
                    board.colorMailbox[215] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    board.pieceMailbox[49] = ROOK;
                    board.colorMailbox[49] = BLUE;
                    board.pieceMailbox[97] = NONE_PIECE;
                    board.colorMailbox[97] = NONE_COLOR;
                }
                else if (destination == 145)
                {
                    board.pieceMailbox[161] = ROOK;
                    board.colorMailbox[161] = BLUE;
                    board.pieceMailbox[129] = NONE_PIECE;
                    board.colorMailbox[129] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    board.pieceMailbox[4] = ROOK;
                    board.colorMailbox[4] = YELLOW;
                    board.pieceMailbox[6] = NONE_PIECE;
                    board.colorMailbox[6] = NONE_COLOR;
                }
                else if (destination == 9)
                {
                    board.pieceMailbox[11] = ROOK;
                    board.colorMailbox[11] = YELLOW;
                    board.pieceMailbox[8] = NONE_PIECE;
                    board.colorMailbox[8] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    board.pieceMailbox[62] = ROOK;
                    board.colorMailbox[62] = GREEN;
                    board.pieceMailbox[94] = NONE_PIECE;
                    board.colorMailbox[94] = NONE_COLOR;
                }
                else if (destination == 142)
                {
                    board.pieceMailbox[174] = ROOK;
                    board.colorMailbox[174] = GREEN;
                    board.pieceMailbox[126] = NONE_PIECE;
                    board.colorMailbox[126] = NONE_COLOR;
                }
                break;
            
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Restore en passant
        else if(move.special_move() == 2 || move.special_move() == 3)
        {
            switch(board.colorMailbox[loc])
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

        if (board.pieceMailbox[loc] == KING)
            board.kingTracker[__builtin_ctz((unsigned int)(board.colorMailbox[loc]))] = loc;

        if(useEval) 
        {
            if (board.pieceMailbox[loc] == KING)
            {
                refreshNNUE();
                nnue.init_eval(last.curTurn);
            }
            else
            {
                setFeat(loc, board.pieceMailbox[loc], board.colorMailbox[loc]);
                setFeat(destination, pieceAtDest, colorAtDest);
            }
        }
    }
};
