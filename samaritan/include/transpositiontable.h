#pragma once

#include <stdint.h>
#include <random>
#include <vector>
#include <cstring>



static uint64_t zobristPieces[160][6][4];
static uint64_t zobristTurn[4];
static uint64_t zobristCastle[8];
static uint64_t zobristEnPassant[4][160];

static void initZobrist() {
    std::mt19937_64 rng(123456789ULL);

    for (int sq = 0; sq < 160; sq++)
        for (int piece = 0; piece < 6; piece++)
            for (int color = 0; color < 4; color++)
                zobristPieces[sq][piece][color] = rng();

    for (int i = 0; i < 4; i++)
        zobristTurn[i] = rng();

    for (int i = 0; i < 8; i++)
        zobristCastle[i] = rng();

    for (int player = 0; player < 4; player++)
        for (int sq = 0; sq < 160; sq++)
            zobristEnPassant[player][sq] = rng();
}


enum TTFlag : uint8_t
{
    TT_EXACT,
    TT_LOWER,
    TT_UPPER
};

struct TTEntry
{
    uint64_t key  = 0;
    int      score = 0;
    Move     bestMove;
    int8_t   depth = -1;
    TTFlag   flag  = TT_EXACT;
};

class TranspositionTable
{
    std::vector<TTEntry> table;
    uint64_t mask = 0;

public:
    void resize(size_t mb)
    {
        size_t count = (mb * 1024 * 1024) / sizeof(TTEntry);
        count = [](size_t n) {
            size_t p = 1;
            while (p * 2 <= n) p *= 2;
            return p;
        }(count);
        table.resize(count);
        std::memset(table.data(), 0, count*sizeof(TTEntry));
        mask = count - 1;
    }

    TTEntry* probe(uint64_t key)
    {
        TTEntry* entry = &table[key & mask];
        return entry->key == key ? entry : nullptr;
    }

    void store(uint64_t key, int score, Move move, int depth, TTFlag flag)
    {
        TTEntry* entry = &table[key & mask];
        entry->key      = key;
        entry->score    = score;
        entry->bestMove = move;
        entry->depth    = static_cast<int8_t>(depth);
        entry->flag     = flag;
    }
};
