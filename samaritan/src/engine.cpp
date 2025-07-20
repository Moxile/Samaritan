#include "engine.h"

#include "search.h"

namespace samaritan
{
    const std::string modern_fen = "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
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
    Engine::Engine() : pos()
    {
        loadFEN(pos, modern_fen);


        auto* uciCommand = app.add_subcommand("uci", "[UCI] Start UCI protocol and identify the engine")
        ->callback([this]() { handleUCI(); });

        auto* isreadyCommand = app.add_subcommand("isready", "Ensure engine is fully initialized before continuing")
        ->callback([this]() { handleIsReady(); });

        auto* setOptionCommand = app.add_subcommand("setoption", "Set an engine option in UCI format")
        ->callback([this]() { handleSetOption(); });

        setOptionCommand->allow_extras();

        auto* ucinewgameCommand = app.add_subcommand("ucinewgame", "Start a new game")
        ->callback([this]() { handleUCINewGame(); });

        auto* positionCommand = app.add_subcommand("position", "Position setup and display")
        ->callback([this]() { handlePosition(); });
        
        positionCommand->add_option("mode", position_mode, "Position setup mode")
        ->required()
        ->check(CLI::IsMember({"classic", "modern", "fen"}));
        
        positionCommand->allow_extras();
        
        auto* goCommand = app.add_subcommand("go", "COMPLETE")
        ->callback([this]() { handleGo(); });

        auto* stopCommand = app.add_subcommand("stop", "COMPLETE")
        ->callback([this]() { handleStop(); });

        auto* exitCommand = app.add_subcommand("quit", "Quit the engine")
        ->callback([this]() { handleQuit(); });

        auto* perftCommand = app.add_subcommand("perft", "Run perft to given depth")
        ->callback([this]() { handlePerft(); });

        perftCommand->add_option("depth", perft_depth, "Depth to run perft")
        ->required()
        ->check(CLI::PositiveNumber);

        perftCommand->add_flag("-f,--full", perft_full, "Show full detailed report");
        perftCommand->add_flag("-s,--split", perft_split, "Show perft per move (split node counts)");
        perftCommand->add_flag("-c,--cumulative", perft_cumulative, "Show cumulative totals at each depth");

        auto* printCommand = app.add_subcommand("print", "Print current position")
        ->callback([this]() { handlePrint(); });

        auto* configCommand =  app.add_subcommand("config", "Show current configuration")
        ->callback([this]() { handleConfig();  });
    }

    void Engine::launch()
    {
        std::string line;
        while (true)
        {
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;

            std::transform(line.begin(), line.end(), line.begin(), ::tolower);
            std::vector<std::string> args = tokenize(line);
            args.insert(args.begin(), "samaritan");
            
            std::vector<const char*> argv;
            for (const auto& arg : args) argv.push_back(arg.c_str());
            int argc = static_cast<int>(argv.size());
            
            execute(argc, argv.data());
        }
    }

    void Engine::execute(int argc, const char* argv[])
    {
        try // execute command //
        {
            app.clear();

            perft_full = false;
            perft_split = false;
            perft_cumulative = false;
            
            app.parse(argc, argv);
        } 
        catch (const CLI::ParseError& e) { std::cerr << "info string cli invalid command" << std::endl; } 
        catch (const std::exception&  e) { std::cout << "info string " << e.what() << std::endl; }
    }

    void Engine::handleUCI()
    {
        std::cout << "id name Samaritan" << std::endl;
        std::cout << "id author Martin (Moxile)" << std::endl;
        std::cout << "uciok" << std::endl << std::flush;
    }

    void Engine::handleIsReady()
    {
        std::cout << "readyok" << std::endl << std::flush;
    }

    void Engine::handleSetOption()
    {
        const auto& extras = app.get_subcommand("setoption")->remaining();

        if (extras.size() != 4 || extras[0] != "name" || extras[2] != "value") 
            throw std::invalid_argument("expected format: setoption name <name> value <value>");

        const std::string& name  = extras[1];
        const std::string& value = extras[3];

        if (name == "debug")
        {
                if (value == "on" ) debug = true ;
            else if (value == "off") debug = false;
            else throw std::invalid_argument("invalid debug value: " + value);
        }

        else throw std::invalid_argument("unknown option name: " + name);
    }

    void Engine::handleUCINewGame()
    {

    }

    void Engine::handlePosition()
    {
        loadFEN(pos, modern_fen);
    }

    void Engine::handleGo()
    {
        std::cout << negaMax(pos, 4) << std::endl << std::flush;
    }

    void Engine::handleStop()
    {

    }

    void Engine::handleQuit() {
        std::exit(0);
    }

    void Engine::handlePerft() {
        perft(perft_depth, pos);
    }

    void Engine::handlePrint() {
        print(pos);
    }

    void Engine::handleConfig()
    {
        std::cout << "info string debug " << (debug ? "on" : "off") << std::endl;
    }
}