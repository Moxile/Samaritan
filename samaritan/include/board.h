#pragma once

// The board itself: piece placement, the derived piece list, and the attack
// scan. Split out from position.h so that the evaluator can operate on a Board
// without position.h having to include the evaluator back.

#include <cstring>
#include <cstdlib>

#include "chess.h"

class Board
{
public:
    alignas(64) PieceType pieceMailbox[224];
    PieceColor colorMailbox[224];
    int kingTracker[4] = {-1, -1, -1, -1};
    int nonPawnPieceCount[4] = {0, 0, 0, 0};

    // Occupied squares per colour, so move generation iterates ~16 entries
    // instead of scanning all 224 mailbox squares. pieceIdx maps a square back
    // to its slot, which makes removal an O(1) swap-and-pop.
    int pieceList[4][32] = {};
    int pieceCount[4] = {0, 0, 0, 0};
    int pieceIdx[224] = {};

    Board()
    {
        std::memset(colorMailbox, NONE_COLOR, sizeof(colorMailbox));
        std::memset(pieceMailbox, NONE_PIECE, sizeof(pieceMailbox));
    }

    inline void listAdd(int sq, PieceColor col)
    {
        const int c = ctz((unsigned int)col);
        pieceIdx[sq] = pieceCount[c];
        pieceList[c][pieceCount[c]++] = sq;
    }

    inline void listRemove(int sq, PieceColor col)
    {
        const int c = ctz((unsigned int)col);
        const int i = pieceIdx[sq];
        const int last = pieceList[c][--pieceCount[c]];
        pieceList[c][i] = last;
        pieceIdx[last] = i;
    }

    inline void listMove(int from, int to, PieceColor col)
    {
        const int c = ctz((unsigned int)col);
        const int i = pieceIdx[from];
        pieceList[c][i] = to;
        pieceIdx[to] = i;
    }

    // Rebuild from the mailboxes. Used after FEN loading and by test helpers
    // that populate the board directly.
    void rebuildPieceList()
    {
        for (int c = 0; c < 4; ++c) pieceCount[c] = 0;
        for (int sq = 0; sq < 224; ++sq)
            if (pieceMailbox[sq] != NONE_PIECE && colorMailbox[sq] != NONE_COLOR)
                listAdd(sq, colorMailbox[sq]);
    }

    // Debug helper: does the incremental list still agree with the mailboxes?
    bool pieceListConsistent() const
    {
        int seen[4] = {0, 0, 0, 0};
        for (int sq = 0; sq < 224; ++sq)
            if (pieceMailbox[sq] != NONE_PIECE && colorMailbox[sq] != NONE_COLOR)
                seen[ctz((unsigned int)colorMailbox[sq])]++;
        for (int c = 0; c < 4; ++c)
        {
            if (seen[c] != pieceCount[c]) return false;
            for (int i = 0; i < pieceCount[c]; ++i)
            {
                const int sq = pieceList[c][i];
                if (pieceMailbox[sq] == NONE_PIECE) return false;
                if (ctz((unsigned int)colorMailbox[sq]) != c) return false;
                if (pieceIdx[sq] != i) return false;
            }
        }
        return true;
    }

    constexpr bool isEmpty(int loc) const
    {
        return pieceMailbox[loc] == NONE_PIECE;
    }

    // Is `square` attacked by anyone not on `color`'s team?
    //
    // Structured as three cheap phases instead of one generic 16-direction ray
    // walk: knights are direct probes, and king/pawn attacks can only come from
    // an adjacent square, so they are folded into the first step of each ray.
    // `baseColor` is accepted for call-site compatibility but is redundant --
    // a friendly piece blocks a ray either way, since the walk stops at the
    // first occupied square regardless of its colour.
    bool isSquareAttacked(int square, PieceColor color, PieceColor /*baseColor*/ = NONE_COLOR) const
    {
        const PieceColor myTeam = getTeam(color);

        // --- knights: eight direct probes, no walking ---
        for (int i = 0; i < 8; ++i)
        {
            const int n = square + knightOffsets[i];
            if (isInvalidLocation(n)) continue;
            if (pieceMailbox[n] == KNIGHT && !isOnTeam(colorMailbox[n], myTeam))
                return true;
        }

        // --- eight rays: sliders, plus king and pawn on the first step only ---
        for (int i = 0; i < 8; ++i)
        {
            const int dir = kingOffsets[i];
            const bool diag = (dir == -17 || dir == -15 || dir == 15 || dir == 17);
            const PieceType slider = diag ? BISHOP : ROOK;

            const int endSq = rayEndTable[square][i];
            if (endSq == square) continue;          // no room in this direction

            int n = square + dir;
            PieceType pie = pieceMailbox[n];
            if (pie != NONE_PIECE)
            {
                const PieceColor col = colorMailbox[n];
                if (!isOnTeam(col, myTeam))
                {
                    if (pie == QUEEN || pie == slider || pie == KING) return true;
                    if (pie == PAWN)
                    {
                        // A pawn on `n` attacks `square` if one of its own two
                        // capture offsets lands there.
                        const int c = ctz((unsigned int)col) + 1;
                        if (n + offsets[c][1] == square || n + offsets[c][2] == square)
                            return true;
                    }
                }
                continue;   // ray is blocked at the first square
            }

            while (n != endSq)
            {
                n += dir;
                pie = pieceMailbox[n];
                if (pie == NONE_PIECE) continue;
                if (!isOnTeam(colorMailbox[n], myTeam) && (pie == QUEEN || pie == slider))
                    return true;
                break;
            }
        }
        return false;
    }
};

// Check and pin state for the side to move, derived from the board and cached
// per position. Computing this once per node is what lets move generation skip
// the make/unmake legality test on the overwhelming majority of moves.
//
// A king has exactly 8 sliding rays, so at most 8 of our pieces can be pinned
// and a compact list beats any per-square table we would have to copy or clear.
struct CheckInfo
{
    int  checkerSq   = -1;   // square of the checker when exactly one gives check
    int  checkerDir  = 0;    // ray direction ksq -> checkerSq, 0 if not a slider
    int  checkerCount = 0;   // 0, 1, or 2 (2 meaning "two or more" -> king must move)
    int  pinSq[8]    = {};   // our own pieces that are pinned to the king
    int  pinDir[8]   = {};   // the ray each pinned piece is stuck on (both signs legal)
    int  pinCount    = 0;
    bool valid       = false;

    // Returns the pin ray for a piece, or 0 when it is free to move anywhere.
    constexpr int pinRayOf(int sq) const
    {
        for (int i = 0; i < pinCount; ++i)
            if (pinSq[i] == sq) return pinDir[i];
        return 0;
    }
};
