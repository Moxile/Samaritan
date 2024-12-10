/*
To add:
- Castling
- En passant
*/

#include "movegen.h"

constexpr ExtMove *getPawnMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    int offset = 0;
    bool isOnStartRank = false;
    bool nextPromotion = false;
    switch (pos.colorMailbox[loc])
    {
    case RED:
        offset = 1;
        if (loc / 16 == 12)
            isOnStartRank = true;
        if (loc / 16 == 4)
            nextPromotion = true;
        break;
    case BLUE:
        offset = 2;
        if (loc % 16 == 2)
            isOnStartRank = true;
        if (loc % 16 == 10)
            nextPromotion = true;
        break;
    case YELLOW:
        offset = 3;
        if (loc / 16 == 1)
            isOnStartRank = true;
        if (loc / 16 == 9)
            nextPromotion = true;
        break;
    case GREEN:
        offset = 4;
        if (loc % 16 == 13)
            isOnStartRank = true;
        if (loc % 16 == 5)
            nextPromotion = true;
        break;
    }
    if (!isInvalidLocation(loc + offsets[offset][0]) && pos.isEmpty(loc + offsets[offset][0]))
    {
        if (nextPromotion)
        {
            *moveList++ = Move(loc + offsets[offset][0], loc, QUEEN, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][0], loc, ROOK, 0);
            moveList->gen_type = QUIETS;
            *moveList++ = Move(loc + offsets[offset][0], loc, BISHOP, 0);
            moveList->gen_type = QUIETS;
            *moveList++ = Move(loc + offsets[offset][0], loc, KNIGHT, 0);
            moveList->gen_type = QUIETS;
        }
        else
        {
            *moveList++ = Move(loc + offsets[offset][0], loc, 0, 0);
            moveList->gen_type = QUIETS;

            if (isOnStartRank && pos.isEmpty(loc + 2 * offsets[offset][0]))
            {
                *moveList++ = Move(loc + 2 * offsets[offset][0], loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
        }
    }
    if (!isInvalidLocation(loc + offsets[offset][1]) && !pos.isEmpty(loc + offsets[offset][1]) && getTeam(pos.colorMailbox[loc + offsets[offset][1]]) != getTeam(pos.colorMailbox[loc]))
    {
        if (nextPromotion)
        {
            *moveList++ = Move(loc + offsets[offset][1], loc, QUEEN, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][1], loc, ROOK, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][1], loc, BISHOP, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][1], loc, KNIGHT, 0);
            moveList->gen_type = CAPTURES;
        }
        else
        {
            *moveList++ = Move(loc + offsets[offset][1], loc, 0, 0);
            moveList->gen_type = CAPTURES;
        }
    }
    if (!isInvalidLocation(loc + offsets[offset][2]) && !pos.isEmpty(loc + offsets[offset][2]) && getTeam(pos.colorMailbox[loc + offsets[offset][2]]) != getTeam(pos.colorMailbox[loc]))
    {
        if (nextPromotion)
        {
            *moveList++ = Move(loc + offsets[offset][2], loc, QUEEN, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][2], loc, ROOK, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][2], loc, BISHOP, 0);
            moveList->gen_type = CAPTURES;
            *moveList++ = Move(loc + offsets[offset][2], loc, KNIGHT, 0);
            moveList->gen_type = CAPTURES;
        }
        else
        {
            *moveList++ = Move(loc + offsets[offset][2], loc, 0, 0);
            moveList->gen_type = CAPTURES;
        }
    }

    return moveList;
}

constexpr ExtMove *getKnightMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const int moves[] = {loc + NORTH + NORTH + EAST, loc + NORTH + NORTH + WEST, loc + WEST + WEST + NORTH,
                         loc + WEST + WEST + SOUTH, loc + SOUTH + SOUTH + WEST, loc + SOUTH + SOUTH + EAST,
                         loc + EAST + EAST + SOUTH, loc + EAST + EAST + NORTH};

    for (int move : moves)
    {
        if (!isInvalidLocation(move))
        {
            if (pos.isEmpty(move))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
            else if (getTeam(pos.colorMailbox[move]) != getTeam(pos.colorMailbox[loc]))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getStraightLineMoves(const Position &pos, const int loc, ExtMove *moveList, const int increment)
{
    for (int destination = loc + increment; !isInvalidLocation(destination); destination += increment)
    {
        if (!pos.isEmpty(destination) && (getTeam(pos.colorMailbox[destination]) == getTeam(pos.colorMailbox[loc])))
        {
            break;
        }
        if (pos.pieceMailbox[destination] != NONE_PIECE)
        {
            *moveList++ = Move(destination, loc, 0, 0);
            moveList->gen_type = CAPTURES;
            break;
        }
        else
        {
            *moveList++ = Move(destination, loc, 0, 0);
            moveList->gen_type = QUIETS;
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

constexpr ExtMove *getKingMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[9]; offset++)
    {
        int move = offsets[9][offset] + loc;
        if (!isInvalidLocation(move))
        {
            if (pos.isEmpty(move))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
            else if (getTeam(pos.colorMailbox[move]) != getTeam(pos.colorMailbox[loc]))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
            }
        }
    }
    return moveList;
}

bool inCheck(const Position &pos, PieceColor color)
{
    // Return true if the king is in check.
    return pos.isSquareAttacked(pos.kingTracker[__builtin_ctz(color)], color, getTeam(color));
}

ExtMove *generate(const Position &pos, ExtMove *moveList)
{
    ExtMove pseudorandoms[MAX_MOVES];
    ExtMove *move_ptr = pseudorandoms;
    for (int loc = 0; loc < 224; loc++)
    {
        if (pos.colorMailbox[loc] == pos.curTurn)
        {
            switch (pos.pieceMailbox[loc])
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
        Position temp = pos;
        temp.move(*move);
        if (!inCheck(temp, pos.curTurn))
        {

            if (move->gen_type == CAPTURES)
            {
                move->value += pos.piece_weights[pos.pieceMailbox[move->to()] - 1] - (pos.piece_weights[pos.pieceMailbox[move->from()] - 1])/10;
            }
            if(*move == pos.bestMove)
            {
                move->value += 10000;
            }
            *moveList++ = *move;
        }
    }
    return moveList;
}
MoveList::MoveList(Position pos) : last(generate(pos, moveList)) {}

void MoveList::print() const
{
    for (auto move : moveList)
    {
    }
}