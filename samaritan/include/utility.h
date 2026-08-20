#pragma once

#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <ranges>

#include "position.h"

// ASCII lower-casing for command words and option names. UCI is
// case-insensitive for those; FEN strings and file paths are not, so this is
// applied per token rather than to the whole input line.
inline std::string toLower(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

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

const void fen_setEnPassant(GameState &state, const std::string field);

const void fen_setHalfmoveClock(GameState &state, const std::string field);

void print(Position &pos);

std::string positionToFEN(const Position &pos);