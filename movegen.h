#pragma once
#include <cstdint>
#include <algorithm>

#include "position.h"

const int MAX_MOVES = 512;

struct MoveList
{
    explicit MoveList(Position pos);

    ExtMove *begin() { return moveList; }
    ExtMove *end() { return last; }
    const ExtMove *begin() const { return moveList; }
    const ExtMove *end() const { return last; }

    size_t size() const { return last - moveList; }

    bool contains(Move move) const { return std::find(begin(), end(), move) != end(); }

    void print() const;

    void sort()
    {
        std::sort(begin(), end(), [](ExtMove &a, ExtMove &b)
                  {
                      return a > b;
                  });
    }

private:
    ExtMove moveList[MAX_MOVES], *last;
};
