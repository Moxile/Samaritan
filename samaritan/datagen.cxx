#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "movegen.h"
#include "search.h"
#include "utility.h"

static const std::string MODERN_FEN =
    "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
    "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
    "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
    "14/"
    "bR,bP,10,gP,gR/"
    "bN,bP,10,gP,gN/"
    "bB,bPP,10,gP,gB/"
    "bQ,bP,10,gP,gK/"
    "bK,bP,10,gP,gQ/"
    "bB,bP,10,gP,gB/"
    "bN,bP,10,gP,gN/"
    "bR,bP,10,gP,gR/"
    "14/"
    "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
    "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";

enum class GameResult { WIN_RY, WIN_BG, DRAW };

struct RecordedPos {
    std::string fen;
    PieceColor  sideToMove;
};

static GameResult playGame(int depth, int randomPlies, int maxMoves,
                           std::vector<RecordedPos>& positions,
                           std::mt19937& rng, bool visual)
{
    Position pos;
    loadFEN(pos, MODERN_FEN);
    positions.clear();

    int  moveCount     = 0;
    bool boardPrinted  = false;

    while (true)
    {
        PieceColor curTurn = pos.gameStates.back().curTurn;
        MoveList   moves(pos);

        // Termination: no legal moves
        if (moves.size() == 0)
        {
            if (inCheck(pos, curTurn))
            {
                // The side that just moved delivered checkmate
                // curTurn is the checkmated side
                return (getTeam(curTurn) == TEAM_RY) ? GameResult::WIN_BG
                                                      : GameResult::WIN_RY;
            }
            return GameResult::DRAW;
        }

        // Termination: move limit
        if (moveCount >= maxMoves)
            return GameResult::DRAW;

        // Record position (after random opening phase, before making the move)
        if (moveCount >= randomPlies)
            positions.push_back({positionToFEN(pos), curTurn});

        // Select move
        Move chosen;
        if (moveCount < randomPlies)
        {
            std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
            chosen = *(moves.begin() + dist(rng));
        }
        else
        {
            pos.nnue.init_eval(curTurn);
            SearchInfo info = iterativeDeepening(pos, depth, /*silent=*/true);
            if (info.pv_length[0] == 0)
            {
                // Fallback: pick first legal move (shouldn't happen)
                chosen = *moves.begin();
            }
            else
            {
                chosen = info.pv_table[0][0];
            }
        }

        pos.move(chosen);
        moveCount++;

        if (visual)
        {
            if (boardPrinted)
                printf("\033[21A");  // 20 board lines + 1 move line
            print(pos);
            printf("  move %d\n", moveCount);
            boardPrinted = true;
        }
        else
        {
            std::cout << "\r  move " << moveCount << "  " << std::flush;
        }
    }
}

int main(int argc, char** argv)
{
    CLI::App app{"Samaritan self-play data generator"};

    int         numGames    = 1000;
    int         searchDepth = 4;
    int         randomPlies = 8;
    int         maxMoves    = 400;
    std::string outputPath  = "selfplay.txt";
    bool        visual      = false;

    app.add_option("--games",        numGames,    "Number of games to play")->capture_default_str();
    app.add_option("--depth",        searchDepth, "Search depth per move")->capture_default_str();
    app.add_option("--random-plies", randomPlies, "Random opening moves for diversity")->capture_default_str();
    app.add_option("--max-moves",    maxMoves,    "Max moves before declaring draw")->capture_default_str();
    app.add_option("--output",       outputPath,  "Output file path")->capture_default_str();
    app.add_flag  ("--visual",       visual,      "Live board display in terminal");

    CLI11_PARSE(app, argc, argv);

    std::ofstream out(outputPath, std::ios::app);
    if (!out)
    {
        std::cerr << "Error: cannot open output file: " << outputPath << "\n";
        return 1;
    }

    std::mt19937 rng(std::random_device{}());

    int winsRY = 0, winsBG = 0, draws = 0;

    for (int game = 1; game <= numGames; game++)
    {
        std::vector<RecordedPos> positions;
        GameResult result = playGame(searchDepth, randomPlies, maxMoves, positions, rng, visual);

        // Update counters
        if      (result == GameResult::WIN_RY) winsRY++;
        else if (result == GameResult::WIN_BG) winsBG++;
        else                                    draws++;

        // Score positions (WDL from side-to-move perspective: 1.0/0.5/0.0)
        for (const auto& rp : positions)
        {
            float score;
            if (result == GameResult::DRAW)
            {
                score = 0.5f;
            }
            else
            {
                PieceColor winningTeam = (result == GameResult::WIN_RY) ? TEAM_RY : TEAM_BG;
                score = (getTeam(rp.sideToMove) == winningTeam) ? 1.0f : 0.0f;
            }
            out << rp.fen << ";" << score << "\n";
        }

        {
            const char* resultStr = (result == GameResult::WIN_RY) ? "RY wins"
                                  : (result == GameResult::WIN_BG) ? "BG wins"
                                                                    : "Draw   ";
            std::cout << "\rGame " << game << "/" << numGames
                      << "  [" << resultStr << "]"
                      << "  moves: " << positions.size() + randomPlies
                      << "  recorded: " << positions.size()
                      << "  RY=" << winsRY << " BG=" << winsBG << " D=" << draws
                      << "\n";
        }
    }

    std::cout << "Done. Written to " << outputPath << "\n";
    return 0;
}
