#pragma once

#include <cstdint>
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
    alignas(64) Piece mailbox[256];
    // PieceColor colorMailbox[224];
    // PieceType pieceMailbox[224];
    Square kingTracker[4] = {Square::A1, Square::A1, Square::A1, Square::A1};
    int nonPawnPieceCount[4] = {0, 0, 0, 0};

    Board()
    {
        std::memset(mailbox, NONE_PIECE_COLOR, sizeof(mailbox));
        // std::memset(pieceMailbox, NONE_PIECE, sizeof(pieceMailbox));
    }

    Piece piece(Square sq) const noexcept
    {
        return mailbox[sq];
    }

    PieceType pieceType(Square sq) const noexcept
    {
        return getPieceType(mailbox[sq]);
    }

    PieceColor pieceColor(Square sq) const noexcept
    {
        return getPieceColor(mailbox[sq]);
    }

    constexpr bool isEmpty(Square sq) const
    {
        return mailbox[sq] == NONE_PIECE_COLOR;
    }

    bool isSquareAttacked(Square square, PieceColor color, PieceColor baseColor) const
    {
        for (int start = 0; start < offsetsNum[10]; ++start)
        {
            int increment = offsets[10][start];
            for (uint8_t n = square + increment;; n += increment)
            {
                if (isInvalidLocation(n))
                {
                    break;
                }
                PieceType pie = pieceType(static_cast<Square>(n));
                PieceColor col = pieceColor(static_cast<Square>(n));
                if (baseColor != NONE_COLOR && pie != NONE_PIECE && getTeam(col) == baseColor)
                {
                    break;
                }
                if (pie != NONE_PIECE)
                {
                    if (getTeam(col) != getTeam(color))
                    {
                        if ((abs(increment) == 16 || abs(increment) == 1) && (pie == ROOK || pie == QUEEN || (pie == KING && n == square + increment)))
                        {
                            return true;
                        }
                        if (pie == PAWN && n == square + increment)
                        {
                            if (col == RED && (increment == 15 || increment == 17))
                                return true;
                            else if (col == BLUE && (increment == 15 || increment == -17))
                                return true;
                            else if (col == YELLOW && (increment == -15 || increment == -17))
                                return true;
                            else if (col == GREEN && (increment == -15 || increment == 17))
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
    Square enpassants[4] = {Square::A1 ,Square::A1 ,Square::A1, Square::A1};
    Piece lastCaptured = NONE_PIECE_COLOR;
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
            std::cout << "zzzzzzzzz" << std::endl;
            for (int s = 0; s < 256; s++)
        {
            Square sq = static_cast<Square>(s);
            if (board.pieceType(sq) == NONE_PIECE || board.pieceType(sq) == KING) continue;
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    if (board.kingTracker[k] != Square::A1)
                        acc.input[acc.get_board_feat(sq, board.pieceType(sq), board.pieceColor(sq), board.kingTracker[k], static_cast<PieceColor>(k))] = 1;
        }
    }

    void makeNullMove()
    {
        GameState state = GameState();
        state.castleRights = gameStates.back().castleRights;
        state.curTurn = gameStates.back().curTurn;
        state.curTurn++;

        state.zobristKey = gameStates.back().zobristKey;
        state.zobristKey ^= zobristTurn[state.curTurn];
        state.zobristKey ^= zobristTurn[gameStates.back().curTurn];


        // en passant
        std::memcpy(state.enpassants,
            gameStates.back().enpassants,
            sizeof(state.enpassants));
        state.enpassants[state.curTurn] = Square::A1;
        gameStates.push_back(state);
    }

    void undoNullMove()
    {
        gameStates.pop_back();
    }

    void move(const Move &move)
    {
        Square loc = static_cast<Square>(move.from());
        Square destination = static_cast<Square>(move.to());
        PieceType movingPiece = board.pieceType(loc);

        auto setFeat = [&](int sq, PieceType pie, PieceColor col) {
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    acc.set(acc.get_board_feat(sq, pie, col, board.kingTracker[k], static_cast<PieceColor>(k)));
        };

        // Create the new game state
        GameState oldState = gameStates.back();
        GameState state = GameState();
        state.castleRights = oldState.castleRights;
        state.curTurn = oldState.curTurn;
        state.curTurn++;
        state.lastCaptured = board.piece(destination);

        state.zobristKey = oldState.zobristKey;

        // turn
        state.zobristKey ^= zobristTurn[state.curTurn];
        state.zobristKey ^= zobristTurn[oldState.curTurn];

        auto lastType = getPieceType(state.lastCaptured);
        auto lastColor = getPieceColor(state.lastCaptured);

        if (useEval)
        {
            if(lastType != NONE_PIECE)
            {
                if(lastType != KING)
                {
                    for(auto &accumulator : nnue.accumulators)
                    {
                        for (int k = 0; k < 4; k++)
                        {
                            accumulator.set(accumulator.get_board_feat(destination, lastType, lastColor, board.kingTracker[k], static_cast<PieceColor>(k)));
                        }
                    }
                }

                // for null move pruning
                if(lastType != PAWN)
                {
                    board.nonPawnPieceCount[lastColor]--;
                }
            }
        }

        std::memcpy(state.enpassants,
            oldState.enpassants,
            sizeof(state.enpassants));
        state.enpassants[state.curTurn] = Square::A1;

        // Save moving color before board is updated
        PieceColor movingColor = board.pieceColor(loc);
        int movingColorIdx = movingColor;

        // Move the piece
        board.mailbox[destination] = board.mailbox[loc];
        board.mailbox[loc] = NONE_PIECE_COLOR;

        auto desPiece = board.pieceType(destination);
        auto desColor = board.pieceColor(destination);

        auto special = move.special_move();
        // Handle Promotion
        if(special == 1 || special == 3)
        {
            board.mailbox[destination] = makePiece(static_cast<PieceType>(move.promotion()), desColor);
        }
        // Check castling
        else if (special == 4)
        {
            switch (desColor)
            {
            case RED:
                if (destination == 218)
                {
                    state.zobristKey ^= zobristPieces[board_table[219]][ROOK][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[217]][ROOK][movingColorIdx];
                    board.mailbox[217] = makePiece(ROOK, RED);
                    board.mailbox[219] = NONE_PIECE_COLOR;
                }
                else if (destination == 214)
                {
                    state.zobristKey ^= zobristPieces[board_table[212]][ROOK][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[215]][ROOK][movingColorIdx];
                    board.mailbox[215] = makePiece(ROOK, RED);
                    board.mailbox[212] = NONE_PIECE_COLOR;
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[97]][ROOK][movingColorIdx];
                    board.mailbox[97] = makePiece(ROOK, BLUE);
                    board.mailbox[49] = NONE_PIECE_COLOR;
                }
                else if (destination == 145)
                {
                    state.zobristKey ^= zobristPieces[board_table[161]][ROOK][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[129]][ROOK][movingColorIdx];
                    board.mailbox[129] = makePiece(ROOK, BLUE);
                    board.mailbox[161] = NONE_PIECE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK-1][movingColorIdx];
                    board.mailbox[6] = makePiece(ROOK, YELLOW);
                    board.mailbox[4] = NONE_PIECE_COLOR;
                }
                else if (destination == 9)
                {
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK-1][movingColorIdx];
                    board.mailbox[8] = makePiece(ROOK, YELLOW);
                    board.mailbox[11] = NONE_PIECE_COLOR;
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    state.zobristKey ^= zobristPieces[board_table[43]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[94]][ROOK-1][movingColorIdx];
                    board.mailbox[94] = makePiece(ROOK, GREEN);
                    board.mailbox[62] = NONE_PIECE_COLOR;
                }
                else if (destination == 142)
                {
                    state.zobristKey ^= zobristPieces[board_table[174]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[126]][ROOK-1][movingColorIdx];
                    board.mailbox[126] = makePiece(ROOK, GREEN);
                    board.mailbox[174] = NONE_PIECE_COLOR;
                }
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Update castling rights when king is moved
        if (board.pieceType(destination) == KING)
        {
            switch (board.pieceColor(destination))
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
        else if (board.pieceType(destination) == ROOK || getPieceType(state.lastCaptured) == ROOK)
        {
            int rook = destination;
            if(board.pieceType(destination) == ROOK)
            {
                rook = loc;
            }
            switch (board.pieceColor(destination))
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
        else if(board.pieceType(destination) == PAWN)
        {
            if (special == 2 || special == 3)
            {
                switch(board.pieceColor(destination))
                {
                    case RED:
                        state.zobristKey ^= zobristPieces[board_table[loc + NORTH]][PAWN][board.pieceColor(loc + NORTH)];
                        if(useEval) setFeat(loc + NORTH, PAWN, board.pieceColor(loc + NORTH));
                        board.mailbox[loc + NORTH] = NONE_PIECE_COLOR;
                        break;
                    case BLUE:
                        state.zobristKey ^= zobristPieces[board_table[loc + EAST]][PAWN][board.pieceColor(loc + EAST)];
                        if(useEval) setFeat(loc + EAST, PAWN, board.pieceColor(loc + EAST));
                        board.mailbox[loc + EAST] = NONE_PIECE_COLOR;
                        break;
                    case YELLOW:
                        state.zobristKey ^= zobristPieces[board_table[loc + SOUTH]][PAWN][board.pieceColor(loc + SOUTH)];
                        if(useEval) setFeat(loc + SOUTH, PAWN, board.pieceColor(loc + SOUTH));
                        board.mailbox[loc + SOUTH] = NONE_PIECE_COLOR;
                        break;
                    case GREEN:
                        state.zobristKey ^= zobristPieces[board_table[loc + WEST]][PAWN][board.pieceColor(loc + WEST)];
                        if(useEval) setFeat(loc + WEST, PAWN, board.pieceColor(loc + WEST));
                        board.mailbox[loc + WEST] = NONE_PIECE_COLOR;
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
            if (board.pieceType(destination) == KING)
            {
                refreshNNUE();
                nnue.init_eval(state.curTurn);
            }
            else
            {
                setFeat(loc, movingPiece, board.pieceColor(destination));
                setFeat(destination, board.pieceType(destination), board.pieceColor(destination));
            }
        }

        // Zobrist: moving piece off origin, onto destination (promotion handled automatically)
        state.zobristKey ^= zobristPieces[board_table[loc]][movingPiece-1][movingColorIdx];
        state.zobristKey ^= zobristPieces[board_table[destination]][board.pieceType(destination)][movingColorIdx];

        // Zobrist: captured piece at destination
        if (lastType != NONE_PIECE)
        {
            int capIdx = lastColor;
            state.zobristKey ^= zobristPieces[board_table[destination]][lastType][capIdx];
        }

        // Zobrist: castling rights — XOR only bits that changed
        int changedRights = oldState.castleRights ^ state.castleRights;
        for (int i = 0; i < 8; i++)
            if (changedRights & (1 << i))
                state.zobristKey ^= zobristCastle[i];

        // Zobrist: en passant squares — XOR out old, XOR in new
        for (int player = 0; player < 4; player++)
        {
            if (oldState.enpassants[player] != Square::A1)
                state.zobristKey ^= zobristEnPassant[player][board_table[oldState.enpassants[player]]];
            if (state.enpassants[player] != Square::A1)
                state.zobristKey ^= zobristEnPassant[player][board_table[state.enpassants[player]]];
        }

        gameStates.push_back(state);
    }

    void undoMove(const Move &move)
    {
        Square loc = static_cast<Square>(move.from());
        Square destination = static_cast<Square>(move.to());
        PieceType pieceAtDest = board.pieceType(destination);
        PieceColor colorAtDest = board.pieceColor(destination);

        auto setFeat = [&](int sq, PieceType pie, PieceColor col) {
            for (auto &acc : nnue.accumulators)
                for (int k = 0; k < 4; k++)
                    acc.set(acc.get_board_feat(sq, pie, col, board.kingTracker[k], static_cast<PieceColor>(k)));
        };

        // Restore the old piece position
        GameState last = gameStates.back();
        gameStates.pop_back();
        board.mailbox[loc] = board.mailbox[destination];
        board.mailbox[destination] = last.lastCaptured;

        auto lastPieceType = getPieceType(last.lastCaptured);
        auto lastPieceColor = getPieceColor(last.lastCaptured);
        if(useEval)
        {
            if (lastPieceType != NONE_PIECE)
            {
                if(lastPieceType != KING)
                {
                    setFeat(destination, lastPieceType, lastPieceColor);
                }

                // for null move pruning
                if(lastPieceType != PAWN)
                {
                    board.nonPawnPieceCount[lastPieceColor]++;
                }
            }
        }
        // Restore promotion
        if (move.special_move() == 1 || move.special_move() == 3)
        {
            board.mailbox[loc] = makePiece(PAWN, colorAtDest);
        }

        // Restore rook when castling
        if (move.special_move() == 4)
        {
            switch (board.pieceColor(loc))
            {
            case RED:
                if (destination == 218)
                {
                    board.mailbox[219] = makePiece(ROOK, RED);
                    board.mailbox[217] = NONE_PIECE_COLOR;
                }
                else if (destination == 214)
                {
                    board.mailbox[212] = makePiece(ROOK, RED);
                    board.mailbox[215] = NONE_PIECE_COLOR;
                }
                break;
            case BLUE:
                if (destination == 81)
                {
                    board.mailbox[49] = makePiece(ROOK, BLUE);
                    board.mailbox[97] = NONE_PIECE_COLOR;
                }
                else if (destination == 145)
                {
                    board.mailbox[161] = makePiece(ROOK, BLUE);
                    board.mailbox[129] = NONE_PIECE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == 5)
                {
                    board.mailbox[4] = makePiece(ROOK, YELLOW);
                    board.mailbox[6] = NONE_PIECE_COLOR;
                }
                else if (destination == 9)
                {
                    board.mailbox[11] = makePiece(ROOK, YELLOW);
                    board.mailbox[8] = NONE_PIECE_COLOR;
                }
                break;
            case GREEN:
                if (destination == 78)
                {
                    board.mailbox[62] = makePiece(ROOK, GREEN);
                    board.mailbox[94] = NONE_PIECE_COLOR;
                }
                else if (destination == 142)
                {
                    board.mailbox[174] = makePiece(ROOK, GREEN);
                    board.mailbox[126] = NONE_PIECE_COLOR;
                }
                break;
            
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Restore en passant
        else if(move.special_move() == 2 || move.special_move() == 3)
        {
            switch(board.pieceColor(loc))
            {
                case RED:
                    if(destination == loc + NORTH + WEST)
                    {
                        board.mailbox[loc + NORTH] = makePiece(PAWN, BLUE);
                    }
                    else
                    {
                       board.mailbox[loc + NORTH] = makePiece(PAWN, GREEN);
                    }
                    if(useEval) setFeat(loc + NORTH, PAWN, board.pieceColor(loc + NORTH));
                    break;
                case BLUE:
                    if(destination == loc + EAST + NORTH)
                    {
                        board.mailbox[loc + EAST] = makePiece(PAWN, YELLOW);
                    }
                    else
                    {
                        board.mailbox[loc + EAST] = makePiece(PAWN, RED);
                    }
                    if(useEval) setFeat(loc + EAST, PAWN, board.pieceColor(loc + EAST));
                    break;
                case YELLOW:
                    if(destination == loc + SOUTH + WEST)
                    {
                        board.mailbox[loc + SOUTH] = makePiece(PAWN, BLUE);
                    }
                    else
                    {
                        board.mailbox[loc + SOUTH] = makePiece(PAWN, GREEN);
                    }
                    if(useEval) setFeat(loc + SOUTH, PAWN, board.pieceColor(loc + SOUTH));
                    break;
                case GREEN:
                    if(destination == loc + WEST + NORTH)
                    {
                        board.mailbox[loc + WEST] = makePiece(PAWN, YELLOW);
                    }
                    else
                    {
                        board.mailbox[loc + WEST] = makePiece(PAWN, RED);
                    }
                    if(useEval) setFeat(loc + WEST, PAWN, board.pieceColor(loc + WEST));
                    break;
                default:
                    throw std::runtime_error("Unknown color given");
            }
        }

        if (board.pieceType(loc) == KING)
            board.kingTracker[board.pieceColor(loc)] = loc;

        if(useEval) 
        {
            if (board.pieceType(loc) == KING)
            {
                refreshNNUE();
                nnue.init_eval(last.curTurn);
            }
            else
            {
                setFeat(loc, board.pieceType(loc), board.pieceColor(loc));
                setFeat(destination, pieceAtDest, colorAtDest);
            }
        }
    }
};
