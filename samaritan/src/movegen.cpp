/*
To add:
- Castling
- En passant
*/

#include "movegen.h"
#include <stdexcept>

// Per-colour pawn geometry, so the hot path is a table read rather than a
// four-way switch executed once per pawn. `useRow` picks whether a colour's
// ranks run along rows (RED/YELLOW) or columns (BLUE/GREEN).
struct PawnCfg
{
    int8_t o;          // index into offsets[]
    int8_t useRow;     // 1 -> compare loc/16, 0 -> compare loc%16
    int8_t startVal;   // value identifying the double-push rank
    int8_t promoVal;   // value identifying the rank before promotion
    int8_t epL;        // enpassants[] slot reachable via offsets[o][2]
    int8_t epR;        // enpassants[] slot reachable via offsets[o][1]
};
constexpr PawnCfg pawnCfg[5] = {
    { 0, 0,  0,  0, 0, 0 },
    { 1, 1, 12,  4, 1, 3 },   // RED    moves north
    { 2, 0,  2, 10, 2, 0 },   // BLUE   moves east
    { 3, 1,  1,  9, 3, 1 },   // YELLOW moves south
    { 4, 0, 13,  5, 0, 2 },   // GREEN  moves west
};

constexpr ExtMove *getPawnMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const auto enpassants = pos.gameStates.back().enpassants;
    const auto myColor = pos.board.colorMailbox[loc];
    const auto myTeam = getTeam(myColor);

    const PawnCfg &cfg = pawnCfg[ctz((unsigned int)myColor) + 1];
    const int offset = cfg.o;
    const int rank = cfg.useRow ? (loc / 16) : (loc % 16);
    const bool isOnStartRank  = (rank == cfg.startVal);
    const bool nextPromotion  = (rank == cfg.promoVal);

    // En passant is only real if the skipped square is still empty AND the pawn
    // that ran past it is still standing there. Without both guards a stale
    // en-passant square lets us "capture" whatever moved onto the victim square
    // (or land on a teammate), which corrupts the board on undo.
    const int victimSq = loc + offsets[offset][0];
    const bool victimIsEnemyPawn =
        !isInvalidLocation(victimSq)
        && pos.board.pieceMailbox[victimSq] == PAWN
        && !isOnTeam(pos.board.colorMailbox[victimSq], myTeam);

    const int epLeftSq  = loc + offsets[offset][2];
    const int epRightSq = loc + offsets[offset][1];
    const bool leftEnPassant  = victimIsEnemyPawn && enpassants[cfg.epL] == epLeftSq
                             && !isInvalidLocation(epLeftSq)  && pos.board.isEmpty(epLeftSq);
    const bool rightEnPassant = victimIsEnemyPawn && enpassants[cfg.epR] == epRightSq
                             && !isInvalidLocation(epRightSq) && pos.board.isEmpty(epRightSq);

    // Create move for forward pawn move
    if (!isInvalidLocation(loc + offsets[offset][0]) && pos.board.isEmpty(loc + offsets[offset][0]))
    {
        if (nextPromotion)
        {
            *moveList = Move(loc + offsets[offset][0], loc, QUEEN, 1);
            moveList->gen_type = QUEEN_PROMOTION;
            moveList->value = piece_values[QUEEN] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, ROOK, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[ROOK] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, BISHOP, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[BISHOP] - 1;
            moveList++;
            *moveList = Move(loc + offsets[offset][0], loc, KNIGHT, 1);
            moveList->gen_type = UNDERPROMOTION;
            moveList->value = piece_values[KNIGHT] - 1;
            moveList++;
        }
        else
        {
            *moveList = Move(loc + offsets[offset][0], loc, 0, 0);
            moveList->gen_type = QUIETS;
            moveList->value = 0;
            moveList++;

            if (isOnStartRank && pos.board.isEmpty(loc + 2 * offsets[offset][0]))
            {
                *moveList = Move(loc + 2 * offsets[offset][0], loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList->value = 0;
                moveList++;
            }
        }
    }

    // Create move for right diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][1]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][1]) && !isOnTeam(pos.board.colorMailbox[loc + offsets[offset][1]], myTeam))
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][1], loc, QUEEN, 1);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, ROOK, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, BISHOP, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, KNIGHT, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][1], loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][1]]] - piece_values[PAWN];
                moveList++;
            }
        }
        else if (rightEnPassant)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][1], loc, QUEEN, 3);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, ROOK, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, BISHOP, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][1], loc, KNIGHT, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][1], loc, 0, 2);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[PAWN] - piece_values[PAWN];
                moveList++;
            }
        }
    }

    // Create move for left diagonal capture
    if (!isInvalidLocation(loc + offsets[offset][2]))
    {
        if(!pos.board.isEmpty(loc + offsets[offset][2]) && !isOnTeam(pos.board.colorMailbox[loc + offsets[offset][2]], myTeam))
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][2], loc, QUEEN, 1);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, ROOK, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, BISHOP, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, KNIGHT, 1);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][2], loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[loc + offsets[offset][2]]] - piece_values[PAWN];
                moveList++;
            }
        }
        else if (leftEnPassant)
        {
            if (nextPromotion)
            {
                *moveList = Move(loc + offsets[offset][2], loc, QUEEN, 3);
                moveList->gen_type = QUEEN_PROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[QUEEN];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, ROOK, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[ROOK];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, BISHOP, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[BISHOP];
                moveList++;
                *moveList = Move(loc + offsets[offset][2], loc, KNIGHT, 3);
                moveList->gen_type = UNDERPROMOTION;
                moveList->value = 10*piece_values[PAWN] - 1 + piece_values[KNIGHT];
                moveList++;
            }
            else
            {
                *moveList = Move(loc + offsets[offset][2], loc, 0, 2);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[PAWN] - piece_values[PAWN];
                moveList++;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getKnightMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const auto myTeam = getTeam(pos.board.colorMailbox[loc]);
    // Check all possible knight moves using precomputed offsets
    const int moves[] = {loc + NORTH + NORTH + EAST, loc + NORTH + NORTH + WEST, loc + WEST + WEST + NORTH,
                         loc + WEST + WEST + SOUTH, loc + SOUTH + SOUTH + WEST, loc + SOUTH + SOUTH + EAST,
                         loc + EAST + EAST + SOUTH, loc + EAST + EAST + NORTH};

    for (int move : moves)
    {
        if (!isInvalidLocation(move))
        {
            if (pos.board.isEmpty(move))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList->value = 0;
                moveList++;
            }
            else if (!isOnTeam(pos.board.colorMailbox[move], myTeam))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[move]] - piece_values[KNIGHT];
                moveList++;
            }
        }
    }

    return moveList;
}

constexpr ExtMove *getStraightLineMoves(const Position &pos, const int loc, ExtMove *moveList, const int increment)
{
    const auto myTeam = getTeam(pos.board.colorMailbox[loc]);
    const auto myPiece = pos.board.pieceMailbox[loc];
    // Generate moves using a ray-casting approach
    for (int destination = loc + increment; !isInvalidLocation(destination); destination += increment)
    {
        if (!pos.board.isEmpty(destination) && isOnTeam(pos.board.colorMailbox[destination], myTeam))
        {
            break;
        }
        if (pos.board.pieceMailbox[destination] != NONE_PIECE)
        {
            *moveList = Move(destination, loc, 0, 0);
            moveList->gen_type = CAPTURES;
            moveList->value = 10*piece_values[pos.board.pieceMailbox[destination]] - piece_values[myPiece];
            moveList++;
            break;
        }
        else
        {
            *moveList = Move(destination, loc, 0, 0);
            moveList->gen_type = QUIETS;
            moveList->value = 0;
            moveList++;
        }
    }

    return moveList;
}

constexpr ExtMove *getBishopMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[6]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[6][offset]);
    }
    return moveList;
}

constexpr ExtMove *getRookMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[7]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[7][offset]);
    }
    return moveList;
}

constexpr ExtMove *getQueenMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    for (int offset = 0; offset < offsetsNum[8]; offset++)
    {
        moveList = getStraightLineMoves(pos, loc, moveList, offsets[8][offset]);
    }
    return moveList;
}

ExtMove *getKingMoves(const Position &pos, const int loc, ExtMove *moveList)
{
    const auto &state = pos.gameStates.back();
    const auto turn = state.curTurn;
    const auto myTeam = getTeam(turn);

    for (int offset = 0; offset < offsetsNum[9]; offset++)
    {
        int move = offsets[9][offset] + loc;
        if (!isInvalidLocation(move))
        {
            if (pos.board.isEmpty(move))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = QUIETS;
                moveList->value = 0;
                moveList++;
            }
            else if (!isOnTeam(pos.board.colorMailbox[move], myTeam))
            {
                *moveList = Move(move, loc, 0, 0);
                moveList->gen_type = CAPTURES;
                moveList->value = 10*piece_values[pos.board.pieceMailbox[move]] - piece_values[KING];
                moveList++;
            }
        }
    }

    // Castling
    if (pos.board.isSquareAttacked(loc, turn, myTeam))
    {
        return moveList; // Cannot castle if the king is in check
    }
    switch (turn)
    {
    case RED:
        if (state.castleRights & RED_OO)
        {
            if (pos.board.isEmpty(J14) && pos.board.isEmpty(K14))
            {
                if (!pos.board.isSquareAttacked(J14, turn, myTeam) && !pos.board.isSquareAttacked(K14, turn, myTeam))
                {
                    *moveList = Move(K14, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        if (state.castleRights & RED_OOO)
        {
            if (pos.board.isEmpty(H14) && pos.board.isEmpty(G14) && pos.board.isEmpty(F14))
            {
                if (!pos.board.isSquareAttacked(H14, turn, myTeam) && !pos.board.isSquareAttacked(G14, turn, myTeam))
                {
                    *moveList = Move(G14, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        break;
    case BLUE:
        if (state.castleRights & BLUE_OO)
        {
            if (pos.board.isEmpty(B7) && pos.board.isEmpty(B6) && pos.board.isEmpty(B5))
            {
                if (!pos.board.isSquareAttacked(B7, turn, myTeam) && !pos.board.isSquareAttacked(B6, turn, myTeam))
                {
                    *moveList = Move(B6, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        if (state.castleRights & BLUE_OOO)
        {
            if (pos.board.isEmpty(B9) && pos.board.isEmpty(B10))
            {
                if (!pos.board.isSquareAttacked(B9, turn, myTeam) && !pos.board.isSquareAttacked(B10, turn, myTeam))
                {
                    *moveList = Move(B10, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        break;
    case YELLOW:
        if (state.castleRights & YELLOW_OO)
        {
            if (pos.board.isEmpty(F1) && pos.board.isEmpty(G1))
            {
                if (!pos.board.isSquareAttacked(F1, turn, myTeam) && !pos.board.isSquareAttacked(G1, turn, myTeam))
                {
                    *moveList = Move(F1, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        if (state.castleRights & YELLOW_OOO)
        {
            if (pos.board.isEmpty(I1) && pos.board.isEmpty(J1) && pos.board.isEmpty(K1))
            {
                if (!pos.board.isSquareAttacked(I1, turn, myTeam) && !pos.board.isSquareAttacked(J1, turn, myTeam))
                {
                    *moveList = Move(J1, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        break;
    case GREEN:
        if (state.castleRights & GREEN_OO)
        {
            if (pos.board.isEmpty(O5) && pos.board.isEmpty(O6))
            {
                if (!pos.board.isSquareAttacked(O5, turn, myTeam) && !pos.board.isSquareAttacked(O6, turn, myTeam))
                {
                    *moveList = Move(O5, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        if (state.castleRights & GREEN_OOO)
        {
            if (pos.board.isEmpty(O8) && pos.board.isEmpty(O9) && pos.board.isEmpty(O10))
            {
                if (!pos.board.isSquareAttacked(O8, turn, myTeam) && !pos.board.isSquareAttacked(O9, turn, myTeam))
                {
                    *moveList = Move(O9, loc, 0, 4);
                    moveList->gen_type = CASTLING;
                    moveList->value = 0;
                    moveList++;
                }
            }
        }
        break;
    default:
        throw std::runtime_error("Unknown color given");
    }

    return moveList;
}

// ---------------------------------------------------------------------------
//  Check and pin detection
// ---------------------------------------------------------------------------

// True when a slider of type `pie` attacks along the ray `dir`.
constexpr bool slidesAlong(PieceType pie, int dir)
{
    const bool diagonal = (dir == -17 || dir == -15 || dir == 15 || dir == 17);
    if (pie == QUEEN)  return true;
    if (pie == BISHOP) return diagonal;
    if (pie == ROOK)   return !diagonal;
    return false;
}

// Fill `info` with everything move generation needs to decide legality without
// touching the board: who is giving check, and which of our pieces are pinned.
//
// The eight sliding rays are walked once each. The first occupant of a ray is
// either a checker (enemy slider aligned with the king) or a potential pin
// victim; in the latter case we keep walking to see whether an enemy slider
// stands behind it. Knight and pawn checks cannot be blocked, so they are found
// by looking backwards from the king instead of outwards.
void computeCheckInfo(const Position &pos, PieceColor us, CheckInfo &info)
{
    info = CheckInfo();
    info.valid = true;

    const int ksq = pos.board.kingTracker[ctz((unsigned int)us)];
    if (ksq < 0) return;                       // king already captured

    const PieceColor myTeam = getTeam(us);
    const Board &b = pos.board;

    auto addChecker = [&](int sq, int dir) {
        if (info.checkerCount == 0) { info.checkerSq = sq; info.checkerDir = dir; }
        if (info.checkerCount < 2) info.checkerCount++;
    };

    // --- sliding rays: checkers and pins in one pass ---
    for (int i = 0; i < offsetsNum[8]; ++i)
    {
        const int dir = offsets[8][i];

        int n = ksq + dir;
        while (!isInvalidLocation(n) && b.pieceMailbox[n] == NONE_PIECE) n += dir;
        if (isInvalidLocation(n)) continue;

        const PieceType firstPie = b.pieceMailbox[n];
        const PieceColor firstCol = b.colorMailbox[n];

        if (!isOnTeam(firstCol, myTeam))
        {
            // First occupant is an enemy: it checks us if it slides this way.
            if (slidesAlong(firstPie, dir)) addChecker(n, dir);
            continue;                          // an enemy blocker cannot be pinned by us
        }

        // First occupant is ours or our partner's. Only our own pieces are worth
        // recording, since we can only move those.
        const int blocker = n;
        n += dir;
        while (!isInvalidLocation(n) && b.pieceMailbox[n] == NONE_PIECE) n += dir;
        if (isInvalidLocation(n)) continue;

        if (!isOnTeam(b.colorMailbox[n], myTeam) && slidesAlong(b.pieceMailbox[n], dir)
            && b.colorMailbox[blocker] == us)
        {
            info.pinSq[info.pinCount]  = blocker;
            info.pinDir[info.pinCount] = dir;
            info.pinCount++;
        }
    }

    // --- knight checks ---
    for (int i = 0; i < offsetsNum[5]; ++i)
    {
        const int n = ksq + knightOffsets[i];
        if (isInvalidLocation(n)) continue;
        if (b.pieceMailbox[n] == KNIGHT && !isOnTeam(b.colorMailbox[n], myTeam))
            addChecker(n, 0);
    }

    // --- pawn checks ---
    // An enemy pawn of colour e checks us when it sits on one of the squares
    // from which its own capture offsets land on our king.
    for (int c = 0; c < 4; ++c)
    {
        const PieceColor e = static_cast<PieceColor>(1 << c);
        if (getTeam(e) == myTeam) continue;
        for (int k = 1; k <= 2; ++k)
        {
            const int n = ksq - offsets[c + 1][k];
            if (isInvalidLocation(n)) continue;
            if (b.pieceMailbox[n] == PAWN && b.colorMailbox[n] == e)
                addChecker(n, 0);
        }
    }
}

bool inCheck(const Position &pos, PieceColor color)
{
    const int ksq = pos.board.kingTracker[ctz(color)];
    // No king to be in check: that colour has already been captured out of the
    // game. Guarded rather than assumed, because the attack scan indexes tables
    // by square and -1 would walk off the front of them.
    if (ksq < 0) return false;
    // Return true if the king is in check.
    return pos.board.isSquareAttacked(ksq, color, getTeam(color));
}

// Is `to` reachable from `ksq` along `dir` without passing a blocker?
// `from` is skipped because that is the moving piece's own square.
static inline bool onPinRay(const Position &pos, int ksq, int dir, int from, int to)
{
    for (int n = ksq + dir; !isInvalidLocation(n); n += dir)
    {
        if (n == to) return true;
        if (n != from && pos.board.pieceMailbox[n] != NONE_PIECE) return false;
    }
    return false;
}

// Does `to` interpose between our king and the single checking slider?
static inline bool blocksCheck(int ksq, const CheckInfo &info, int to)
{
    if (info.checkerDir == 0) return false;          // knight or pawn: cannot be blocked
    for (int n = ksq + info.checkerDir; n != info.checkerSq; n += info.checkerDir)
        if (n == to) return true;
    return false;
}

// Legality without touching the board, except for en passant, which is rare
// enough that make/unmake remains the cheapest correct answer.
static inline bool isLegalMove(Position &pos, const ExtMove &m, const CheckInfo &info,
                               int ksq, PieceColor us)
{
    const int from = m.from();
    const int to   = m.to();
    const int special = m.special_move();

    // Capturing an enemy king wins the game for our team instantly, so it is
    // legal no matter what: while in check, in double check, and from a pinned
    // piece. This must be tested before any of those.
    if (pos.board.pieceMailbox[to] == KING) return true;

    if (special == 2 || special == 3)            // en passant (and ep-promotion)
    {
        pos.move(m);
        const bool ok = !inCheck(pos, us);
        pos.undoMove(m);
        return ok;
    }

    if (from == ksq)
    {
        // The king must not shadow the ray it is trying to step out of, so it is
        // lifted off the board before the destination is tested.
        Board &b = pos.board;
        b.pieceMailbox[ksq] = NONE_PIECE;
        b.colorMailbox[ksq] = NONE_COLOR;
        const bool attacked = b.isSquareAttacked(to, us, getTeam(us));
        b.pieceMailbox[ksq] = KING;
        b.colorMailbox[ksq] = us;
        return !attacked;
    }

    // Any non-king move while two pieces give check is hopeless.
    if (info.checkerCount >= 2) return false;

    if (info.checkerCount == 1 && to != info.checkerSq && !blocksCheck(ksq, info, to))
        return false;

    const int pin = info.pinRayOf(from);
    if (pin && !onPinRay(pos, ksq, pin, from, to)) return false;

    return true;
}

ExtMove *generate(Position &pos, ExtMove *moveList)
{
    const auto curTurn = pos.gameStates.back().curTurn;

    // A captured king ends the game immediately for both teams, so any position
    // missing a king is terminal and has no legal moves at all.
    for (int c = 0; c < 4; ++c)
        if (pos.board.kingTracker[c] < 0) return moveList;

    CheckInfo &info = pos.gameStates.back().checkInfo;
    computeCheckInfo(pos, curTurn, info);

    const int ksq = pos.board.kingTracker[ctz((unsigned int)curTurn)];

    // Generate straight into the caller's buffer; no second staging array.
    ExtMove *end = moveList;
    const int me = ctz((unsigned int)curTurn);
    const int *myPieces = pos.board.pieceList[me];
    const int myCount = pos.board.pieceCount[me];
    for (int i = 0; i < myCount; ++i)
    {
        const int loc = myPieces[i];
        switch (pos.board.pieceMailbox[loc])
        {
        case PAWN:   end = getPawnMoves(pos, loc, end);   break;
        case KNIGHT: end = getKnightMoves(pos, loc, end); break;
        case BISHOP: end = getBishopMoves(pos, loc, end); break;
        case ROOK:   end = getRookMoves(pos, loc, end);   break;
        case QUEEN:  end = getQueenMoves(pos, loc, end);  break;
        case KING:   end = getKingMoves(pos, loc, end);   break;
        default: break;
        }
    }

    // Filter in place: overwrite an illegal move with the last one and shrink.
    ExtMove *cur = moveList;
    while (cur != end)
    {
        if (isLegalMove(pos, *cur, info, ksq, curTurn)) cur++;
        else *cur = *(--end);
    }
    return end;
}
MoveList::MoveList(Position& pos) : last(generate(pos, moveList)) {}

void MoveList::print() const
{
    for (auto move : moveList)
    {
    }
}
