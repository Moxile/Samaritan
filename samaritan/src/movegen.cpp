/*
To add:
- Castling
- En passant
*/

#include "movegen.h"
#include <stdexcept>

constexpr ExtMove *getPawnMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    int offset = -1;
    bool isOnStartRank = false;
    bool nextPromotion = false;
    bool rightEnPassant = false;
    bool leftEnPassant = false;
    const auto enpassants = pos.gameStates.back().enpassants;
    const auto myTeam = getTeam(pos.board.colorMailbox[loc]);
    // checking different scenarios for pawn position
    switch (pos.board.colorMailbox[loc])
    {
    case RED:
        offset = 1;
        if (loc / 16 == 12)
            isOnStartRank = true;
        if (loc / 16 == 4)
            nextPromotion = true;
        if (enpassants[1] == loc + NORTH + WEST)
            leftEnPassant = true;
        if (enpassants[3] == loc + NORTH + EAST)
            rightEnPassant = true;
        break;
    case BLUE:
        offset = 2;
        if (loc % 16 == 2)
            isOnStartRank = true;
        if (loc % 16 == 10)
            nextPromotion = true;
        if (enpassants[2] == loc + EAST + NORTH)
            leftEnPassant = true;
        if (enpassants[0] == loc + EAST + SOUTH)
            rightEnPassant = true;
        break;
    case YELLOW:
        offset = 3;
        if (loc / 16 == 1)
            isOnStartRank = true;
        if (loc / 16 == 9)
            nextPromotion = true;
        if (enpassants[3] == loc + SOUTH + EAST)
            leftEnPassant = true;
        if (enpassants[1] == loc + SOUTH + WEST)
            rightEnPassant = true;
        break;
    case GREEN:
        offset = 4;
        if (loc % 16 == 13)
            isOnStartRank = true;
        if (loc % 16 == 5)
            nextPromotion = true;
        if (enpassants[0] == loc + WEST + SOUTH)
            leftEnPassant = true;
        if (enpassants[2] == loc + WEST + NORTH)
            rightEnPassant = true;
        break;
    default:
        throw std::runtime_error("Unknown color given");
    }

    // Create move for forward pawn move
    if (!isInvalidLocation(loc + offsets[offset][0]) && pos.board.isEmpty(loc + offsets[offset][0]))
    {
        if (nextPromotion)
        {
            *moveList = Move(loc + offsets[offset][0], loc, QUEEN, 1);
            moveList->gen_type = QUEEN_PROMOTION;
            moveList->value = piece_values[QUEEN] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, ROOK, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[ROOK] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, BISHOP, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[BISHOP] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, KNIGHT, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[KNIGHT] - 1;
            moveList++;
        }
        else
        {
            *moveList = Move(loc + offsets[offset][0], loc, 0, 0);
            moveList->gen_type = QUIETS;
            moveList++;

            if (isOnStartRank && pos.board.isEmpty(loc + 2 * offsets[offset][0]))
            {
                *moveList = Move(loc + 2 * offsets[offset][0], loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList++;
            }
        }
    }

    // Create move for right diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][1]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][1]) && getTeam(pos.board.colorMailbox[loc + offsets[offset][1]]) != myTeam)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][1], loc, QUEEN, 1);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, ROOK, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, BISHOP, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, KNIGHT, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][1], loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - piece_values[PAWN];
                moveList++;
            }
        }
        else if (rightEnPassant)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][1], loc, QUEEN, 3);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, ROOK, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, BISHOP, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, KNIGHT, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][1], loc, 0, 2);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[PAWN] - piece_values[PAWN];
                moveList++;
            }
        }
    }

    // Create move for left diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][2]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][2]) && getTeam(pos.board.colorMailbox[loc + offsets[offset][2]]) != myTeam)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][2], loc, QUEEN, 1);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, ROOK, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, BISHOP, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, KNIGHT, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][2], loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - piece_values[PAWN];
                moveList++;
            }
        }
        else if (leftEnPassant)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][2], loc, QUEEN, 3);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, ROOK, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, BISHOP, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, KNIGHT, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][2], loc, 0, 2);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[PAWN] - piece_values[PAWN];
                moveList++;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getKnightMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const auto myTeam = getTeam(pos.board.colorMailbox[loc]);
    // Check all possible knight moves using precomputed offsets
    const int moves[] = {loc + NORTH + NORTH + EAST, loc + NORTH + NORTH + WEST, loc + WEST + WEST + NORTH,
                         loc + WEST + WEST + SOUTH, loc + SOUTH + SOUTH + WEST, loc + SOUTH + SOUTH + EAST,
                         loc + EAST + EAST + SOUTH, loc + EAST + EAST + NORTH};

    for (int move : moves)
    {
        if (!isInvalidLocation(move))
        {
            if (pos.board.isEmpty(move))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList++;
            }
            else if (getTeam(pos.board.colorMailbox[move]) != myTeam)
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[move]] - piece_values[KNIGHT];
                moveList++;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getStraightLineMoves(const Position &pos, const int loc, ExtMove *moveList, const int increment)
{
    const auto myTeam = getTeam(pos.board.colorMailbox[loc]);
    const auto myPiece = pos.board.pieceMailbox[loc];
    // Generate moves using a ray-casting approach
    for (int destination = loc + increment; !isInvalidLocation(destination); destination += increment)
    {
        if (!pos.board.isEmpty(destination) && (getTeam(pos.board.colorMailbox[destination]) == myTeam))
        {
            break;
        }
        if (pos.board.pieceMailbox[destination] != NONE_PIECE)
        {
            *moveList = Move(destination, loc, 0, 0);
            moveList->gen_type = CAPTURES;
            moveList->value = 10*piece_values[pos.board.pieceMailbox[destination]] - piece_values[myPiece];
            moveList++;
            break;
        }
        else
        {
            *moveList = Move(destination, loc, 0, 0);
            moveList->gen_type = QUIETS;
            moveList++;
        }
    }

    return moveList;
}

constexpr ExtMove *getBishopMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[6]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[6][offset]);
    }
    return moveList;
}

constexpr ExtMove *getRookMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[7]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[7][offset]);
    }
    return moveList;
}

constexpr ExtMove *getQueenMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[8]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[8][offset]);
    }
    return moveList;
}

ExtMove *getKingMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const auto &state = pos.gameStates.back();
    const auto turn = state.curTurn;
    const auto myTeam = getTeam(turn);

    for (int offset = 0; offset < offsetsNum[9]; offset++)
    {
        int move = offsets[9][offset] + loc;
        if (!isInvalidLocation(move))
        {
            if (pos.board.isEmpty(move))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList++;
            }
            else if (getTeam(pos.board.colorMailbox[move]) != myTeam)
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[move]] - piece_values[KING];
                moveList++;
            }
        }
    }

    // Castling
    if (pos.board.isSquareAttacked(loc, turn, myTeam))
    {
        return moveList; // Cannot castle if the king is in check
    }
    switch (turn)
    {
    case RED:
        if (state.castleRights & RED_OO)
        {
            if (pos.board.isEmpty(J14) && pos.board.isEmpty(K14))
            {
                if (!pos.board.isSquareAttacked(J14, turn, myTeam) && !pos.board.isSquareAttacked(K14, turn, myTeam))
                {
                    *moveList = Move(K14, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        if (state.castleRights & RED_OOO)
        {
            if (pos.board.isEmpty(H14) && pos.board.isEmpty(G14) && pos.board.isEmpty(F14))
            {
                if (!pos.board.isSquareAttacked(H14, turn, myTeam) && !pos.board.isSquareAttacked(G14, turn, myTeam))
                {
                    *moveList = Move(G14, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        break;
    case BLUE:
        if (state.castleRights & BLUE_OO)
        {
            if (pos.board.isEmpty(B7) && pos.board.isEmpty(B6) && pos.board.isEmpty(B5))
            {
                if (!pos.board.isSquareAttacked(B7, turn, myTeam) && !pos.board.isSquareAttacked(B6, turn, myTeam))
                {
                    *moveList = Move(B6, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        if (state.castleRights & BLUE_OOO)
        {
            if (pos.board.isEmpty(B9) && pos.board.isEmpty(B10))
            {
                if (!pos.board.isSquareAttacked(B9, turn, myTeam) && !pos.board.isSquareAttacked(B10, turn, myTeam))
                {
                    *moveList = Move(B10, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        break;
    case YELLOW:
        if (state.castleRights & YELLOW_OO)
        {
            if (pos.board.isEmpty(F1) && pos.board.isEmpty(G1))
            {
                if (!pos.board.isSquareAttacked(F1, turn, myTeam) && !pos.board.isSquareAttacked(G1, turn, myTeam))
                {
                    *moveList = Move(F1, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        if (state.castleRights & YELLOW_OOO)
        {
            if (pos.board.isEmpty(I1) && pos.board.isEmpty(J1) && pos.board.isEmpty(K1))
            {
                if (!pos.board.isSquareAttacked(I1, turn, myTeam) && !pos.board.isSquareAttacked(J1, turn, myTeam))
                {
                    *moveList = Move(J1, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        break;
    case GREEN:
        if (state.castleRights & GREEN_OO)
        {
            if (pos.board.isEmpty(O5) && pos.board.isEmpty(O6))
            {
                if (!pos.board.isSquareAttacked(O5, turn, myTeam) && !pos.board.isSquareAttacked(O6, turn, myTeam))
                {
                    *moveList = Move(O5, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        if (state.castleRights & GREEN_OOO)
        {
            if (pos.board.isEmpty(O8) && pos.board.isEmpty(O9) && pos.board.isEmpty(O10))
            {
                if (!pos.board.isSquareAttacked(O8, turn, myTeam) && !pos.board.isSquareAttacked(O9, turn, myTeam))
                {
                    *moveList = Move(O9, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList++;
                }
            }
        }
        break;
    default:
        throw std::runtime_error("Unknown color given");
    }

    return moveList;
}

bool inCheck(const Position &pos, PieceColor color)
{
    // Return true if the king is in check.
    return pos.board.isSquareAttacked(pos.board.kingTracker[__builtin_ctz(color)], color, getTeam(color));
}

ExtMove *generate(Position &pos, ExtMove *moveList)
{
    const auto curTurn = pos.gameStates.back().curTurn;

    ExtMove pseudorandoms[MAX_MOVES];
    ExtMove *move_ptr = pseudorandoms;
    for (int loc = 0; loc < 224; loc++)
    {
        if (pos.board.colorMailbox[loc] == curTurn)
        {
            switch (pos.board.pieceMailbox[loc])
            {
            case PAWN:
                move_ptr = getPawnMoves(pos, loc, move_ptr);
                break;
            case KNIGHT:
                move_ptr = getKnightMoves(pos, loc, move_ptr);
                break;
            case BISHOP:
                move_ptr = getBishopMoves(pos, loc, move_ptr);
                break;
            case ROOK:
                move_ptr = getRookMoves(pos, loc, move_ptr);
                break;
            case QUEEN:
                move_ptr = getQueenMoves(pos, loc, move_ptr);
                break;
            case KING:
                move_ptr = getKingMoves(pos, loc, move_ptr);
                break;
            default:
                break;
            }
        }
    }
    for (ExtMove *move = pseudorandoms; move != move_ptr; move++)
    {
        pos.move(*move);
        if (!inCheck(pos, curTurn))
        {
            *moveList++ = *move;
        }
        pos.undoMove(*move);
    }
    return moveList;
}
MoveList::MoveList(Position& pos) : last(generate(pos, moveList)) {}

void MoveList::print() const
{
    for (auto move : moveList)
    {
    }
}
