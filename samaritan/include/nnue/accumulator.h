#pragma once

#include "chess.h"
#include <array>

constexpr auto board_table = []
{
    std::array<Square, 225> arr{};
    int index = 0;
    for(int i = 0; i < 225; i++)
    {
        if(isInvalidLocation(i))
        {
            arr[i] = -1;
        }
        else
        {
            arr[i] = index++;
        }
    }
    return arr;
}();


 
class Accumulator
{
    public:
        static constexpr size_t FEATURE_COUT = 3844;
        std::array<int, FREATURE_COUT> input;
        std::vector<int> changes;

        reset()
        {
            input.fill(0);
        }

        constexpr int update_change(int feat)
        {
            if(changes.contains(feat))
            {
                changes.erase(std::remove(changes.begin(), changes.end(), feat), changes.end());
            }
            else
            {
            changes.push_back(feat);
            }
        }

        constexpr int get_board_feat(int loc, PieceType pie, PieceColor col) const
        {
            return board_table[loc] * 4*6 + pie * 4 + __builtin_ctz((unsigned int) col);
        }

        constexpr int get_turn_fen(PieceColor col) const
        {
            return __builtin_ctz((unsigned int) col) + 32480;
        }
}