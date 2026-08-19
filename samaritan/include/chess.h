#pragma once

#include <string>
#include <sstream>
#include <cstdint>
#include <array>

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

enum Square : uint8_t 
{
    A1 , B1 , C1 , D1 , E1 , F1 , G1 , H1 , I1 , J1 , K1 , L1 , M1 , N1 , O1 , P1 ,
    A2 , B2 , C2 , D2 , E2 , F2 , G2 , H2 , I2 , J2 , K2 , L2 , M2 , N2 , O2 , P2 ,
    A3 , B3 , C3 , D3 , E3 , F3 , G3 , H3 , I3 , J3 , K3 , L3 , M3 , N3 , O3 , P3 ,
    A4 , B4 , C4 , D4 , E4 , F4 , G4 , H4 , I4 , J4 , K4 , L4 , M4 , N4 , O4 , P4 ,
    A5 , B5 , C5 , D5 , E5 , F5 , G5 , H5 , I5 , J5 , K5 , L5 , M5 , N5 , O5 , P5 ,
    A6 , B6 , C6 , D6 , E6 , F6 , G6 , H6 , I6 , J6 , K6 , L6 , M6 , N6 , O6 , P6 ,
    A7 , B7 , C7 , D7 , E7 , F7 , G7 , H7 , I7 , J7 , K7 , L7 , M7 , N7 , O7 , P7 ,
    A8 , B8 , C8 , D8 , E8 , F8 , G8 , H8 , I8 , J8 , K8 , L8 , M8 , N8 , O8 , P8 ,
    A9 , B9 , C9 , D9 , E9 , F9 , G9 , H9 , I9 , J9 , K9 , L9 , M9 , N9 , O9 , P9 ,
    A10, B10, C10, D10, E10, F10, G10, H10, I10, J10, K10, L10, M10, N10, O10, P10,
    A11, B11, C11, D11, E11, F11, G11, H11, I11, J11, K11, L11, M11, N11, O11, P11,
    A12, B12, C12, D12, E12, F12, G12, H12, I12, J12, K12, L12, M12, N12, O12, P12,
    A13, B13, C13, D13, E13, F13, G13, H13, I13, J13, K13, L13, M13, N13, O13, P13,
    A14, B14, C14, D14, E14, F14, G14, H14, I14, J14, K14, L14, M14, N14, O14, P14,
    A15, B15, C15, D15, E15, F15, G15, H15, I15, J15, K15, L15, M15, N15, O15, P15,
    A16, B16, C16, D16, E16, F16, G16, H16, I16, J16, K16, L16, M16, N16, O16, P16,
};

// Ray walking and knight probes step off the board before they are rejected, so
// the validity table is padded to cover every index those offsets can reach:
// from 1-33 on the low side up to 219+33 on the high side. That turns the
// bounds test into a single load, which matters because this is the hottest
// predicate in the engine.
inline constexpr int SQ_GUARD = 33;
inline constexpr int SQ_TABLE_SIZE = 224 + 2 * SQ_GUARD;   // 290

inline constexpr auto squareValidTable = [] {
    std::array<bool, SQ_TABLE_SIZE> t{};
    for (int i = 0; i < SQ_TABLE_SIZE; ++i)
    {
        const int loc = i - SQ_GUARD;
        t[i] = (loc >= 1 && loc <= 220 && baseMailbox[loc] != -1);
    }
    return t;
}();

constexpr inline bool isInvalidLocation(int location) {
    return !squareValidTable[location + SQ_GUARD];
}

// PieceColor is a 4-bit mask (RED=1, BLUE=2, YELLOW=4, GREEN=8), so the colour
// operations below are pure bit twiddling: teammates sit 2 bits apart and the
// turn order is a rotate-left within those 4 bits. Branchless by construction.
constexpr int COLOR_MASK = RED | BLUE | YELLOW | GREEN;

// Smear each colour onto its partner: RED<->YELLOW, BLUE<->GREEN.
constexpr inline PieceColor getTeam(PieceColor color)
{
    const int c = static_cast<int>(color);
    return static_cast<PieceColor>((c | (c << 2) | (c >> 2)) & COLOR_MASK);
}

// Rotate left by n within the 4 colour bits.
constexpr PieceColor operator+(PieceColor color, int n)
{
    const int c = static_cast<int>(color);
    const int r = n & 3;
    return static_cast<PieceColor>(((c << r) | (c >> (4 - r))) & COLOR_MASK);
}

constexpr PieceColor operator++(PieceColor& color, int)
{
    const PieceColor oldColor = color;
    color = oldColor + 1;
    return oldColor;
}

// Team membership without going through getTeam(): a colour bit ANDed with a
// team mask is non-zero exactly when that colour is on that team. Callers must
// already know the square is occupied, since NONE_COLOR reads as "not mine".
constexpr inline bool isOnTeam(PieceColor col, PieceColor team)
{
    return (static_cast<int>(col) & static_cast<int>(team)) != 0;
}

// Contiguous copies of the two offset sets the attack scan walks, so the hot
// loops index a small dedicated array rather than a row of offsets[11][16].
// Last on-board square along each of the eight king-directions from a square,
// or the square itself when the ray is empty. Ray walks use this as the loop
// bound so they no longer pay a validity lookup per step. 3.6 KB, L1-resident --
// deliberately far smaller than a per-square-pair bitboard table.
inline constexpr auto rayEndTable = [] {
    std::array<std::array<int16_t, 8>, 224> t{};
    constexpr int dirs[8] = { -17, -16, -15, 1, 17, 16, 15, -1 };
    for (int sq = 0; sq < 224; ++sq)
        for (int d = 0; d < 8; ++d)
        {
            int last = sq;
            for (int n = sq + dirs[d]; !isInvalidLocation(n); n += dirs[d]) last = n;
            t[sq][d] = static_cast<int16_t>(last);
        }
    return t;
}();

inline constexpr int knightOffsets[8] = { -18, -33, -31, -14, 18, 33, 31, 14 };
inline constexpr int kingOffsets[8]   = { -17, -16, -15,  1, 17, 16, 15, -1 };

static_assert(getTeam(RED) == TEAM_RY && getTeam(YELLOW) == TEAM_RY);
static_assert(getTeam(BLUE) == TEAM_BG && getTeam(GREEN) == TEAM_BG);
static_assert(getTeam(NONE_COLOR) == NONE_COLOR);
static_assert(RED + 1 == BLUE && BLUE + 1 == YELLOW && YELLOW + 1 == GREEN && GREEN + 1 == RED);
static_assert(RED + 0 == RED && RED + 2 == YELLOW && RED + 4 == RED);


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
    CAPTURES,           // All captures
    QUIETS,             // All non-captures + under promotions
    CASTLING,           // Castling moves
    UNDERPROMOTION,     // Underpromotions
    QUEEN_PROMOTION,    // Underpromotions that capture + all queen promotions
};

constexpr int ranks[14] = {14, 13, 12, 11, 10, 9, 8 ,7, 6, 5, 4, 3, 2, 1};
constexpr char files[14] = {'a',  'b',  'c',  'd',  'e',  'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'};

class Move
{
    // Stockfish note
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





