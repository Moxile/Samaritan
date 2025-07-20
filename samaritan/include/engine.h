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
            CLI::App app{"Athena Engine CLI"};

            // Configuration
            bool debug = false;

            // Position options
            std::string position_mode;

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
            void handleGo();
            void handleStop();
            void handleQuit();

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
