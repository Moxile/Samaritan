#include "engine.h"

#include "search.h"
#include "nnue/network.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

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

        initZobrist();
        loadFEN(pos, modern_fen);
        tt.resize(64);

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
        ->check(CLI::IsMember({"classic", "modern", "startpos", "fen"}));
        
        positionCommand->allow_extras();

        // Convenience command for interactive use. UCI GUIs normally rebuild
        // the position with `position ... moves ...`; accepting one move at a
        // time makes the terminal interface much more pleasant to use.
        auto* moveCommand = app.add_subcommand("move", "Play one legal UCI-style move")
        ->alias("makemove")
        ->callback([this]() { handleMove(); });

        moveCommand->add_option("uci-move", move_text, "Move such as h2h3 or j13j14q")
        ->required();
        
        auto* goCommand = app.add_subcommand("go", "COMPLETE")
        ->callback([this]() { handleGo(); });

        goCommand->allow_extras();

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
            // A GUI that writes CRLF down a pipe leaves the CR on the last
            // token, so "go depth 6\r" parses as a depth of "6\r".
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            if (line.empty()) continue;

            std::vector<std::string> args = tokenize(line);
            // Only the command word is case-folded. Lower-casing the whole line
            // used to be harmless when `position` ignored its arguments; it
            // destroys a FEN ("R-0,0..." and "yR" both carry meaning in case).
            if (!args.empty()) args[0] = toLower(args[0]);
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
        // `random` is named for what it is. It exercises the whole NNUE path at
        // full cost but plays no better than noise, so it must not be reachable
        // by asking for "nnue" -- that word now means a network from a file.
        std::cout << "option name eval type combo default material var material var random var nnue" << std::endl;
        std::cout << "option name evalfile type string default <empty>" << std::endl;
        std::cout << "option name hash type spin default 64 min 1 max 4096" << std::endl;
        std::cout << "uciok" << std::endl << std::flush;
    }

    void Engine::handleIsReady()
    {
        std::cout << "readyok" << std::endl << std::flush;
    }

    void Engine::handleSetOption()
    {
        const auto& extras = app.get_subcommand("setoption")->remaining();

        if (extras.size() != 4 || toLower(extras[0]) != "name" || toLower(extras[2]) != "value")
            throw std::invalid_argument("expected format: setoption name <name> value <value>");

        // UCI option names are case-insensitive; the value of `evalfile` is a
        // path, so that one keeps its case.
        const std::string  name  = toLower(extras[1]);
        const std::string& rawValue = extras[3];
        const std::string  value = toLower(rawValue);

        if (name == "debug")
        {
                if (value == "on" ) debug = true ;
            else if (value == "off") debug = false;
            else throw std::invalid_argument("invalid debug value: " + value);
        }

        else if (name == "hash")
        {
            const int mb = std::stoi(value);
            if (mb < 1 || mb > 4096) throw std::invalid_argument("hash must be 1..4096 MB");
            tt.resize(static_cast<size_t>(mb));
            std::cout << "info string hash " << mb << " MB" << std::endl;
        }

        // setoption name evalfile value <path to an SNN1 network>
        else if (name == "evalfile")
        {
            loadNetwork(rawValue);
        }

        // setoption name eval value material|random|nnue
        else if (name == "eval")
        {
            if (value == "material")
            {
                nnue::network().clear();
                // The stack the position carries was computed from weights that
                // no longer exist; drop it in the same breath as the weights.
                pos.refreshEval();
                std::cout << "info string eval material" << std::endl;
            }
            else if (value == "random")
            {
                const auto t0 = std::chrono::steady_clock::now();
                nnue::network().randomize(42);
                const double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
                const double mb = double(nnue::NUM_FEATURES) * nnue::ACC_SIZE * sizeof(float) / 1048576.0;
                // Rebuilding here is what makes the switch safe mid-game: the
                // accumulators must describe the position under the *new*
                // weights before the next search touches them.
                pos.refreshEval();
                std::cout << "info string eval random (RANDOM WEIGHTS - plumbing only, not a real evaluation)"
                          << std::endl;
                std::cout << "info string nnue " << nnue::NUM_FEATURES << " features x "
                          << nnue::ACC_SIZE << ", " << mb << " MB, initialised in "
                          << ms << " ms" << std::endl;
            }
            else if (value == "nnue")
            {
                if (evalFile.empty())
                    throw std::invalid_argument(
                        "no network loaded: setoption name evalfile value <path> first");
                loadNetwork(evalFile);
            }
            else throw std::invalid_argument("invalid eval value: " + value);
        }

        else throw std::invalid_argument("unknown option name: " + name);
    }

    // Load an SNN1 network and make the current position consistent with it.
    // Failure leaves the previous evaluator untouched and says why.
    void Engine::loadNetwork(const std::string& path)
    {
        std::string error;
        const auto t0 = std::chrono::steady_clock::now();
        if (!nnue::network().load(path, &error))
        {
            std::cout << "info string evalfile rejected: " << error << std::endl;
            return;
        }
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        evalFile = path;
        pos.refreshEval();
        std::cout << "info string eval nnue, network " << path << " loaded in "
                  << ms << " ms" << std::endl;
    }

    void Engine::handleUCINewGame()
    {
        initZobrist();
        loadFEN(pos, modern_fen);
        // for now reset, can be removed and keep warm tt though
        tt.resize(64);

    }

    // Does this string describe a position we can actually play from? A FEN with
    // no board, or one whose derived state does not add up, is rejected before
    // it can replace the current game.
    static bool fenIsUsable(const std::string& fen, std::string& why)
    {
        Position probe(false);
        loadFEN(probe, fen);

        int pieces = 0;
        for (int c = 0; c < 4; ++c) pieces += probe.board.pieceCount[c];
        if (pieces == 0) { why = "no pieces on the board"; return false; }

        return probe.isConsistent(&why);
    }

    void Engine::handlePosition()
    {
        const auto& extras = app.get_subcommand("position")->remaining();
        const std::string mode = toLower(position_mode);

        size_t i = 0;
        std::string base = modern_fen;
        if (mode == "fen")
        {
            if (extras.empty() || toLower(extras[0]) == "moves")
                throw std::invalid_argument("position fen <fen> [moves ...]");
            // Parsed into a scratch position first. loadFEN cannot fail loudly
            // -- a string it does not understand simply produces an empty board
            // -- so a typo would otherwise silently replace the game with
            // nothing and every later command would answer about that.
            std::string why;
            if (!fenIsUsable(extras[0], why))
                throw std::invalid_argument("bad fen (" + why + ")");
            base = extras[0];
            loadFEN(pos, base);
            i = 1;
        }
        else
        {
            // `classic` and `modern` differ in rules (points, dead players),
            // not in starting placement, so both start from the same board.
            loadFEN(pos, base);
        }

        if (i < extras.size())
        {
            if (toLower(extras[i]) != "moves")
                throw std::invalid_argument("expected 'moves' after the position");
            for (++i; i < extras.size(); ++i)
            {
                Move m = parseMove(pos, toLower(extras[i]));
                if (isNone(m))
                {
                    // All or nothing: half a move list is a position nobody
                    // asked for, and the next `go` would search it in earnest.
                    loadFEN(pos, base);
                    throw std::invalid_argument("illegal or unparsable move: " + extras[i]);
                }
                pos.move(m);
            }
        }
    }

    // "a1n14" style coordinates, optionally followed by a promotion piece
    // ("j13j14q"). Squares are matched against the legal move list rather than
    // decoded into flags, so castling, en passant and promotion come out right
    // without the caller having to know which of them applies.
    Move Engine::parseMove(Position& position, const std::string& token)
    {
        auto square = [](const std::string& s, size_t& at) -> int {
            if (at >= s.size()) return -1;
            const char file = s[at];
            if (file < 'a' || file > 'n') return -1;
            ++at;
            int rank = 0, digits = 0;
            while (at < s.size() && std::isdigit(static_cast<unsigned char>(s[at])))
            {
                rank = rank * 10 + (s[at] - '0');
                ++at; ++digits;
                if (rank > 14) return -1;         // no rank has three digits
            }
            if (digits == 0 || rank < 1) return -1;
            const int row = 14 - rank;            // row 0 is rank 14
            return row * 16 + (file - 'a' + 1);
        };

        size_t at = 0;
        const int from = square(token, at);
        const int to   = square(token, at);
        if (from < 0 || to < 0) return MOVE_NONE;

        PieceType promo = NONE_PIECE;
        if (at < token.size())
        {
            switch (token[at])
            {
                case 'q': promo = QUEEN;  break;
                case 'r': promo = ROOK;   break;
                case 'b': promo = BISHOP; break;
                case 'n': promo = KNIGHT; break;
                default:  return MOVE_NONE;
            }
            ++at;
        }
        if (at != token.size()) return MOVE_NONE;

        Move fallback = MOVE_NONE;
        for (const Move& m : MoveList(position))
        {
            if (m.from() != from || m.to() != to) continue;
            if (promo != NONE_PIECE)
            {
                if (m.promotion() == promo) return m;
                continue;
            }
            // No suffix given: a promotion defaults to a queen, which is what
            // every interface means by it.
            if (m.special_move() == 1 || m.special_move() == 3)
            {
                if (m.promotion() == QUEEN) return m;
                continue;
            }
            if (isNone(fallback)) fallback = m;
        }
        return fallback;
    }

    void Engine::handleMove()
    {
        Move move = parseMove(pos, toLower(move_text));
        if (isNone(move))
            throw std::invalid_argument("illegal or unparsable move: " + move_text);

        pos.move(move);
        std::cout << "info string played " << move.toUCI() << std::endl;
    }

    void Engine::handleGo()
    {
        const auto& extras = app.get_subcommand("go")->remaining();
        depth = 6;
        for (size_t i = 0; i + 1 < extras.size(); i++) {
            if (extras[i] == "depth")
                depth = std::stoi(extras[i + 1]);
        }

        SearchInfo info = iterativeDeepening(pos, tt, depth);
        // A terminal position has no principal variation. Printing
        // pv_table[0][0] regardless meant formatting MOVE_NONE, whose square 0
        // indexes off the front of the file table.
        if (info.pv_length[0] > 0)
            std::cout << "bestmove " << info.pv_table[0][0].toUCI() << std::endl;
        else
            std::cout << "bestmove 0000" << std::endl;
    }

    void Engine::handleStop()
    {

    }

    void Engine::handleQuit() {
        std::exit(0);
    }

    void Engine::handlePerft() {
        Position perft_pos(false);
        loadFEN(perft_pos, positionToFEN(pos));
        perft(perft_depth, perft_pos);
    }

    void Engine::handlePrint() {
        print(pos);
    }

    void Engine::handleConfig()
    {
        std::cout << "info string debug " << (debug ? "on" : "off") << std::endl;
    }
}
