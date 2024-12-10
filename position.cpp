#include <ranges>
#include <cstring>

#include "position.h"

Position::Position()
{
    resetBoard();
    setHash();
}

void Position::setHash()
{
    std::srand(958829);
    for (int color = 0; color < 4; color++)
    {
        turn_hashes_[color] = rand64();
    }
    for (int color = 0; color < 4; color++)
    {
        for (int piece_type = 0; piece_type < 6; piece_type++)
        {
            for (int loc = 0; loc < 224; loc++)
            {
                piece_hashes_[color][piece_type][loc] = rand64();
            }
        }
    }
}

void Position::initHash()
{
    for(int loc  = 0; loc < 224; loc++)
    {
        if(pieceMailbox[loc] != NONE_PIECE)
        {
        UpdatePieceHash(colorMailbox[loc], pieceMailbox[loc], loc);
        }
    }
    UpdateTurnHash(curTurn);
}

void Position::resetBoard()
{
    curTurn = NONE_COLOR;
    posScore = 0;
    kingTracker[3] = kingTracker[2] = kingTracker[1] = kingTracker[0] = -1;
    castleRights = NO_CASTLING;
    enpassants[3] = enpassants[2] = enpassants[1] = enpassants[0] = -1;

    std::memset(colorMailbox, NONE_COLOR, sizeof(colorMailbox));
    std::memset(pieceMailbox, NONE_PIECE, sizeof(pieceMailbox));
}

const void Position::loadFEN(const std::string fen)
{
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
            fen_setPlayerToMove(part);
            break;
        case 2:
            break;
        case 3:
            fen_setCastlingRights(part, RED_OO);
            break;
        case 4:
            fen_setCastlingRights(part, RED_OOO);
            break;
        case 5:
            break;
        case 6:
            break;
        case 7:
            fen_setBoard(part);
            break;
        default:
            break;
        }
        part_counter++;
    }

    initHash();
}

const void Position::fen_setPlayerToMove(const std::string color)
{
    switch (color.at(0))
    {
    case 'R':
        curTurn = PieceColor::RED;
        break;
    case 'B':
        curTurn = PieceColor::BLUE;
        break;
    case 'Y':
        curTurn = PieceColor::YELLOW;
        break;
    case 'G':
        curTurn = PieceColor::GREEN;
        break;
    default:
        break;
    }
}

const void Position::fen_setCastlingRights(const std::string castling, const CastlingRights r)
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
            castleRights |= color;
        }
        color <<= 2;
    }
}

const void Position::fen_setBoard(const std::string boardFEN)
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
                    colorMailbox[loc] = RED;
                    break;
                case 'b':
                    colorMailbox[loc] = BLUE;
                    break;
                case 'y':
                    colorMailbox[loc] = YELLOW;
                    break;
                case 'g':
                    colorMailbox[loc] = GREEN;
                    break;
                default:
                    break;
                }

                switch (piece.at(1))
                {
                case 'P':
                    pieceMailbox[loc] = PieceType::PAWN;
                    break;
                case 'N':
                    pieceMailbox[loc] = PieceType::KNIGHT;
                    break;
                case 'B':
                    pieceMailbox[loc] = PieceType::BISHOP;
                    break;
                case 'R':
                    pieceMailbox[loc] = PieceType::ROOK;
                    break;
                case 'Q':
                    pieceMailbox[loc] = PieceType::QUEEN;
                    break;
                case 'K':
                    pieceMailbox[loc] = PieceType::KING;
                    kingTracker[__builtin_ctz((unsigned int)(colorMailbox[loc]))] = loc;
                    break;
                default:
                    break;
                }

                material_value[__builtin_ctz((unsigned int)(colorMailbox[loc]))] += piece_weights[pieceMailbox[loc] - 1];

                col_num++;
            }
        }

        row_num++;
    }
}

bool Position::isSquareAttacked(int square, PieceColor color, PieceColor baseColor) const
{
    for (int start = 0; start < offsetsNum[10]; ++start)
    {
        int increment = offsets[10][start];
        for (int n = square + increment;; n += increment)
        {
            if (isInvalidLocation(n))
            {
                break;
            }
            PieceType pie = pieceMailbox[n];
            PieceColor col = colorMailbox[n];
            if (baseColor != NONE_COLOR && pie != NONE_PIECE && getTeam(col) == baseColor)
            {
                break;
            }
            if (pie != NONE_PIECE)
            {
                if (isEnemy(n, color))
                {
                    if ((abs(increment) == 16 || abs(increment) == 1) && (pie == ROOK || pie == QUEEN || (pie == KING && n == square + increment)))
                    {
                        return true;
                    }
                    if (pie == PAWN && n == square + increment)
                    {
                        if (colorMailbox[n] == RED && (increment == 15 || increment == 17))
                            return true;
                        else if (colorMailbox[n] == BLUE && (increment == 15 || increment == -17))
                            return true;
                        else if (colorMailbox[n] == YELLOW && (increment == -15 || increment == -17))
                            return true;
                        else if (colorMailbox[n] == GREEN && (increment == -15 || increment == 17))
                            return true;
                    }
                    if ((abs(increment) == 15 || abs(increment) == 17) && (pie == BISHOP || pie == QUEEN || (pie == KING && n == square + increment)))
                    {
                        return true;
                    }
                    if ((abs(increment) == 18 || abs(increment) == 14 || abs(increment) == 31 || abs(increment) == 33) && pie == KNIGHT)
                    {
                        return true;
                    }
                }
                break;
            }
            if ((abs(increment) == 18 || abs(increment) == 14 || abs(increment) == 31 || abs(increment) == 33))
            {
                break;
            }
        }
    }
    return false;
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

void Position::print() const
{
    printf("     +---+---+---+---+---+---+---+---+---+---+---+---+---+---+\n");
    for (int rank = 14; rank >= 1; rank--)
    {
        printf("  %02d |", rank);
        for (int file = 1; file <= 14; file++)
        {
            int loc = (14 - rank) * 16 + file;
            if (pieceMailbox[loc] != NONE_PIECE)
            {
                printf(colorToStringColor(colorMailbox[loc]), piece2char(pieceMailbox[loc]));
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
}