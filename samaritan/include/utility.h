#pragma once

#include <cstring>
#include <iostream>
#include <cmath>
#include <cassert>
#include <string>
#include <ranges>

#include "position.h"

const void loadFEN(Position &pos, const std::string fen);

const void fen_setPlayerToMove(GameState &state, const std::string color);

const void fen_setCastlingRights(GameState &state, const std::string castling, const CastlingRights r);

const void fen_setBoard(Board &board, const std::string boardFEN);

void print(Position &pos);