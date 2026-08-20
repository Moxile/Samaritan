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

// ═══════════════════════════════════════════════════════════════
//  Hand-crafted evaluation: material + slider mobility
// ═══════════════════════════════════════════════════════════════

TEST_F(SearchTest, OpenSliderMobilityIsRewardedByPieceWeight) {
    auto mobilityOf = [](PieceType piece) {
        Position pos(false);
        setupPosition(pos, RED, {
            {119, piece, RED},
            {216, KING, RED}, {7, KING, YELLOW},
            {113, KING, BLUE}, {110, KING, GREEN},
        });
        return evaluateMobility(pos);
    };

    const int bishop = mobilityOf(BISHOP);
    const int rook   = mobilityOf(ROOK);
    const int queen  = mobilityOf(QUEEN);
    EXPECT_GT(bishop, 0);
    EXPECT_GT(rook, bishop);
    EXPECT_GT(queen, rook);
}

TEST_F(SearchTest, FriendlyPiecesBlockSliderMobility) {
    Position open(false), blocked(false);
    const std::vector<Piece> kings = {
        {216, KING, RED}, {7, KING, YELLOW},
        {113, KING, BLUE}, {110, KING, GREEN},
    };

    auto openPieces = kings;
    openPieces.push_back({119, QUEEN, RED});
    setupPosition(open, RED, openPieces);

    auto blockedPieces = openPieces;
    for (int sq : {102, 103, 104, 118, 120, 134, 135, 136})
        blockedPieces.push_back({sq, PAWN, RED});
    setupPosition(blocked, RED, blockedPieces);

    EXPECT_GT(evaluateMobility(open), evaluateMobility(blocked));
    EXPECT_EQ(sliderMobility(blocked.board, 119, QUEEN, TEAM_RY), 0);
}

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


// ═══════════════════════════════════════════════════════════════
//  Draw rules  (4PC: threefold repetition, 50 whole moves without progress)
// ═══════════════════════════════════════════════════════════════

// Each player moves a knight out, then each moves it back: eight plies that
// return to exactly the starting position.
static bool shuffleRound(Position& p) {
    std::vector<Move> out;
    for (int i = 0; i < 4; i++) {
        Move chosen; bool got = false;
        for (const auto& m : MoveList(p))
            if (p.board.pieceMailbox[m.from()] == KNIGHT && p.board.isEmpty(m.to())) {
                chosen = m; got = true; break;
            }
        if (!got) return false;
        p.move(chosen); out.push_back(chosen);
    }
    for (int i = 0; i < 4; i++) {
        Move back(out[i].from(), out[i].to(), 0, 0);
        if (!MoveList(p).contains(back)) return false;
        p.move(back);
    }
    return true;
}

TEST_F(SearchTest, ShuffleRoundReturnsToTheSamePosition) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    const uint64_t key = pos.gameStates.back().zobristKey;
    ASSERT_TRUE(shuffleRound(pos));
    EXPECT_EQ(pos.gameStates.back().zobristKey, key)
        << "eight plies of knight shuffling should restore the position exactly";
    EXPECT_EQ(pos.gameStates.back().halfmoveClock, 8);
}

TEST_F(SearchTest, ThreefoldRepetitionIsADrawButTwofoldIsNot) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    EXPECT_FALSE(isDraw(pos)) << "first occurrence";
    ASSERT_TRUE(shuffleRound(pos));
    EXPECT_FALSE(isDraw(pos)) << "second occurrence must not be a draw";
    ASSERT_TRUE(shuffleRound(pos));
    EXPECT_TRUE(isDraw(pos)) << "third occurrence is a threefold draw";
}

TEST_F(SearchTest, HalfmoveClockResetsOnPawnMoveAndCapture) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    ASSERT_TRUE(shuffleRound(pos));
    ASSERT_EQ(pos.gameStates.back().halfmoveClock, 8);
    // a pawn push is irreversible
    Move pawn(165, 197, 0, 0);
    ASSERT_TRUE(MoveList(pos).contains(pawn));
    pos.move(pawn);
    EXPECT_EQ(pos.gameStates.back().halfmoveClock, 0);
}

TEST_F(SearchTest, FiftyMoveRuleIsADraw) {
    Position pos(false);
    loadFEN(pos, START_FEN);
    int rounds = 0;
    while (pos.gameStates.back().halfmoveClock < HALFMOVE_LIMIT && rounds < 64) {
        ASSERT_TRUE(shuffleRound(pos));
        rounds++;
    }
    EXPECT_EQ(pos.gameStates.back().halfmoveClock, HALFMOVE_LIMIT);
    EXPECT_EQ(HALFMOVE_LIMIT, 200) << "50 whole moves x 4 players";
    EXPECT_TRUE(isDraw(pos));
}

// ═══════════════════════════════════════════════════════════════
//  Mate scores survive the transposition table
// ═══════════════════════════════════════════════════════════════

TEST_F(SearchTest, KingCaptureScoresAsMateAtDistanceOne) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED}, {200, ROOK, BLUE}, {113, KING, BLUE},
        {7, KING, YELLOW}, {110, KING, GREEN}, {109, QUEEN, RED},
    });
    TranspositionTable warm; warm.resize(4);
    for (int d = 1; d <= 5; d++) {
        SearchInfo fi, wi;
        TranspositionTable fresh; fresh.resize(4);
        EXPECT_EQ(negaMax(pos, d, 0, fi, fresh), VALUE_MATE - 1)
            << "depth " << d << " with a fresh table";
        EXPECT_EQ(negaMax(pos, d, 0, wi, warm), VALUE_MATE - 1)
            << "depth " << d << " with a warm table";
    }
}


// ═══════════════════════════════════════════════════════════════
//  Quiescence: terminal states and check
// ═══════════════════════════════════════════════════════════════

// A king capture available at the horizon is mate, not a pile of material.
// Quiescence used to score it as the static evaluation of the position it
// reached: movegen (correctly) offers no moves once a king is gone, so nothing
// there ever noticed the game had ended.
TEST_F(SearchTest, QuiescenceScoresAKingCaptureAsMate) {
    Position pos(false);
    setupPosition(pos, RED, {
        {114, QUEEN, RED},    // one square from the blue king
        {113, KING, BLUE},
        {216, KING, RED},
        {7,   KING, YELLOW},
        {110, KING, GREEN},
    });

    TranspositionTable tt; tt.resize(1);
    SearchInfo info;
    const int q = qSearch(pos, 0, info, tt, -VALUE_INFINITE, VALUE_INFINITE);

    EXPECT_EQ(q, VALUE_MATE - 1) << "capturing a king is mate at distance one";
    EXPECT_GE(q, VALUE_MATE_IN_MAX_PLY) << "must read as a mate score, not a material score";

    // Entering through alpha-beta at depth 0 goes straight to quiescence, and
    // has to agree.
    SearchInfo info2;
    TranspositionTable tt2; tt2.resize(1);
    EXPECT_EQ(negaMax(pos, 0, 0, info2, tt2), VALUE_MATE - 1);
}

// A checked side cannot stand pat: it has to answer the check, and its only
// answer may be a quiet move. Quiescence therefore searches every legal move
// when in check, not only captures.
TEST_F(SearchTest, QuiescenceSearchesEvasionsWhenInCheck) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED},
        {212, ROOK, BLUE},    // same rank as the red king, nothing in between
        {113, KING, BLUE},
        {7,   KING, YELLOW},
        {110, KING, GREEN},
    });
    ASSERT_TRUE(inCheck(pos, RED));

    // No red or yellow piece can capture the rook, so a captures-only
    // quiescence has nothing at all to search.
    int captures = 0;
    for (const auto& m : MoveList(pos))
        if (m.gen_type == CAPTURES) captures++;
    ASSERT_EQ(captures, 0);

    TranspositionTable tt; tt.resize(1);
    SearchInfo info;
    qSearch(pos, 0, info, tt, -VALUE_INFINITE, VALUE_INFINITE);

    EXPECT_GT(info.nodes, 1) << "the evasions must actually be searched";
}

// With no king on the board for the side to move, the position is over. It must
// not reach the attack scan, which indexes tables by the king's square.
TEST_F(SearchTest, SearchHandlesAMissingKing) {
    Position pos(false);
    setupPosition(pos, RED, {
        {216, KING, RED}, {113, KING, BLUE}, {7, KING, YELLOW},   // green has none
        {114, QUEEN, RED},
    });
    EXPECT_FALSE(inCheck(pos, GREEN));

    TranspositionTable tt; tt.resize(1);
    SearchInfo info;
    EXPECT_NO_THROW(negaMax(pos, 2, 0, info, tt));
}

// ═══════════════════════════════════════════════════════════════
//  The draw clock survives FEN
// ═══════════════════════════════════════════════════════════════

// START_FEN with a different halfmove field. The fields are '-' separated and
// the clock is the sixth.
static std::string fenWithClock(int clock) {
    const std::string marker = "-0,0,0,0-0-";
    const size_t at = START_FEN.rfind(marker);
    return START_FEN.substr(0, at) + "-0,0,0,0-" + std::to_string(clock) + "-"
         + START_FEN.substr(at + marker.size());
}

TEST_F(SearchTest, FENCarriesTheDrawClock) {
    for (int clock : {0, 199, 200}) {
        Position pos(false);
        loadFEN(pos, fenWithClock(clock));
        EXPECT_EQ(pos.gameStates.back().halfmoveClock, clock)
            << "clock " << clock << " was dropped on load";
        EXPECT_EQ(isDraw(pos), clock >= HALFMOVE_LIMIT)
            << "clock " << clock << " decides the progress draw";
    }
}

TEST_F(SearchTest, FENRoundTripsTheDrawClock) {
    Position pos(false);
    loadFEN(pos, fenWithClock(199));
    ASSERT_FALSE(isDraw(pos));

    // One quiet move takes it to the limit; serialising and reloading must not
    // hand back a position with 200 plies of progress it never made.
    Position reloaded(false);
    loadFEN(reloaded, positionToFEN(pos));
    EXPECT_EQ(reloaded.gameStates.back().halfmoveClock, 199);
    EXPECT_FALSE(isDraw(reloaded));

    Move knight(180, 213, 0, 0);   // a quiet knight move, no capture, no pawn
    ASSERT_TRUE(MoveList(pos).contains(knight));
    pos.move(knight);
    EXPECT_EQ(pos.gameStates.back().halfmoveClock, 200);
    EXPECT_TRUE(isDraw(pos));

    Position atLimit(false);
    loadFEN(atLimit, positionToFEN(pos));
    EXPECT_EQ(atLimit.gameStates.back().halfmoveClock, 200);
    EXPECT_TRUE(isDraw(atLimit)) << "a drawn position must still be drawn after a round trip";
}
