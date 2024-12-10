#pragma once
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>

#include "types.h"
#include "move.h"

class Position {
public:
    PieceColor curTurn = NONE_COLOR;
    int posScore = 0;
    Move bestMove = Move(0, 0, 0, 0);
    int kingTracker[4];
    int castleRights = NO_CASTLING;
    PieceColor colorMailbox[224];
    PieceType pieceMailbox[224];
    int enpassants[4];
    int material_value[4] = {0, 0, 0, 0};
    int64_t hash_value = 0;
    int64_t piece_hashes_[4][6][224];
    int64_t turn_hashes_[4];
    
    int piece_weights[6] = {100, 250, 350, 600, 1000, 0};

    Position();

    void initHash();
    void setHash();
    void resetBoard();

    int64_t rand64()
    {
        int32_t t0 = rand();
        int32_t t1 = rand();
        return (((int64_t)t0) << 32) + (int64_t)t1;
    }

    void UpdatePieceHash(const PieceColor color, const PieceType piece, const int loc) 
    {
        hash_value ^= piece_hashes_[__builtin_ctz((unsigned int) color)][piece-1][loc];
    }

    void UpdateTurnHash(const PieceColor turn)
    {
        hash_value ^= turn_hashes_[__builtin_ctz((unsigned int) turn)];
    }

    constexpr bool isEmpty(int loc) const
    {
        return (pieceMailbox[loc] == NONE_PIECE);
    }

    constexpr bool isEnemy(int loc, PieceColor color) const
    {
        return !(colorMailbox[loc] & color);
    }

    constexpr bool isTeam(int loc, PieceColor color) const
    {
        return (colorMailbox[loc] & color);
    }

    constexpr void move(const Move &move)
    {
        int loc = move.from();
        int destination = move.to();
        if(pieceMailbox[destination] != NONE_PIECE)
        {
            material_value[__builtin_ctz((unsigned int)(colorMailbox[destination]))] -= piece_weights[pieceMailbox[destination]-1];
        }
        pieceMailbox[destination] = pieceMailbox[loc];
        colorMailbox[destination] = colorMailbox[loc];
        pieceMailbox[loc] = NONE_PIECE;
        colorMailbox[loc] = NONE_COLOR;
        if(pieceMailbox[destination] == KING) kingTracker[__builtin_ctz((unsigned int)(colorMailbox[destination]))] = destination;
        curTurn++;
        UpdatePieceHash(colorMailbox[destination], pieceMailbox[destination], destination);
        UpdateTurnHash(curTurn);
    }

    // long algebraic notation
    void moveLAN(std::string san_move)
    {
        // "e2e4"
        if (san_move.length() == 4)
        {
            int ini = 1+(san_move[0] - 'a') + (14 - (san_move[1] - '1')) * 16;
            int fin = 1+(san_move[2] - 'a') + (7 - (san_move[3] - '1')) * 16;
            Move m(fin, ini, 0, 0);
            move(m);
        }
        else
        {
            std::cerr << "Bad SAN move " << san_move << std::endl;
        }
    }


    const void loadFEN(const std::string fen);

    const void fen_setPlayerToMove(const std::string color);

    const void fen_setCastlingRights(const std::string castling, const CastlingRights r);

    const void fen_setBoard(const std::string boardFEN);

    bool isSquareAttacked(int square, PieceColor color, PieceColor baseColor) const;

    void print() const;

};

constexpr int offsetsNum[11] = { 0, 3, 3, 3, 3, 8, 4, 4, 8, 8, 16, };
constexpr int offsets[11][16] = {{},
    { -16, -17, -15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Red Pawn
    {  1,   17, -15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Blue Pawn
    {  16,  17,  15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Yellow Pawn
    { -1,  -17,  15,  0,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Green Pawn
    { -18, -33, -31, -14, 18, 33, 31,  14, 0,   0,   0,   0,  0,  0,  0,  0, }, // Knight
    { -17, -15,  17,  15, 0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Bishop
    { -16,  1,   16, -1,  0,  0,  0,   0,  0,   0,   0,   0,  0,  0,  0,  0, }, // Rook
    { -17, -16, -15,  1,  17, 16, 15, -1,  0,   0,   0,   0,  0,  0,  0,  0, }, // Queen
    { -17, -16, -15,  1,  17, 16, 15, -1,  0,   0,   0,   0,  0,  0,  0,  0, }, // King
    { -17, -16, -15,  1,  17, 16, 15, -1, -18, -33, -31, -14, 18, 33, 31, 14 }, // All
};

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

constexpr inline bool isInvalidLocation(int location) {
    return location < 1 || baseMailbox[location] == -1 || location > 220;
}

constexpr inline PieceColor getTeam(PieceColor color)
{
    assert(color != NONE_COLOR);
    return (color&TEAM_RY) ? TEAM_RY : TEAM_BG;
}