#pragma once

#include "chess.h"

#include <vector>
#include <algorithm>

constexpr int board_table[] = 
{
    -1,  -1,  -1,  -1,   0,   1,   2,   3,   4,   5,   6,   7,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,   8,   9,  10,  11,  12,  13,  14,  15,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  16,  17,  18,  19,  20,  21,  22,  23,  -1,  -1,  -1,  -1,
    -1,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  -1,
    -1,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  -1,
    -1,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  -1,
    -1,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  -1,
    -1,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  -1,
    -1,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107,  -1,
    -1, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,  -1,
    -1, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,  -1,
    -1,  -1,  -1,  -1, 136, 137, 138, 139, 140, 141, 142, 143,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1, 144, 145, 146, 147, 148, 149, 150, 151,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  -1, 152, 153, 154, 155, 156, 157, 158, 159,  -1,  -1,  -1,  -1
};
 
class Accumulator
{
    public:
        static constexpr size_t FEATURE_COUNT = 3844;

        std::vector<float> input;
        std::vector<int> changes = {};

        void reset()
        {
            input.resize(FEATURE_COUNT, 0);
        }

        void set(int feat)
        {
            changes.push_back(feat);
        }

        void clear(int feat)
        {
            auto it = std::find(changes.begin(), changes.end(), feat);
            if (it != changes.end()) {
                changes.erase(it);
            }
        }
};

constexpr int get_board_feat(int loc, PieceType pie, PieceColor col)
{
    return board_table[loc] * 4*6 + pie * 4 + __builtin_ctz((unsigned int) col);
}

constexpr int get_turn_feat(PieceColor col)
{
    return __builtin_ctz((unsigned int) col) + 3240;
}