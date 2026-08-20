#include "utility.h"

#ifdef _WIN32
#include <windows.h>

// The board is drawn with ANSI colour escapes. A Windows console understands
// them only once virtual-terminal processing is switched on, which is a runtime
// call rather than a compile-time one -- without it the escapes are printed
// literally and the board becomes unreadable.
static void enableAnsiColors()
{
    static bool done = false;
    if (done) return;
    done = true;

    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(out, &mode)) return;   // redirected to a file: nothing to set
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
static void enableAnsiColors() {}
#endif

const void loadFEN(Position &pos, const std::string fen)
{
    pos.gameStates.clear();

    // Reset every piece of board state the FEN is about to define. Without this
    // a second loadFEN into the same Position keeps the previous position's
    // pieces and double-counts kingTracker / nonPawnPieceCount.
    std::memset(pos.board.pieceMailbox, NONE_PIECE, sizeof(pos.board.pieceMailbox));
    std::memset(pos.board.colorMailbox, NONE_COLOR, sizeof(pos.board.colorMailbox));
    std::fill(std::begin(pos.board.kingTracker), std::end(pos.board.kingTracker), -1);
    std::fill(std::begin(pos.board.nonPawnPieceCount), std::end(pos.board.nonPawnPieceCount), 0);

    GameState initialState;
    initialState.zobristKey = 0;
    // Fields are '-' separated:
    //   0 turn | 1 dead | 2 castle O-O | 3 castle O-O-O | 4 points | 5 halfmove
    //   6 en passant (optional) | board
    // The en-passant field is only present when some player actually has one,
    // so the board is the last field either way. This mirrors the reference
    // engine's parser exactly, so the two stay round-trip compatible.
    std::vector<std::string> parts;
    for (auto part : fen | std::views::split('-') | std::views::transform([](auto v)
                     { auto c = v | std::views::common;
                       return std::string(c.begin(), c.end()); }))
        parts.push_back(part);

    if (parts.size() > 0) fen_setPlayerToMove(initialState, parts[0]);
    if (parts.size() > 2) fen_setCastlingRights(initialState, parts[2], RED_OO);
    if (parts.size() > 3) fen_setCastlingRights(initialState, parts[3], RED_OOO);
    if (parts.size() > 5) fen_setHalfmoveClock(initialState, parts[5]);

    std::string epStr, boardStr;
    if (parts.size() >= 8)
    {
        epStr    = parts[6];
        boardStr = parts[7];
    }
    else if (parts.size() == 7)
    {
        if (parts[6].find('/') != std::string::npos) boardStr = parts[6];
        else                                         epStr    = parts[6];
    }

    if (!epStr.empty() && epStr.find("enPassant") != std::string::npos)
        fen_setEnPassant(initialState, epStr);

    if (!boardStr.empty()) fen_setBoard(pos.board, boardStr);

    // Set Zobrist Key
    for (int sq = 0; sq < 224; sq++) {
        if (pos.board.pieceMailbox[sq] == NONE_PIECE) continue;
        int idx = board_table[sq];
        int piece = pos.board.pieceMailbox[sq] - 1;
        int color = ctz((unsigned int)pos.board.colorMailbox[sq]);
        initialState.zobristKey ^= zobristPieces[idx][piece][color];
    }

    initialState.zobristKey ^= zobristTurn[ctz((unsigned int)initialState.curTurn)];

    int rights = initialState.castleRights;
    for (int i = 0; i < 8; i++)
        if (rights & (1 << i))
            initialState.zobristKey ^= zobristCastle[i];

    for (int player = 0; player < 4; player++) {
        int ep = initialState.enpassants[player];
        if (ep != -1)
            initialState.zobristKey ^= zobristEnPassant[player][board_table[ep]];
    }

    pos.board.rebuildPieceList();

    pos.gameStates.push_back(initialState);
    pos.refreshEval();
}

// "a3" or "a3:a4" (target before the colon), optionally single-quoted.
// Returns a mailbox location, or -1 when the token is empty or malformed.
static int parseEnPassantSquare(std::string s)
{
    if (!s.empty() && s.front() == '\'') s.erase(s.begin());
    if (!s.empty() && s.back()  == '\'') s.pop_back();
    if (s.empty()) return -1;

    const auto colon = s.find(':');
    if (colon != std::string::npos) s = s.substr(0, colon);
    if (s.size() < 2) return -1;

    const int file = s[0] - 'a';                       // 'a' -> column 1
    int rank = 0;
    try { rank = std::stoi(s.substr(1)); } catch (...) { return -1; }
    if (file < 0 || file > 13 || rank < 1 || rank > 14) return -1;

    const int row = 14 - rank;                         // row 0 is rank 14
    return row * 16 + (file + 1);
}

// Plies since the last capture or pawn move. This is not decoration: it drives
// the 200-ply progress draw, so a position reloaded from FEN with the field
// dropped restarts the clock and postpones a draw that was nearly due.
//
// pliesFromNull is deliberately *not* serialised. It is search-only state
// describing an artificial null move inside a tree, and a null move never
// belongs to a real game the FEN describes.
const void fen_setHalfmoveClock(GameState &state, const std::string field)
{
    try { state.halfmoveClock = std::stoi(field); }
    catch (...) { state.halfmoveClock = 0; return; }
    if (state.halfmoveClock < 0) state.halfmoveClock = 0;
}

// Field shape: {'enPassant':('a3','','','')} -- one slot per colour, R B Y G.
const void fen_setEnPassant(GameState &state, const std::string field)
{
    const auto open  = field.find('(');
    const auto close = field.find(')');
    if (open == std::string::npos || close == std::string::npos || close < open) return;

    const std::string content = field.substr(open + 1, close - open - 1);
    std::stringstream ss(content);
    std::string token;
    for (int c = 0; c < 4 && std::getline(ss, token, ','); ++c)
        state.enpassants[c] = parseEnPassantSquare(token);
}

const void fen_setPlayerToMove(GameState &state, const std::string color)
{
    switch (color.at(0))
    {
    case 'R':
        state.curTurn = PieceColor::RED;
        break;
    case 'B':
        state.curTurn = PieceColor::BLUE;
        break;
    case 'Y':
        state.curTurn = PieceColor::YELLOW;
        break;
    case 'G':
        state.curTurn = PieceColor::GREEN;
        break;
    default:
        break;
    }
}

const void fen_setCastlingRights(GameState &state, const std::string castling, const CastlingRights r)
{
    auto rights = castling | std::views::split(',') | std::views::transform([](auto v)
                                                                            {
        auto c = v | std::views::common;
        return std::string(c.begin(), c.end()); });

    int color = r;

    for (auto right : rights)
    {
        if (right.at(0) == '1')
        {
            state.castleRights |= color;
        }
        color <<= 2;
    }
}

const void fen_setBoard(Board &board, const std::string boardFEN)
{
    auto rows = boardFEN | std::views::split('/') | std::views::transform([](auto v)
                                                                          {
                    auto c = v | std::views::common;
                    return std::string(c.begin(), c.end()); });

    int row_num = 0;
    for (auto row : rows)
    {
        auto pieces = row | std::views::split(',') | std::views::transform([](auto v)
                                                                           {
                    auto c = v | std::views::common;
                    return std::string(c.begin(), c.end()); });

        int col_num = 1;
        for (auto piece : pieces)
        {
            try
            {
                // size_t pos = 0;
                const int i{std::stoi(piece)};
                col_num += i;
            }
            catch (std::invalid_argument const &ex)
            {
                const auto loc = 16 * row_num + col_num;
                auto &squareColor = board.colorMailbox[loc];
                auto &squarePiece = board.pieceMailbox[loc];

                switch (piece.at(0))
                {
                case 'r':
                    squareColor = RED;
                    break;
                case 'b':
                    squareColor = BLUE;
                    break;
                case 'y':
                    squareColor = YELLOW;
                    break;
                case 'g':
                    squareColor = GREEN;
                    break;
                default:
                    break;
                }

                // Guarded: a malformed FEN can leave the square colourless,
                // and ctz(0) is 32, which indexes nothing valid.
                const auto colorIdx = squareColor != NONE_COLOR
                                    ? ctz((unsigned int)squareColor)
                                    : 0;

                switch (piece.at(1))
                {
                case 'P':
                    squarePiece = PieceType::PAWN;
                    break;
                case 'N':
                    squarePiece = PieceType::KNIGHT;
                    break;
                case 'B':
                    squarePiece = PieceType::BISHOP;
                    break;
                case 'R':
                    squarePiece = PieceType::ROOK;
                    break;
                case 'Q':
                    squarePiece = PieceType::QUEEN;
                    break;
                case 'K':
                    squarePiece = PieceType::KING;
                    board.kingTracker[colorIdx] = loc;
                    break;
                default:
                    break;
                }

                // Every piece except the pawn counts toward null-move pruning.
                if (squarePiece != NONE_PIECE && squarePiece != PieceType::PAWN)
                {
                    board.nonPawnPieceCount[colorIdx]++;
                }

                col_num++;
            }
        }

        row_num++;
    }
}

const char *colorToStringColor(const PieceColor color)
{
    switch (color)
    {
    case RED:
        return " \033[1;31m%c\033[0m  ";
        break;
    case BLUE:
        return " \033[1;34m%c\033[0m  ";
        break;
    case YELLOW:
        return " \033[1;33m%c\033[0m  ";
        break;
    case GREEN:
        return " \033[1;32m%c\033[0m  ";
        break;
    default:
        return " %c ";
    }
}

constexpr char piece2char(PieceType type)
{
    switch (type)
    {
    case PAWN:
        return 'P';
    case KNIGHT:
        return 'N';
    case BISHOP:
        return 'B';
    case ROOK:
        return 'R';
    case QUEEN:
        return 'Q';
    case KING:
        return 'K';
    default:
        return '?';
    }
}

std::string positionToFEN(const Position &pos)
{
    const GameState &state = pos.gameStates.back();

    // Turn
    char turnChar = '?';
    switch (state.curTurn) {
        case RED:    turnChar = 'R'; break;
        case BLUE:   turnChar = 'B'; break;
        case YELLOW: turnChar = 'Y'; break;
        case GREEN:  turnChar = 'G'; break;
        default: break;
    }

    // Castling OO: R,B,Y,G
    auto castleBit = [&](CastlingRights r) -> char {
        return (state.castleRights & r) ? '1' : '0';
    };
    std::string oo  = std::string(1, castleBit(RED_OO))    + "," + castleBit(BLUE_OO)    + "," + castleBit(YELLOW_OO)    + "," + castleBit(GREEN_OO);
    std::string ooo = std::string(1, castleBit(RED_OOO))   + "," + castleBit(BLUE_OOO)   + "," + castleBit(YELLOW_OOO)   + "," + castleBit(GREEN_OOO);

    // Board: 14 rows, cols 1-14
    std::string board;
    for (int row = 0; row < 14; row++)
    {
        if (row > 0) board += '/';
        int emptyCount = 0;
        bool firstCell = true;

        auto flushEmpty = [&]() {
            if (emptyCount > 0) {
                if (!firstCell) board += ',';
                board += std::to_string(emptyCount);
                emptyCount = 0;
                firstCell = false;
            }
        };

        for (int col = 1; col <= 14; col++)
        {
            int loc = row * 16 + col;
            if (baseMailbox[loc] == -1)
            {
                emptyCount++;
            }
            else if (pos.board.pieceMailbox[loc] == NONE_PIECE)
            {
                emptyCount++;
            }
            else
            {
                flushEmpty();
                if (!firstCell) board += ',';
                // color char
                switch (pos.board.colorMailbox[loc]) {
                    case RED:    board += 'r'; break;
                    case BLUE:   board += 'b'; break;
                    case YELLOW: board += 'y'; break;
                    case GREEN:  board += 'g'; break;
                    default: break;
                }
                board += piece2char(pos.board.pieceMailbox[loc]);
                firstCell = false;
            }
        }
        flushEmpty();
    }

    // En passant is only written when someone actually has one, matching the
    // reference engine: with no such player the field is omitted entirely.
    std::string ep;
    bool anyEp = false;
    for (int c = 0; c < 4; c++) if (state.enpassants[c] != -1) anyEp = true;
    if (anyEp)
    {
        ep = "{'enPassant':(";
        for (int c = 0; c < 4; c++)
        {
            const int sq = state.enpassants[c];
            if (sq != -1)
                ep += std::string("'") + files[(sq % 16) - 1] + std::to_string(ranks[sq / 16]) + "'";
            else
                ep += "''";
            if (c != 3) ep += ",";
        }
        ep += ")}-";
    }

    return std::string(1, turnChar) + "-0,0,0,0-" + oo + "-" + ooo + "-0,0,0,0-"
         + std::to_string(state.halfmoveClock) + "-" + ep + board;
}

void print(Position &pos)
{
    enableAnsiColors();
    printf("     +---+---+---+---+---+---+---+---+---+---+---+---+---+---+\n");
    for (int rank = 14; rank >= 1; rank--)
    {
        printf("  %02d |", rank);
        for (int file = 1; file <= 14; file++)
        {
            int loc = (14 - rank) * 16 + file;
            if (pos.board.pieceMailbox[loc] != NONE_PIECE)
            {
                printf(colorToStringColor(pos.board.colorMailbox[loc]), piece2char(pos.board.pieceMailbox[loc]));
            }
            else
            {
                printf(" %c  ", ' ');
            }
        }
        printf("\r\n");
    }
    printf("     +---+---+---+---+---+---+---+---+---+---+---+---+---+---+\n");
    printf("       a   b   c   d   e   f   g   h   i   j   k   l   m   n\n");
    printf("\n\n");
    printf("Current turn: %s\n", pos.gameStates.back().curTurn == RED ? "Red" :
           pos.gameStates.back().curTurn == BLUE ? "Blue" :
           pos.gameStates.back().curTurn == YELLOW ? "Yellow" : "Green");
}
