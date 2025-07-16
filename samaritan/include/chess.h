#pragma once

#include <string>
#include <sstream>
#include <cstdint>

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

// square availability
constexpr int baseMailbox[224] = {
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
    -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1,
};

// offsets for all pieces
constexpr int offsetsNum[11] = { 0, 3, 3, 3, 3, 8, 4, 4, 8, 8, 16, };
constexpr int offsets[11][16] = {{},
    { -16, -15, -17,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Red Pawn
    {  1,   17, -15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Blue Pawn
    {  16,  15,  17,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Yellow Pawn
    { -1,  -17,  15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Green Pawn
    { -18, -33, -31, -14, 18, 33, 31,  14, 0,   0,   0,   0,  0,  0,  0,  0, }, // Knight
    { -17, -15,  17,  15, 0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Bishop
    { -16,  1,   16, -1,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Rook
    { -17, -16, -15,  1,  17, 16, 15, -1,  0,   0,   0,   0,  0,  0,  0,  0, }, // Queen
    { -17, -16, -15,  1,  17, 16, 15, -1,  0,   0,   0,   0,  0,  0,  0,  0, }, // King
    { -17, -16, -15,  1,  17, 16, 15, -1, -18, -33, -31, -14, 18, 33, 31, 14 }, // All
};

constexpr inline bool isInvalidLocation(int location) {
    return location < 1 || baseMailbox[location] == -1 || location > 220;
}

constexpr inline PieceColor getTeam(PieceColor color)
{
    switch (color)
    {
        case RED: return TEAM_RY;
        case BLUE: return TEAM_BG;
        case YELLOW: return TEAM_RY;
        case GREEN: return TEAM_BG;
        default: return NONE_COLOR;
    }
}


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

constexpr int ranks[14] = {14, 13, 12, 11, 10, 9, 8 ,7, 6, 5, 4, 3, 2, 1};
constexpr char files[14] = {'a',  'b',  'c',  'd',  'e',  'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'};

class Move
{
    // A move needs 32 bits to be stored
    //
    // bit  0- 8: destination square (from 0 to 63)
    // bit  8-16: origin square (from 0 to 63)
    // bit 17-24: promotion piece type
    // bit 25-32: special move flag: promotion (1), en passant (2), en passant and promotion(3),  castling(4)   0 -> no special move
    // NOTE: every information has 8 bits reserved as a layout

public:
    enum Field
    {
        DESTINATION = 0,
        ORIGIN = 8,
        PROMOTION = 16,
        SPECIAL = 24
    };

    explicit Move() = default;

    Move(int destination, int origin, int promotion_type, int special_move)
    {
        data = destination;
        data |= origin << 8;
        data |= promotion_type << 16;
        data |= special_move << 24;
    }

    const std::string toUCI() const
    {
        std::stringstream uci;
        uci << files[(from() % 16)-1] << ranks[from() / 16];
        uci << files[(to() % 16)-1] << ranks[to() / 16];
        return uci.str();
    }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr int from() const { return (data >> 8) & 0xFF; }

    constexpr int to() const { return data & 0xFF; }

    constexpr int promotion() const { return (data >> 16) & 0xFF; }

    constexpr int special_move() const { return (data >> 24) & 0xFFl; }

    constexpr uint32_t getRawData() const { return data; }

    template <Field F>
    constexpr int get() { return (data >> F) & 0xFF; }

protected:
    uint32_t data;
};

struct ExtMove : public Move
{
    int gen_type; // MoveType: CAPTURES or QUIETS
    int value;

    void operator=(Move m) { data = m.getRawData(); }

    // Inhibit unwanted implicit conversions to Move
    // with an ambiguity that yields to a compile error.
    operator float() const = delete;
};

inline bool operator<(const ExtMove &f, const ExtMove &s) { return f.value < s.value; }
inline bool operator>(const ExtMove &f, const ExtMove &s) { return f.value > s.value; }





