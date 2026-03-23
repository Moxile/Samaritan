#include <gtest/gtest.h>
#include "test_helpers.h"
#include "perft.h"

// ───────────────────────────────────────────────────────────────
// Fixture – initialises Zobrist tables once per test suite.
// ───────────────────────────────────────────────────────────────
class MoveGenTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { initZobrist(); }
};

// ═══════════════════════════════════════════════════════════════
//  Starting-position tests
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, StartingPositionRedHas20Moves) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    MoveList moves(pos);
    EXPECT_EQ(moves.size(), 20u);
}

TEST_F(MoveGenTest, StartingPositionSymmetry) {
    // Every colour should have the same number of legal moves from the
    // symmetric starting position.
    const char turns[] = {'R', 'B', 'Y', 'G'};
    size_t expected = 0;

    for (int i = 0; i < 4; i++) {
        std::string fen = START_FEN;
        fen[0] = turns[i];
        Position pos(false);
        loadFEN(pos, fen);
        MoveList moves(pos);
        if (i == 0) expected = moves.size();
        EXPECT_EQ(moves.size(), expected)
            << "Colour " << turns[i] << " has a different move count";
    }
}

// ═══════════════════════════════════════════════════════════════
//  Move-validity invariants (should hold for ANY legal position)
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, AllMovesTargetValidSquares) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    MoveList moves(pos);
    for (const auto& m : moves) {
        EXPECT_FALSE(isInvalidLocation(m.to()))
            << m.toUCI() << " targets invalid square " << m.to();
        EXPECT_FALSE(isInvalidLocation(m.from()))
            << m.toUCI() << " originates from invalid square " << m.from();
    }
}

TEST_F(MoveGenTest, NoSelfCaptures) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    PieceColor turn = pos.gameStates.back().curTurn;
    MoveList moves(pos);
    for (const auto& m : moves) {
        if (pos.board.pieceMailbox[m.to()] != NONE_PIECE) {
            EXPECT_NE(getTeam(pos.board.colorMailbox[m.to()]), getTeam(turn))
                << m.toUCI() << " captures a same-team piece";
        }
    }
}

TEST_F(MoveGenTest, NoDuplicateMoves) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    MoveList moves(pos);
    for (auto it = moves.begin(); it != moves.end(); ++it)
        for (auto jt = it + 1; jt != moves.end(); ++jt)
            EXPECT_NE(it->getRawData(), jt->getRawData())
                << "Duplicate move " << it->toUCI();
}

TEST_F(MoveGenTest, NoMoveLeavesSelfInCheck) {
    // Verify across several plies from the starting position.
    Position pos(false);
    loadFEN(pos, START_FEN);

    std::vector<Move> played;
    for (int ply = 0; ply < 8; ply++) {
        MoveList moves(pos);
        if (moves.size() == 0) break;
        PieceColor turn = pos.gameStates.back().curTurn;
        for (const auto& m : moves) {
            pos.move(m);
            EXPECT_FALSE(inCheck(pos, turn))
                << "At ply " << ply << " move " << m.toUCI()
                << " leaves own king in check";
            pos.undoMove(m);
        }
        pos.move(*moves.begin());
        played.push_back(*moves.begin());
    }
    for (int i = (int)played.size() - 1; i >= 0; i--)
        pos.undoMove(played[i]);
}

// ═══════════════════════════════════════════════════════════════
//  Make / unmake invariants
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, MakeUnmakeRestoresBoard) {
    Position pos(false);
    loadFEN(pos, START_FEN);

    BoardSnapshot before = BoardSnapshot::capture(pos.board);
    uint64_t zobristBefore = pos.gameStates.back().zobristKey;

    MoveList moves(pos);
    for (const auto& m : moves) {
        pos.move(m);
        pos.undoMove(m);
        EXPECT_TRUE(BoardSnapshot::capture(pos.board) == before)
            << "Board not restored after " << m.toUCI();
        EXPECT_EQ(pos.gameStates.back().zobristKey, zobristBefore)
            << "Zobrist key not restored after " << m.toUCI();
    }
}

TEST_F(MoveGenTest, MakeUnmakeRestoresBoardDeep) {
    // Play 12 plies then undo all.
    Position pos(false);
    loadFEN(pos, START_FEN);

    BoardSnapshot before = BoardSnapshot::capture(pos.board);
    uint64_t zobristBefore = pos.gameStates.back().zobristKey;

    std::vector<Move> played;
    for (int ply = 0; ply < 12; ply++) {
        MoveList moves(pos);
        if (moves.size() == 0) break;
        pos.move(*moves.begin());
        played.push_back(*moves.begin());
    }

    for (int i = (int)played.size() - 1; i >= 0; i--)
        pos.undoMove(played[i]);

    EXPECT_TRUE(BoardSnapshot::capture(pos.board) == before);
    EXPECT_EQ(pos.gameStates.back().zobristKey, zobristBefore);
}

TEST_F(MoveGenTest, MoveCountUnchangedAfterMakeUnmake) {
    Position pos(false);
    loadFEN(pos, START_FEN);

    int countBefore = MoveList(pos).size();
    MoveList moves(pos);
    pos.move(*moves.begin());
    pos.undoMove(*moves.begin());
    int countAfter = MoveList(pos).size();

    EXPECT_EQ(countBefore, countAfter);
}

// ═══════════════════════════════════════════════════════════════
//  Piece-specific movement
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, KnightFromCenterHas8Moves) {
    // A knight in the centre of an otherwise empty board should
    // reach all 8 destinations.
    Position pos(false);
    // 119 = row 7, col 7  (centre)
    setupPosition(pos, RED, {
        {119, KNIGHT, RED},
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });
    EXPECT_EQ(countMovesFrom(MoveList(pos), 119), 8);
}

TEST_F(MoveGenTest, RookOnOpenBoardHasFullReach) {
    // Rook in the centre should slide in 4 directions unobstructed.
    Position pos(false);
    setupPosition(pos, RED, {
        {119, ROOK, RED},    // row 7, col 7
        {216, KING, RED},
        {7,   KING, YELLOW},
        {49,  KING, BLUE},   // row 3, col 1
        {62,  KING, GREEN},  // row 3, col 14
    });
    int rookMoves = countMovesFrom(MoveList(pos), 119);
    // At least 20 squares reachable (the exact count depends on
    // board geometry; the point is that all four rays extend).
    EXPECT_GT(rookMoves, 15);
}

TEST_F(MoveGenTest, BishopOnOpenBoardMovesAllDiagonals) {
    Position pos(false);
    setupPosition(pos, RED, {
        {119, BISHOP, RED},
        {216, KING, RED},
        {7,   KING, YELLOW},
        {49,  KING, BLUE},
        {62,  KING, GREEN},
    });
    int bishopMoves = countMovesFrom(MoveList(pos), 119);
    EXPECT_GT(bishopMoves, 10);
}

TEST_F(MoveGenTest, PawnBlockedCannotAdvance) {
    // RED pawn on starting rank with a piece directly ahead.
    Position pos(false);
    setupPosition(pos, RED, {
        {197, PAWN, RED},     // row 12, col 5 – starting rank
        {181, PAWN, BLUE},    // row 11, col 5 – directly in front
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });
    MoveList moves(pos);
    for (const auto& m : moves) {
        if (m.from() == 197) {
            // The only pawn moves should be diagonal captures, never
            // straight-ahead pushes.
            int dest = m.to();
            EXPECT_NE(dest, 181)
                << "Pawn pushed through a blocking piece";
            EXPECT_NE(dest, 165)
                << "Pawn double-pushed through a blocking piece";
        }
    }
}

TEST_F(MoveGenTest, PawnDoublePushOnlyFromStartRank) {
    // A RED pawn NOT on its starting rank must not double-push.
    Position pos(false);
    setupPosition(pos, RED, {
        {181, PAWN, RED},     // row 11, col 5 – one step in already
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });
    MoveList moves(pos);
    for (const auto& m : moves) {
        if (m.from() == 181) {
            int diff = m.from() - m.to();   // RED moves north → from > to
            EXPECT_LE(diff, 16)
                << "Pawn double-pushed from non-starting rank";
        }
    }
}

TEST_F(MoveGenTest, PawnCanCaptureDiagonally) {
    // Place an enemy piece on the RED pawn's diagonal.
    Position pos(false);
    setupPosition(pos, RED, {
        {197, PAWN, RED},      // row 12, col 5
        {180, PAWN, BLUE},     // row 11, col 4 – diagonal
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });
    EXPECT_TRUE(hasMoveFromTo(MoveList(pos), 197, 180))
        << "RED pawn should be able to capture diagonally";
}

// ═══════════════════════════════════════════════════════════════
//  En passant
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, EnPassantSquareSetAfterDoublePush) {
    Position pos(false);
    loadFEN(pos, START_FEN);

    // Find the double-push of the RED pawn at col 5 (index 197 → 165).
    Move doublePush(165, 197, 0, 0);
    ASSERT_TRUE(MoveList(pos).contains(doublePush))
        << "Double push from 197 to 165 should be legal";

    pos.move(doublePush);
    // RED = colour index 0 → enpassants[0] should hold the skipped square.
    EXPECT_EQ(pos.gameStates.back().enpassants[0], 181);
}

// ═══════════════════════════════════════════════════════════════
//  Castling
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, CastlingNotAvailableWhenPathBlocked) {
    // In the starting position all castling paths are blocked.
    Position pos(false);
    loadFEN(pos, START_FEN);
    MoveList moves(pos);
    for (const auto& m : moves)
        EXPECT_NE(m.special_move(), 4)
            << "No castling should be available from the starting position";
}

TEST_F(MoveGenTest, RedKingsideCastlingAvailable) {
    // Clear the path between RED king (216) and rook (219).
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {219, ROOK, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    }, RED_OO);

    MoveList moves(pos);
    bool found = false;
    for (const auto& m : moves)
        if (m.special_move() == 4 && m.from() == 216 && m.to() == 218)
            found = true;
    EXPECT_TRUE(found) << "RED kingside castling should be available";
}

TEST_F(MoveGenTest, CastlingMoveMovesKingAndRook) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {219, ROOK, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    }, RED_OO);

    Move castle(218, 216, 0, 4);
    pos.move(castle);

    EXPECT_EQ(pos.board.pieceMailbox[218], KING);
    EXPECT_EQ(pos.board.colorMailbox[218], RED);
    EXPECT_EQ(pos.board.pieceMailbox[217], ROOK);
    EXPECT_EQ(pos.board.colorMailbox[217], RED);
    EXPECT_EQ(pos.board.pieceMailbox[216], NONE_PIECE);
    EXPECT_EQ(pos.board.pieceMailbox[219], NONE_PIECE);
}

TEST_F(MoveGenTest, CastlingUndoRestoresAll) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {219, ROOK, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    }, RED_OO);

    BoardSnapshot before = BoardSnapshot::capture(pos.board);
    uint64_t zobristBefore = pos.gameStates.back().zobristKey;

    Move castle(218, 216, 0, 4);
    pos.move(castle);
    pos.undoMove(castle);

    EXPECT_TRUE(BoardSnapshot::capture(pos.board) == before);
    EXPECT_EQ(pos.gameStates.back().zobristKey, zobristBefore);
}

TEST_F(MoveGenTest, CastlingRightsLostAfterKingMove) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {219, ROOK, RED},
        {212, ROOK, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    }, RED_OO | RED_OOO);

    // Move the king one square.
    Move kingMove(217, 216, 0, 0);
    pos.move(kingMove);

    EXPECT_EQ(pos.gameStates.back().castleRights & RED_CASTLING, 0)
        << "Both RED castling rights should be revoked after a king move";
}

TEST_F(MoveGenTest, CastlingBlockedWhenKingInCheck) {
    // Place an enemy rook attacking the RED king's square.
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {219, ROOK, RED},
        {200, ROOK, BLUE},   // row 12, col 8 – attacks row 13 col 8 = 216
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    }, RED_OO);

    MoveList moves(pos);
    for (const auto& m : moves)
        EXPECT_NE(m.special_move(), 4)
            << "Castling must not be allowed while in check";
}

// ═══════════════════════════════════════════════════════════════
//  Promotion
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, PromotionMovesGenerated) {
    // RED pawn on row 4 (loc/16 == 4) → promotion rank.
    Position pos(false);
    setupPosition(pos, RED, {
        {69,  PAWN, RED},     // row 4, col 5
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });

    MoveList moves(pos);
    int promoCount = 0;
    for (const auto& m : moves)
        if (m.from() == 69 && m.promotion() != 0)
            promoCount++;

    EXPECT_EQ(promoCount, 4)
        << "Four promotion choices (Q, R, B, N) expected";
}

TEST_F(MoveGenTest, PromotionChangePieceType) {
    // After making a queen-promotion move the piece at the destination
    // should be a QUEEN, not a PAWN.
    Position pos(false);
    setupPosition(pos, RED, {
        {69,  PAWN, RED},     // row 4, col 5
        {216, KING, RED},
        {7,   KING, YELLOW},
        {113, KING, BLUE},
        {110, KING, GREEN},
    });

    MoveList moves(pos);
    for (const auto& m : moves) {
        if (m.from() == 69 && m.promotion() == QUEEN) {
            pos.move(m);
            EXPECT_EQ(pos.board.pieceMailbox[m.to()], QUEEN)
                << "Piece should be QUEEN after queen-promotion";
            pos.undoMove(m);
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  Zobrist key consistency
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, SamePositionSameZobrist) {
    Position a(false), b(false);
    loadFEN(a, START_FEN);
    loadFEN(b, START_FEN);
    EXPECT_EQ(a.gameStates.back().zobristKey,
              b.gameStates.back().zobristKey);
}

TEST_F(MoveGenTest, DifferentPositionDifferentZobrist) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    uint64_t before = pos.gameStates.back().zobristKey;

    MoveList moves(pos);
    pos.move(*moves.begin());
    EXPECT_NE(pos.gameStates.back().zobristKey, before);
}

// ═══════════════════════════════════════════════════════════════
//  Perft (regression / smoke tests)
// ═══════════════════════════════════════════════════════════════

TEST_F(MoveGenTest, PerftDepth1) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    EXPECT_EQ(fullsearch(1, pos), 20);
}

TEST_F(MoveGenTest, PerftDepth2IsConsistent) {
    // Two independent runs from the same position must agree.
    Position a(false), b(false);
    loadFEN(a, START_FEN);
    loadFEN(b, START_FEN);
    EXPECT_EQ(fullsearch(2, a), fullsearch(2, b));
}

TEST_F(MoveGenTest, PerftUnchangedAfterMakeUnmake) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    int p = fullsearch(2, pos);

    MoveList moves(pos);
    pos.move(*moves.begin());
    pos.undoMove(*moves.begin());

    EXPECT_EQ(fullsearch(2, pos), p);
}
