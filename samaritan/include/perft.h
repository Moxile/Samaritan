#pragma once

#include <iostream>

#include <chrono>
#include "movegen.h"

int fullsearch(int depth, Position &pos)
{
    if (depth == 1)
    {
        MoveList moves = MoveList(pos);
        return moves.size();
    }

    int nodes = 0;
    MoveList moves = MoveList(pos);
    for (Move move : moves)
    {
        pos.move(move);
        nodes += fullsearch(depth - 1, pos);
        pos.undoMove(move);
    }

    return nodes;
};

void perft(int initial_depth, Position &pos)
{
    std::cout << "Perft depth " << initial_depth << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    int nodes = fullsearch(initial_depth, pos);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Nodes per second (nps) as integer (or double if you want decimals)
    double nps = nodes / elapsed.count();

    std::cout << "Nodes: " << nodes
              << "  Time: " << elapsed.count() << " sec"
              << "  NPS: " << static_cast<long long>(nps)
              << std::endl;
};

void branch_test(int initial_depth, Position &pos)
{
    std::cout << "Branch test Depth " << initial_depth << std::endl;

    MoveList moves = MoveList(pos);
    for (Move move : moves)
    {
        std::cout << move.toUCI();
        pos.move(move);
        int nodes = fullsearch(initial_depth - 1, pos);
        pos.undoMove(move);
        std::cout << ": " << nodes << std::endl;
    }
}
