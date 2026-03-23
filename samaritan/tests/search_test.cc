#include <gtest/gtest.h>
#include "test_helpers.h"
#include "search.h"

// ───────────────────────────────────────────────────────────────
// Fixture
// ───────────────────────────────────────────────────────────────
class SearchTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { initZobrist(); }
};

// Starting-position pieces for use with setupPosition.
// Avoids loadFEN which calls refreshNNUE unconditionally and
// triggers the KING-feature OOB bug.
static const std::vector<Piece> START_PIECES = {
    // Yellow back rank (row 0, cols 4-11)
    {4, ROOK, YELLOW}, {5, KNIGHT, YELLOW}, {6, BISHOP, YELLOW},
    {7, KING, YELLOW}, {8, QUEEN, YELLOW}, {9, BISHOP, YELLOW},
    {10, KNIGHT, YELLOW}, {11, ROOK, YELLOW},
    // Yellow pawns (row 1, cols 4-11)
    {20, PAWN, YELLOW}, {21, PAWN, YELLOW}, {22, PAWN, YELLOW},
    {23, PAWN, YELLOW}, {24, PAWN, YELLOW}, {25, PAWN, YELLOW},
    {26, PAWN, YELLOW}, {27, PAWN, YELLOW},
    // Blue pieces (col 1, rows 3-10)
    {49, ROOK, BLUE}, {65, KNIGHT, BLUE}, {81, BISHOP, BLUE},
    {97, QUEEN, BLUE}, {113, KING, BLUE}, {129, BISHOP, BLUE},
    {145, KNIGHT, BLUE}, {161, ROOK, BLUE},
    // Blue pawns (col 2, rows 3-10)
    {50, PAWN, BLUE}, {66, PAWN, BLUE}, {82, PAWN, BLUE},
    {98, PAWN, BLUE}, {114, PAWN, BLUE}, {130, PAWN, BLUE},
    {146, PAWN, BLUE}, {162, PAWN, BLUE},
    // Green pieces (col 14, rows 3-10)
    {62, ROOK, GREEN}, {78, KNIGHT, GREEN}, {94, BISHOP, GREEN},
    {110, KING, GREEN}, {126, QUEEN, GREEN}, {142, BISHOP, GREEN},
    {158, KNIGHT, GREEN}, {174, ROOK, GREEN},
    // Green pawns (col 13, rows 3-10)
    {61, PAWN, GREEN}, {77, PAWN, GREEN}, {93, PAWN, GREEN},
    {109, PAWN, GREEN}, {125, PAWN, GREEN}, {141, PAWN, GREEN},
    {157, PAWN, GREEN}, {173, PAWN, GREEN},
    // Red pawns (row 12, cols 4-11)
    {196, PAWN, RED}, {197, PAWN, RED}, {198, PAWN, RED},
    {199, PAWN, RED}, {200, PAWN, RED}, {201, PAWN, RED},
    {202, PAWN, RED}, {203, PAWN, RED},
    // Red back rank (row 13, cols 4-11)
    {212, ROOK, RED}, {213, KNIGHT, RED}, {214, BISHOP, RED},
    {215, QUEEN, RED}, {216, KING, RED}, {217, BISHOP, RED},
    {218, KNIGHT, RED}, {219, ROOK, RED},
};

// ═══════════════════════════════════════════════════════════════
//  Transposition table basics
// ═══════════════════════════════════════════════════════════════

TEST_F(SearchTest, TTStoreAndProbe) {
    TranspositionTable tt;
    tt.resize(1);   // 1 MB

    uint64_t key = 0xDEADBEEF12345678ULL;
    Move m(100, 50, 0, 0);
    tt.store(key, 42, m, 5, TT_EXACT);

    TTEntry* e = tt.probe(key);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->score, 42);
    EXPECT_EQ(e->depth, 5);
    EXPECT_EQ(e->flag, TT_EXACT);
    EXPECT_EQ(e->bestMove, m);
}

TEST_F(SearchTest, TTMissReturnsNull) {
    TranspositionTable tt;
    tt.resize(1);
    EXPECT_EQ(tt.probe(0xCAFEBABEULL), nullptr);
}

TEST_F(SearchTest, TTOverwritesEntry) {
    TranspositionTable tt;
    tt.resize(1);

    uint64_t key = 42;
    Move m1(10, 20, 0, 0);
    Move m2(30, 40, 0, 0);

    tt.store(key, 100, m1, 3, TT_EXACT);
    tt.store(key, 200, m2, 5, TT_LOWER);

    TTEntry* e = tt.probe(key);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->score, 200);
    EXPECT_EQ(e->bestMove, m2);
}

// ═══════════════════════════════════════════════════════════════
//  Search – basic correctness
// ═══════════════════════════════════════════════════════════════

TEST_F(SearchTest, FindsKingCapture) {
    // RED queen can capture BLUE king in one move.
    // At depth >= 2 the engine detects lastCapturedPiece == KING.
    Position pos(true);
    setupPosition(pos, RED, {
        {114, QUEEN, RED},   // row 7, col 2
        {113, KING, BLUE},   // row 7, col 1 – one square away
        {216, KING, RED},
        {7,   KING, YELLOW},
        {110, KING, GREEN},
    });

    TranspositionTable tt;
    tt.resize(1);

    SearchInfo info = iterativeDeepening(pos, tt, 3, /*silent=*/true);
    ASSERT_GT(info.pv_length[0], 0) << "PV should contain at least one move";

    Move best = info.pv_table[0][0];
    EXPECT_EQ(best.from(), 114) << "Best move should originate from the queen";
    EXPECT_EQ(best.to(), 113)   << "Best move should capture the blue king";
}

TEST_F(SearchTest, BestMoveIsLegal) {
    // Use eval=false + setupPosition to avoid the NNUE KING-feature
    // OOB bug.  Search still runs — it gets a constant NNUE score.
    Position pos(false);
    setupPosition(pos, RED, START_PIECES, ALL_CASTLING);

    TranspositionTable tt;
    tt.resize(1);

    SearchInfo info = iterativeDeepening(pos, tt, 3, true);
    ASSERT_GT(info.pv_length[0], 0);

    Move best = info.pv_table[0][0];
    MoveList moves(pos);
    bool found = false;
    for (const auto& m : moves)
        if (m.getRawData() == best.getRawData()) { found = true; break; }

    EXPECT_TRUE(found) << "Best move " << best.toUCI()
                       << " is not in the legal move list";
}

TEST_F(SearchTest, PVMovesAreLegal) {
    Position pos(false);
    setupPosition(pos, RED, START_PIECES, ALL_CASTLING);

    TranspositionTable tt;
    tt.resize(1);

    SearchInfo info = iterativeDeepening(pos, tt, 4, true);

    std::vector<Move> played;
    for (int i = 0; i < info.pv_length[0]; i++) {
        Move pvMove = info.pv_table[0][i];
        MoveList legal(pos);
        bool found = false;
        for (const auto& m : legal)
            if (m.getRawData() == pvMove.getRawData()) { found = true; break; }

        EXPECT_TRUE(found)
            << "PV move #" << i << " (" << pvMove.toUCI()
            << ") is illegal at its position";
        if (!found) break;

        pos.move(pvMove);
        played.push_back(pvMove);
    }
    for (int i = (int)played.size() - 1; i >= 0; i--)
        pos.undoMove(played[i]);
}

TEST_F(SearchTest, SearchIsDeterministic) {
    Position a(false), b(false);
    setupPosition(a, RED, START_PIECES, ALL_CASTLING);
    setupPosition(b, RED, START_PIECES, ALL_CASTLING);

    TranspositionTable tt_a, tt_b;
    tt_a.resize(1);
    tt_b.resize(1);

    SearchInfo ia = iterativeDeepening(a, tt_a, 3, true);
    SearchInfo ib = iterativeDeepening(b, tt_b, 3, true);

    ASSERT_GT(ia.pv_length[0], 0);
    ASSERT_GT(ib.pv_length[0], 0);
    EXPECT_EQ(ia.pv_table[0][0].getRawData(),
              ib.pv_table[0][0].getRawData())
        << "Same position should produce the same best move";
}

TEST_F(SearchTest, DeeperSearchDoesNotCrash) {
    Position pos(false);
    setupPosition(pos, RED, START_PIECES, ALL_CASTLING);

    TranspositionTable tt;
    tt.resize(4);

    SearchInfo info = iterativeDeepening(pos, tt, 5, true);
    EXPECT_GT(info.nodes, 0);
    EXPECT_GT(info.pv_length[0], 0);
}

// ═══════════════════════════════════════════════════════════════
//  Null-move pruning guards
// ═══════════════════════════════════════════════════════════════

TEST_F(SearchTest, NullMoveZobristRestore) {
    Position pos(false);
    setupPosition(pos, RED, START_PIECES, ALL_CASTLING);

    uint64_t before = pos.gameStates.back().zobristKey;
    pos.makeNullMove();
    pos.undoNullMove();
    EXPECT_EQ(pos.gameStates.back().zobristKey, before);
}

TEST_F(SearchTest, NullMoveSwitchesTurn) {
    Position pos(false);
    setupPosition(pos, RED, START_PIECES, ALL_CASTLING);

    PieceColor before = pos.gameStates.back().curTurn;
    pos.makeNullMove();
    PieceColor after = pos.gameStates.back().curTurn;
    EXPECT_NE(before, after) << "Null move must change the side to move";
    pos.undoNullMove();
    EXPECT_EQ(pos.gameStates.back().curTurn, before);
}
