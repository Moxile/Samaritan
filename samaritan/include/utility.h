#pragma once

#include <cstring>
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <ranges>

#include "position.h"

inline auto tokenize(const std::string& line, char delimiter = ' ')
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (std::getline(iss, token, delimiter))
        if (!token.empty())
            tokens.push_back(token);
    return tokens;
}

const void loadFEN(Position &pos, const std::string fen);

const void fen_setPlayerToMove(GameState &state, const std::string color);

const void fen_setCastlingRights(GameState &state, const std::string castling, const CastlingRights r);

const void fen_setBoard(Board &board, const std::string boardFEN);

void print(Position &pos);