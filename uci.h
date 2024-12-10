#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iterator>

#include "engine.h"

std::string fen = "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
                     "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
                     "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
                     "14/"
                     "bR,bP,10,gP,gR/"
                     "bN,bP,10,gP,gN/"
                     "bB,bP,10,gP,gB/"
                     "bQ,bP,10,gP,gK/"
                     "bK,bP,10,gP,gQ/"
                     "bB,bP,10,gP,gB/"
                     "bN,bP,10,gP,gN/"
                     "bR,bP,10,gP,gR/"
                     "14/"
                     "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
                     "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";
class UCI
{
private:
    Position &pos;

    std::ofstream log_file;

public:
    UCI(Position &b) : pos(b) {}

    void uci_response(std::string r) {
        std::cout << r << std::endl;
        log_file << "O: " << r << std::endl; 
    }

    bool uci_command(const std::string &input)
    {
        if (input == "uci")
        {
            log_file << "I: " << input << std::endl;
            // Send UCI identification information
            uci_response(getName());
            uci_response(getAuthor());
            // ... other UCI options ...
            uci_response("uciok");
        }
        else if (input == "isready")
        {
            // Respond to 'isready' command
            std::cout << "uciok" << std::endl;
            std::cout << "readyok" << std::endl;
        }
        else if (input.substr(0, 8) == "position")
        {
            log_file << "I: " << input << std::endl;
            uci_cmd_position(input);
        }
        else if (input.substr(0, 2) == "go")
        {
            log_file << "I: " << input << std::endl;
            //int score = engine.think(board);
            search(pos, 10);
            std::stringstream ss;
            ss << "bestmove " << pos.bestMove.toUCI();
            uci_response(ss.str());
            pos.move(pos.bestMove);
        }
        else if (input == "quit")
        {
            log_file << "I: " << input << std::endl;
            // Exit loop
            return false;
        }
        else if (input == "ucinewgame")
        {
            log_file << "I: " << input << std::endl;
            pos = Position();
            pos.loadFEN(fen);

        }
        else if (input == "print") 
        {
            pos.print();
        }

        // ... handle other UCI commands ...

        return true;
    }

    void uci_loop()
    {        
        log_file.open("uci_text.log");
        log_file << "== new session ==\n";
        
        while (true)
        {
            std::string input;
            std::getline(std::cin, input);            

            if (!uci_command(input))
                break;
            log_file.flush();                
        }
        log_file.close();
    }

    // Function to parse the position command
    void uci_cmd_position(const std::string &input)
    {
        std::istringstream iss(input);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                        std::istream_iterator<std::string>{}};

        // Check if the command is valid and contains the "position" keyword
        auto positionKeywordPos = std::find(tokens.begin(), tokens.end(), "position");
        if (positionKeywordPos != tokens.end())
        {
            // Find the position type (fen or startpos)
            auto fenPos = std::find(tokens.begin(), tokens.end(), "fen");
            if (fenPos != tokens.end() && std::next(fenPos) != tokens.end())
            {
                std::string fenString = *(std::next(fenPos));
                //std::cout << "Setting up position with FEN: " << fenString << std::endl;
                pos.loadFEN(fenString);
            }
            else if (std::find(tokens.begin(), tokens.end(), "startpos") != tokens.end())
            {
                // Reset to start position
                pos = Position();
            }

            // Process the list of moves if provided
            auto movesPos = std::find(tokens.begin(), tokens.end(), "moves");
            if (movesPos != tokens.end() && std::next(movesPos) != tokens.end())
            {                
                std::vector<std::string> moves(std::next(movesPos), tokens.end());
                // Process the list of moves
                for (const auto &move : moves)
                {                    
                    pos.moveLAN(move);
                }
            }
        }
        else
        {
            std::cout << "Invalid position command." << std::endl;
        }
    }
};
