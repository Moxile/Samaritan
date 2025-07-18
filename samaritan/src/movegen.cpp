/*
To add:
- Castling
- En passant
*/

#include "movegen.h"

constexpr ExtMove *getPawnMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    int offset = -1;
    bool isOnStartRank = false;
    bool nextPromotion = false;
    bool rightEnPassant = false;
    bool leftEnPassant = false;
    // checking different scenarios for pawn position
    switch (pos.board.colorMailbox[loc])
    {
    case RED:
        offset = 1;
        if (loc / 16 == 12)
            isOnStartRank = true;
        if (loc / 16 == 4)
            nextPromotion = true;
        if (pos.gameStates.back().enpassants[1] == loc + NORTH + WEST)
            leftEnPassant = true;
        if (pos.gameStates.back().enpassants[3] == loc + NORTH + EAST)
            rightEnPassant = true;
        break;
    case BLUE:
        offset = 2;
        if (loc % 16 == 2)
            isOnStartRank = true;
        if (loc % 16 == 10)
            nextPromotion = true;
        if (pos.gameStates.back().enpassants[2] == loc + EAST + NORTH)
            leftEnPassant = true;
        if (pos.gameStates.back().enpassants[0] == loc + EAST + SOUTH)
            rightEnPassant = true;
        break;
    case YELLOW:
        offset = 3;
        if (loc / 16 == 1)
            isOnStartRank = true;
        if (loc / 16 == 9)
            nextPromotion = true;
        if (pos.gameStates.back().enpassants[3] == loc + SOUTH + EAST)
            leftEnPassant = true;
        if (pos.gameStates.back().enpassants[1] == loc + SOUTH + WEST)
            rightEnPassant = true;
        break;
    case GREEN:
        offset = 4;
        if (loc % 16 == 13)
            isOnStartRank = true;
        if (loc % 16 == 5)
            nextPromotion = true;
        if (pos.gameStates.back().enpassants[0] == loc + WEST + SOUTH)
            leftEnPassant = true;
        if (pos.gameStates.back().enpassants[2] == loc + WEST + NORTH)
            rightEnPassant = true;
        break;
    }

    // Create move for forward pawn move
    if (!isInvalidLocation(loc + offsets[offset][0]) && pos.board.isEmpty(loc + offsets[offset][0]))
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

            if (isOnStartRank && pos.board.isEmpty(loc + 2 * offsets[offset][0]))
            {
                *moveList++ = Move(loc + 2 * offsets[offset][0], loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
        }
    }

    // Create move for right diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][1]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][1]) && getTeam(pos.board.colorMailbox[loc + offsets[offset][1]]) != getTeam(pos.board.colorMailbox[loc]))
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
        else if (rightEnPassant)
        {
            if (nextPromotion)
            {
                *moveList++ = Move(loc + offsets[offset][1], loc, QUEEN, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][1], loc, ROOK, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][1], loc, BISHOP, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][1], loc, KNIGHT, 3);
                moveList->gen_type = CAPTURES;
            }
            else
            {
                *moveList++ = Move(loc + offsets[offset][1], loc, 0, 2);
                moveList->gen_type = CAPTURES;
            }
        }
    }

    // Create move for left diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][2]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][2]) && getTeam(pos.board.colorMailbox[loc + offsets[offset][2]]) != getTeam(pos.board.colorMailbox[loc]))
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
        else if (leftEnPassant)
        {
            if (nextPromotion)
            {
                *moveList++ = Move(loc + offsets[offset][2], loc, QUEEN, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][2], loc, ROOK, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][2], loc, BISHOP, 3);
                moveList->gen_type = CAPTURES;
                *moveList++ = Move(loc + offsets[offset][2], loc, KNIGHT, 3);
                moveList->gen_type = CAPTURES;
            }
            else
            {
                *moveList++ = Move(loc + offsets[offset][2], loc, 0, 2);
                moveList->gen_type = CAPTURES;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getKnightMoves(const Position &pos, const int loc, ExtMove *moveList)
{
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
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
            else if (getTeam(pos.board.colorMailbox[move]) != getTeam(pos.board.colorMailbox[loc]))
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
    // Generate moves using a ray-casting approach
    for (int destination = loc + increment; !isInvalidLocation(destination); destination += increment)
    {
        if (!pos.board.isEmpty(destination) && (getTeam(pos.board.colorMailbox[destination]) == getTeam(pos.board.colorMailbox[loc])))
        {
            break;
        }
        if (pos.board.pieceMailbox[destination] != NONE_PIECE)
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

ExtMove *getKingMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[9]; offset++)
    {
        int move = offsets[9][offset] + loc;
        if (!isInvalidLocation(move))
        {
            if (pos.board.isEmpty(move))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
            }
            else if (getTeam(pos.board.colorMailbox[move]) != getTeam(pos.board.colorMailbox[loc]))
            {
                *moveList++ = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
            }
        }
    }

    // Castling
    if (pos.board.isSquareAttacked(loc, pos.gameStates.back().curTurn, getTeam(pos.gameStates.back().curTurn)))
    {
        return moveList; // Cannot castle if the king is in check
    }
    switch (pos.gameStates.back().curTurn)
    {
    case RED:
        if (pos.gameStates.back().castleRights & RED_OO)
        {
            if (pos.board.isEmpty(217) && pos.board.isEmpty(218))
            {
                if (!pos.board.isSquareAttacked(217, RED, TEAM_RY) && !pos.board.isSquareAttacked(218, RED, TEAM_RY))
                {
                    *moveList++ = Move(218, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        if (pos.gameStates.back().castleRights & RED_OOO)
        {
            if (pos.board.isEmpty(215) && pos.board.isEmpty(214))
            {
                if (!pos.board.isSquareAttacked(215, RED, TEAM_RY) && !pos.board.isSquareAttacked(214, RED, TEAM_RY))
                {
                    *moveList++ = Move(214, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        break;
    case BLUE:
        if (pos.gameStates.back().castleRights & BLUE_OO)
        {
            if (pos.board.isEmpty(81) && pos.board.isEmpty(65))
            {
                if (!pos.board.isSquareAttacked(81, BLUE, TEAM_BG) && !pos.board.isSquareAttacked(65, BLUE, TEAM_BG))
                {
                    *moveList++ = Move(65, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        if (pos.gameStates.back().castleRights & BLUE_OOO)
        {
            if (pos.board.isEmpty(113) && pos.board.isEmpty(129))
            {
                if (!pos.board.isSquareAttacked(113, BLUE, TEAM_BG) && !pos.board.isSquareAttacked(129, BLUE, TEAM_BG))
                {
                    *moveList++ = Move(129, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        break;
    case YELLOW:
        if (pos.gameStates.back().castleRights & YELLOW_OO)
        {
            if (pos.board.isEmpty(5) && pos.board.isEmpty(6))
            {
                if (!pos.board.isSquareAttacked(5, YELLOW, TEAM_RY) && !pos.board.isSquareAttacked(6, YELLOW, TEAM_RY))
                {
                    *moveList++ = Move(5, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        if (pos.gameStates.back().castleRights & YELLOW_OOO)
        {
            if (pos.board.isEmpty(8) && pos.board.isEmpty(9))
            {
                if (!pos.board.isSquareAttacked(8, YELLOW, TEAM_RY) && !pos.board.isSquareAttacked(9, YELLOW, TEAM_RY))
                {
                    *moveList++ = Move(9, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        break;
    case GREEN:
        if (pos.gameStates.back().castleRights & GREEN_OO)
        {
            if (pos.board.isEmpty(78) && pos.board.isEmpty(94))
            {
                if (!pos.board.isSquareAttacked(78, GREEN, TEAM_BG) && !pos.board.isSquareAttacked(94, GREEN, TEAM_BG))
                {
                    *moveList++ = Move(94, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        if (pos.gameStates.back().castleRights & GREEN_OOO)
        {
            if (pos.board.isEmpty(126) && pos.board.isEmpty(142))
            {
                if (!pos.board.isSquareAttacked(126, GREEN, TEAM_BG) && !pos.board.isSquareAttacked(142, GREEN, TEAM_BG))
                {
                    *moveList++ = Move(142, loc, 0, 4);
                    moveList->gen_type = QUIETS;
                }
            }
        }
        break;
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
    ExtMove pseudorandoms[MAX_MOVES];
    ExtMove *move_ptr = pseudorandoms;
    for (int loc = 0; loc < 224; loc++)
    {
        if (pos.board.colorMailbox[loc] == pos.gameStates.back().curTurn)
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
    PieceColor curTurn = pos.gameStates.back().curTurn;
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
MoveList::MoveList(Position pos) : last(generate(pos, moveList)) {}

void MoveList::print() const
{
    for (auto move : moveList)
    {
    }
}