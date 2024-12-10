#pragma once

enum PieceType
{
    NONE_PIECE = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6,
};

// enum both Colors
enum PieceColor
{
    NONE_COLOR,
    RED,
    BLUE = RED << 1,
    YELLOW = RED << 2,
    GREEN = RED << 3,

    TEAM_RY = RED | YELLOW,
    TEAM_BG = BLUE | GREEN
};

constexpr PieceColor operator++(PieceColor& color, int)
{
    PieceColor oldColor = color; // Store the current value
    switch(color)
    {
        case RED: color = BLUE; break;
        case BLUE: color = YELLOW; break;
        case YELLOW: color = GREEN; break;
        case GREEN: color = RED; break;
        default: break;
    }
    return oldColor; // Return the old value
}


enum CastlingRights
{
    NO_CASTLING,
    RED_OO,
    RED_OOO = RED_OO << 1,
    BLUE_OO = RED_OO << 2,
    BLUE_OOO = RED_OO << 3,
    YELLOW_OO = RED_OO << 4,
    YELLOW_OOO = RED_OO << 5,
    GREEN_OO = RED_OO << 6,
    GREEN_OOO = RED_OO << 7,

    RED_CASTLING = RED_OO | RED_OOO,
    BLUE_CASTLING = BLUE_OO | BLUE_OOO,
    YELLOW_CASTLING = YELLOW_OO | YELLOW_OOO,
    GREEN_CASTLING = GREEN_OO | GREEN_OOO,

    ALL_CASTLING = RED_CASTLING | BLUE_CASTLING | YELLOW_CASTLING | GREEN_CASTLING
};

enum Directions
{
    NORTH = -16,
    EAST = 1,
    SOUTH = 16,
    WEST = -1,
};

enum MoveType
{
    CAPTURES,         // All captures + queen promotions
    QUIETS,           // All non-captures + under promotions
};