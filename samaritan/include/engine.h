#pragma once

#include <CLI/CLI.hpp>

#include "utility.h"
#include "perft.h"

namespace samaritan
{

    class Engine
    {
        private:

            Position pos = Position();
            TranspositionTable tt;
            CLI::App app{"Samaritan Engine CLI"};

            // Configuration
            bool debug = false;

            // Path of the loaded SNN1 network, empty when evaluation is
            // material or random.
            std::string evalFile;

            // Position options
            std::string position_mode;
            std::string move_text;

            // Search options
            int depth = 10;

            // Perft options
            int  perft_depth;
            bool perft_full;
            bool perft_split;
            bool perft_cumulative;

            // UCI commands
            void handleUCI();
            void handleIsReady();
            void handleSetOption();
            void handleUCINewGame();
            void handlePosition();
            void handleMove();
            void handleGo();
            void handleStop();
            void handleQuit();

            // Evaluation
            void loadNetwork(const std::string& path);
            static Move parseMove(Position& position, const std::string& token);

            // Other commands
            void handlePerft();
            void handlePrint();
            void handleConfig();
            
        public:

            Engine();

            void launch();
            void execute(int argc, const char* argv[]);
    };

}
