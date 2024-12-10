#include <chrono>
#include <string>
#include "movegen.h"
#include "transposition_table.h"

TranspositionTable table = TranspositionTable(2'000'000);

template <
    class result_t = std::chrono::milliseconds,
    class clock_t = std::chrono::steady_clock,
    class duration_t = std::chrono::milliseconds>
auto since(std::chrono::time_point<clock_t, duration_t> const &start)
{
    return std::chrono::duration_cast<result_t>(clock_t::now() - start);
}

std::string getName()
{
    return "Samaritan";
};

std::string getAuthor()
{
    return "Moxile";
};

int mateValue = 1000000; // in centipawns
const int maxdepth = 10;

int nodes = 0;

int evaluate(Position pos)
{
    int value = 0;

    // material
    int team = (pos.curTurn&TEAM_RY) ? 1 : -1;
    int ry_material = (pos.material_value[0] + pos.material_value[2]) * team;
    int bg_material = (pos.material_value[1] + pos.material_value[3]) * -team;
    value += ry_material + bg_material;

    return value;
};

int game_over(const Position pos)
{
    if (pos.isSquareAttacked(pos.kingTracker[__builtin_ctz(pos.curTurn)], pos.curTurn, getTeam(pos.curTurn)))
    {
        return -mateValue;
    }
    return 0;
};

int alphaBeta(int alpha, int beta, int depthleft, Position& pos, int ply)
{
    // Transposition table
#if 0
    const HashTableEntry* entry = table.Get(pos.hash_value);
    if (entry != nullptr && entry->key == pos.hash_value && !entry->is_pv)
    {
        if (entry->bound == EXACT)
        {
            return entry->score;
        }
        if (entry->bound == LOWER_BOUND && entry->score >= beta)
        {
            return entry->score;
        }
        if (entry->bound == UPPER_BOUND && entry->score <= alpha)
        {
            return entry->score;
        }
    }
#endif    

    int score;
    MoveList moveList = MoveList(pos);
    moveList.sort();
    if (moveList.size() == 0)
    {
        return game_over(pos);
    }
    if (depthleft == 0)
    {
        return evaluate(pos);
    }

    for (ExtMove move : moveList)
    {
        // search extension for captures
        int new_depth = depthleft - 1;
        if (move.gen_type == CAPTURES && new_depth == 0 && ply < maxdepth)
        {
            new_depth++;
        }

        // make the move on the board
        nodes++;
        Position temp = pos;
        temp.move(move);

        // search
        score = -alphaBeta(-beta, -alpha, new_depth, temp, ++ply);
        if (score >= beta)
        {
            table.Save(pos.hash_value, new_depth, pos.bestMove, score, LOWER_BOUND, 0);
            return beta; //  fail hard beta-cutoff
        }
        if (score > alpha)
        {
            alpha = score; // alpha acts like max in MiniMax
            pos.posScore = alpha;
            pos.bestMove = move;
            table.Save(pos.hash_value, new_depth, move, alpha, EXACT, 1);
        }
        else
        {
            table.Save(pos.hash_value, new_depth, move, alpha, LOWER_BOUND, 0);
        }
    }
    return alpha;
}


void search(Position &pos, int maxTime)
{
    auto start = std::chrono::steady_clock::now();
    for (int depth = 1; depth <= maxdepth; depth++)
    {
        nodes = 0;
        auto search = std::chrono::steady_clock::now();
        int score = alphaBeta(-1316134912, 1316134912, depth, pos, 0);
        /*
        - Move ordering. PV moves first!
            Assign value to ExtMove, then selection sort for move ordering.=
            The value is decided by checking PV, Hash and other (for example captures)
        - Null Move Pruning
            In special cases a null move is executed
            Check whether value is out of bound. -> Prune
        - Late Move Reduction
        */

        float time = since(search).count();
        std::cout << "info score " << score << " depth " << depth << " cn " << nodes << " time " << time << " nps " << (nodes / time) * 1000 << " pv " <<  pos.bestMove.toUCI() << std::endl;
        if (since(start).count() > maxTime * 1000)
            break;
    }
};
