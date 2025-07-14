#include "utility.h"

const void loadFEN(Position &pos, const std::string fen)
{
    pos.gameStates.clear();
    GameState initialState;
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
                switch (piece.at(0))
                {
                case 'r':
                    board.colorMailbox[loc] = RED;
                    break;
                case 'b':
                    board.colorMailbox[loc] = BLUE;
                    break;
                case 'y':
                    board.colorMailbox[loc] = YELLOW;
                    break;
                case 'g':
                    board.colorMailbox[loc] = GREEN;
                    break;
                default:
                    break;
                }

                switch (piece.at(1))
                {
                case 'P':
                    board.pieceMailbox[loc] = PieceType::PAWN;
                    break;
                case 'N':
                    board.pieceMailbox[loc] = PieceType::KNIGHT;
                    break;
                case 'B':
                    board.pieceMailbox[loc] = PieceType::BISHOP;
                    break;
                case 'R':
                    board.pieceMailbox[loc] = PieceType::ROOK;
                    break;
                case 'Q':
                    board.pieceMailbox[loc] = PieceType::QUEEN;
                    break;
                case 'K':
                    board.pieceMailbox[loc] = PieceType::KING;
                    board.kingTracker[__builtin_ctz((unsigned int)(board.colorMailbox[loc]))] = loc;
                    break;
                default:
                    break;
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

void print(Position &pos)
{
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