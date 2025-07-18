#pragma once

#include <vector>
#include <cstring>
#include <iostream>

#include "chess.h"
#include "nnue/nnue.h"

class Board
{
public:
    PieceColor colorMailbox[224];
    PieceType pieceMailbox[224];
    int kingTracker[4] = {-1, -1, -1, -1};

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
};

class Position
{
public:
    Board board;
    std::vector<GameState> gameStates;
    NNUE nnue = NNUE(10);

    void move(const Move &move)
    {
        int loc = move.from();
        int destination = move.to();

        // Create the new game state
        GameState state = GameState();
        state.curTurn = gameStates.back().curTurn;
        state.curTurn++;
        state.lastCapturedPiece = board.pieceMailbox[destination];
        state.lastCapturedPieceColor = board.colorMailbox[destination];
        if(state.lastCapturedPiece != NONE_PIECE)
        {
            nnue.accumulator.set(get_board_feat(destination, state.lastCapturedPiece, state.lastCapturedPieceColor));
        }
        std::memcpy(state.enpassants,
            gameStates.back().enpassants,
            sizeof(state.enpassants));
        state.enpassants[__builtin_ctz((unsigned int)(board.colorMailbox[loc]))] = -1;

        // Move the piece
        board.pieceMailbox[destination] = board.pieceMailbox[loc];
        board.colorMailbox[destination] = board.colorMailbox[loc];
        board.pieceMailbox[loc] = NONE_PIECE;
        board.colorMailbox[loc] = NONE_COLOR;

        // Handle Promotion
        if(move.special_move() == 1 || move.special_move() == 3)
        {
            board.pieceMailbox[destination] = static_cast<PieceType>(move.promotion());
        }
        // Check castling
        else if (move.special_move() == 4)
        {
            switch (board.colorMailbox[destination])
            {
            case RED:
                if (destination == 218)
                {
                    board.pieceMailbox[217] = ROOK;
                    board.colorMailbox[217] = RED;
                    board.pieceMailbox[219] = NONE_PIECE;
                    board.colorMailbox[219] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(217, ROOK, RED));
                    nnue.accumulator.set(get_board_feat(219, ROOK, RED));
                }
                else if (destination == 214)
                {
                    board.pieceMailbox[215] = ROOK;
                    board.colorMailbox[215] = RED;
                    board.pieceMailbox[212] = NONE_PIECE;
                    board.colorMailbox[212] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(215, ROOK, RED));
                    nnue.accumulator.set(get_board_feat(212, ROOK, RED));
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    board.pieceMailbox[97] = ROOK;
                    board.colorMailbox[97] = BLUE;
                    board.pieceMailbox[49] = NONE_PIECE;
                    board.colorMailbox[49] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(97, ROOK, BLUE));
                    nnue.accumulator.set(get_board_feat(49, ROOK, BLUE));
                }
                else if (destination == 145)
                {
                    board.pieceMailbox[129] = ROOK;
                    board.colorMailbox[129] = BLUE;
                    board.pieceMailbox[161] = NONE_PIECE;
                    board.colorMailbox[161] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(29, ROOK, BLUE));
                    nnue.accumulator.set(get_board_feat(161, ROOK, BLUE));
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    board.pieceMailbox[6] = ROOK;
                    board.colorMailbox[6] = YELLOW;
                    board.pieceMailbox[4] = NONE_PIECE;
                    board.colorMailbox[4] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(6, ROOK, YELLOW));
                    nnue.accumulator.set(get_board_feat(4, ROOK, YELLOW));
                }
                else if (destination == 9)
                {
                    board.pieceMailbox[8] = ROOK;
                    board.colorMailbox[8] = YELLOW;
                    board.pieceMailbox[11] = NONE_PIECE;
                    board.colorMailbox[11] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(8, ROOK, YELLOW));
                    nnue.accumulator.set(get_board_feat(11, ROOK, YELLOW));
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    board.pieceMailbox[94] = ROOK;
                    board.colorMailbox[94] = GREEN;
                    board.pieceMailbox[62] = NONE_PIECE;
                    board.colorMailbox[62] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(94, ROOK, GREEN));
                    nnue.accumulator.set(get_board_feat(62, ROOK, GREEN));
                }
                else if (destination == 142)
                {
                    board.pieceMailbox[126] = ROOK;
                    board.colorMailbox[126] = GREEN;
                    board.pieceMailbox[174] = NONE_PIECE;
                    board.colorMailbox[174] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(126, ROOK, GREEN));
                    nnue.accumulator.set(get_board_feat(174, ROOK, GREEN));
                }
                break;
            }
        }

        // Update castling rights when king is moved
        if (board.pieceMailbox[destination] == KING)
        {
            switch (board.colorMailbox[destination])
            {
            case RED:
                state.castleRights = gameStates.back().castleRights & ~RED_CASTLING;
                board.kingTracker[0] = destination;
                break;
            case BLUE:
                state.castleRights = gameStates.back().castleRights & ~BLUE_CASTLING;
                board.kingTracker[1] = destination;
                break;
            case YELLOW:
                state.castleRights = gameStates.back().castleRights & ~YELLOW_CASTLING;
                board.kingTracker[2] = destination;
                break;
            case GREEN:
                state.castleRights = gameStates.back().castleRights & ~GREEN_CASTLING;
                board.kingTracker[3] = destination;
                break;
            }
        }
        // Update castling right when rook is moved
        else if (board.pieceMailbox[destination] == ROOK)
        {
            switch (board.colorMailbox[destination])
            {
            case RED:
                if (loc == 220)
                    state.castleRights = gameStates.back().castleRights & ~RED_OO;
                else if (loc == 212)
                    state.castleRights = gameStates.back().castleRights & ~RED_OOO;
                break;
            case BLUE:
                if (loc == 49)
                    state.castleRights = gameStates.back().castleRights & ~BLUE_OO;
                else if (loc == 161)
                    state.castleRights = gameStates.back().castleRights & ~BLUE_OOO;
                break;
            case YELLOW:
                if (loc == 4)
                    state.castleRights = gameStates.back().castleRights & ~YELLOW_OO;
                else if (loc == 12)
                    state.castleRights = gameStates.back().castleRights & ~YELLOW_OOO;
                break;
            case GREEN:
                if (loc == 62)
                    state.castleRights = gameStates.back().castleRights & ~GREEN_OO;
                else if (loc == 174)
                    state.castleRights = gameStates.back().castleRights & ~GREEN_OOO;
                break;
            }
        }
        // Handle en passant
        else if(board.pieceMailbox[destination] == PAWN)
        {
            if (move.special_move() == 2 || move.special_move() == 3)
            {
                switch(board.colorMailbox[destination])
                {
                    case RED:
                        nnue.accumulator.set(get_board_feat(loc + NORTH, PAWN, board.colorMailbox[loc + NORTH]));
                        board.pieceMailbox[loc + NORTH] = NONE_PIECE;
                        board.colorMailbox[loc + NORTH] = NONE_COLOR;
                        break;
                    case BLUE:
                    nnue.accumulator.set(get_board_feat(loc + EAST, PAWN, board.colorMailbox[loc + NORTH]));
                        board.pieceMailbox[loc + EAST] = NONE_PIECE;
                        board.colorMailbox[loc + EAST] = NONE_COLOR;
                        break;
                    case YELLOW:
                    nnue.accumulator.set(get_board_feat(loc + SOUTH, PAWN, board.colorMailbox[loc + NORTH]));
                        board.pieceMailbox[loc + SOUTH] = NONE_PIECE;
                        board.colorMailbox[loc + SOUTH] = NONE_COLOR;
                        break;
                    case GREEN:
                    nnue.accumulator.set(get_board_feat(loc + WEST, PAWN, board.colorMailbox[loc + NORTH]));
                        board.pieceMailbox[loc + WEST] = NONE_PIECE;
                        board.colorMailbox[loc + WEST] = NONE_COLOR;
                        break;
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

        nnue.accumulator.set(get_board_feat(loc, board.pieceMailbox[destination], board.colorMailbox[destination]));
        nnue.accumulator.set(get_board_feat(destination, board.pieceMailbox[destination], board.colorMailbox[destination]));
        nnue.accumulator.set(get_turn_feat(state.curTurn));
        

        gameStates.push_back(state);
    }

    void undoMove(const Move &move)
    {
        int loc = move.from();
        int destination = move.to();

        // Restore the old piece position
        board.pieceMailbox[loc] = board.pieceMailbox[destination];
        board.colorMailbox[loc] = board.colorMailbox[destination];
        board.pieceMailbox[destination] = gameStates.back().lastCapturedPiece;
        board.colorMailbox[destination] = gameStates.back().lastCapturedPieceColor;
        nnue.accumulator.set(get_board_feat(destination, gameStates.back().lastCapturedPiece, gameStates.back().lastCapturedPieceColor));

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
                    nnue.accumulator.set(get_board_feat(217, ROOK, RED));
                    nnue.accumulator.set(get_board_feat(219, ROOK, RED));
                }
                else if (destination == 214)
                {
                    board.pieceMailbox[212] = ROOK;
                    board.colorMailbox[212] = RED;
                    board.pieceMailbox[215] = NONE_PIECE;
                    board.colorMailbox[215] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(212, ROOK, RED));
                    nnue.accumulator.set(get_board_feat(215, ROOK, RED));
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    board.pieceMailbox[49] = ROOK;
                    board.colorMailbox[49] = BLUE;
                    board.pieceMailbox[97] = NONE_PIECE;
                    board.colorMailbox[97] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(49, ROOK, BLUE));
                    nnue.accumulator.set(get_board_feat(97, ROOK, BLUE));
                }
                else if (destination == 145)
                {
                    board.pieceMailbox[161] = ROOK;
                    board.colorMailbox[161] = BLUE;
                    board.pieceMailbox[129] = NONE_PIECE;
                    board.colorMailbox[129] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(161, ROOK, BLUE));
                    nnue.accumulator.set(get_board_feat(129, ROOK, BLUE));
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    board.pieceMailbox[4] = ROOK;
                    board.colorMailbox[4] = YELLOW;
                    board.pieceMailbox[6] = NONE_PIECE;
                    board.colorMailbox[6] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(4, ROOK, YELLOW));
                    nnue.accumulator.set(get_board_feat(6, ROOK, YELLOW));
                }
                else if (destination == 9)
                {
                    board.pieceMailbox[11] = ROOK;
                    board.colorMailbox[11] = YELLOW;
                    board.pieceMailbox[8] = NONE_PIECE;
                    board.colorMailbox[8] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(11, ROOK, YELLOW));
                    nnue.accumulator.set(get_board_feat(8, ROOK, YELLOW));
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    board.pieceMailbox[62] = ROOK;
                    board.colorMailbox[62] = GREEN;
                    board.pieceMailbox[94] = NONE_PIECE;
                    board.colorMailbox[94] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(62, ROOK, GREEN));
                    nnue.accumulator.set(get_board_feat(94, ROOK, GREEN));
                }
                else if (destination == 142)
                {
                    board.pieceMailbox[174] = ROOK;
                    board.colorMailbox[174] = GREEN;
                    board.pieceMailbox[126] = NONE_PIECE;
                    board.colorMailbox[126] = NONE_COLOR;
                    nnue.accumulator.set(get_board_feat(174, ROOK, GREEN));
                    nnue.accumulator.set(get_board_feat(126, ROOK, GREEN));
                }
                break;
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
                    break;
            }
        }

        gameStates.pop_back();
        if (board.pieceMailbox[loc] == KING)
            board.kingTracker[__builtin_ctz((unsigned int)(board.colorMailbox[loc]))] = loc;

        nnue.accumulator.set(get_board_feat(loc, board.pieceMailbox[destination], board.colorMailbox[destination]));
        nnue.accumulator.set(get_board_feat(destination, board.pieceMailbox[destination], board.colorMailbox[destination]));
        nnue.accumulator.set(get_turn_feat(gameStates.back().curTurn));
    }
};