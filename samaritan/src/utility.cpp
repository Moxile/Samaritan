#include "utility.h"
#include "chess.h"

const void loadFEN(Position &pos, const std::string fen)
{
    pos.gameStates.clear();
    GameState initialState;
    initialState.zobristKey = 0;
    auto parts = fen | std::views::split('-') | std::views::transform([](auto v)
                                                                      {
                    auto c = v | std::views::common;
                    return std::string(c.begin(), c.end()); });

    int part_counter = 1;
    for (auto part : parts)
    {
        switch (part_counter)
        {
        case 1:
            fen_setPlayerToMove(initialState, part);
            initialState.zobristKey ^= zobristTurn[initialState.curTurn];
            break;
        case 2:
            break;
        case 3:
            fen_setCastlingRights(initialState, part, RED_OO);
            break;
        case 4:
            fen_setCastlingRights(initialState, part, RED_OOO);
            break;
        case 5:
            break;
        case 6:
            break;
        case 7:
            fen_setBoard(pos.board, part);
            break;
        default:
            break;
        }
        part_counter++;
    }

    // Set Zobrist Key
    for (int s = 0; s < 256; s++) {
        Square sq = static_cast<Square>(s);
        if (pos.board.pieceType(sq) == NONE_PIECE) continue;
        int idx = board_table[sq];
        int piece = pos.board.pieceType(sq);
        int color = pos.board.pieceColor(sq);
        initialState.zobristKey ^= zobristPieces[idx][piece][color];
    }

    initialState.zobristKey ^= zobristTurn[initialState.curTurn];

    int rights = initialState.castleRights;
    for (int i = 0; i < 8; i++)
        if (rights & (1 << i))
            initialState.zobristKey ^= zobristCastle[i];
    for (int player = 0; player < 4; player++) {
        int ep = initialState.enpassants[player];
        if (ep != -1)
            initialState.zobristKey ^= zobristEnPassant[player][board_table[ep]];
    }

    // init accumulator
    pos.refreshNNUE();
    pos.nnue.init_eval(initialState.curTurn);

    pos.gameStates.push_back(initialState);
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
                int loc = 16 * row_num + col_num;
                PieceColor c = NONE_COLOR;
                PieceType t = NONE_PIECE;

                switch (piece.at(0))
                {
                case 'r':
                    c = RED;
                    break;
                case 'b':
                    c = BLUE;
                    break;
                case 'y':
                    c = YELLOW;
                    break;
                case 'g':
                    c = GREEN;
                    break;
                default:
                    break;
                }

                switch (piece.at(1))
                {
                case 'P':
                    t = PieceType::PAWN;
                    break;
                case 'N':
                    t = PieceType::KNIGHT;
                    board.nonPawnPieceCount[c]++;
                    break;
                case 'B':
                    t = PieceType::BISHOP;
                    board.nonPawnPieceCount[c]++;
                    break;
                case 'R':
                    t = PieceType::ROOK;
                    board.nonPawnPieceCount[c]++;
                    break;
                case 'Q':
                    t = PieceType::QUEEN;
                    board.nonPawnPieceCount[c]++;
                    break;
                case 'K':
                    t = PieceType::KING;
                    board.nonPawnPieceCount[c]++;
                    board.kingTracker[c] = static_cast<Square>(loc);
                    break;
                default:
                    break;
                }

                board.mailbox[loc] = makePiece(t, c);

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
            Square loc = static_cast<Square>(row * 16 + col);
            if (baseMailbox[loc] == -1)
            {
                emptyCount++;
            }
            else if (pos.board.pieceType(loc)== NONE_PIECE)
            {
                emptyCount++;
            }
            else
            {
                flushEmpty();
                if (!firstCell) board += ',';
                // color char
                switch (pos.board.pieceColor(loc)) {
                    case RED:    board += 'r'; break;
                    case BLUE:   board += 'b'; break;
                    case YELLOW: board += 'y'; break;
                    case GREEN:  board += 'g'; break;
                    default: break;
                }
                board += piece2char(pos.board.pieceType(loc));
                firstCell = false;
            }
        }
        flushEmpty();
    }

    return std::string(1, turnChar) + "-0,0,0,0-" + oo + "-" + ooo + "-0,0,0,0-0-" + board;
}

void print(Position &pos)
{
    printf("     +---+---+---+---+---+---+---+---+---+---+---+---+---+---+\n");
    for (int rank = 14; rank >= 1; rank--)
    {
        printf("  %02d |", rank);
        for (int file = 1; file <= 14; file++)
        {
            Square loc = static_cast<Square>((14 - rank) * 16 + file);
            if (pos.board.pieceType(loc) != NONE_PIECE)
            {
                printf(colorToStringColor(pos.board.pieceColor(loc)), piece2char(pos.board.pieceType(loc)));
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