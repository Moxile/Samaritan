// Lifecycle tests for the NNUE evaluator.
//
// The incremental accumulator is only correct relative to the weights it was
// built from and the position it was built for. Every test here is about one of
// those two links being broken and detected: the network being switched on
// mid-game, the network being replaced under a live stack, the position being
// reloaded, and a weights file that is not what it claims to be.
//
// The full refresh is the oracle throughout: whatever the incremental path
// says, it must agree with rebuilding from the bare board.

#include <gtest/gtest.h>
#include "test_helpers.h"
#include "evaluate.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

// The network is a process-wide singleton (it is 262 MB, one per process is the
// point), so each test has to state what it wants rather than inherit it.
class NnueTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { initZobrist(); }
    void SetUp() override    { nnue::network().clear(); }
    void TearDown() override { nnue::network().clear(); }

    // Play the first legal move, whatever it is. These tests are about
    // accumulator bookkeeping, not about which move is good.
    static Move playFirstLegal(Position& pos)
    {
        MoveList moves(pos);
        EXPECT_GT(moves.size(), 0u);
        Move m = *moves.begin();
        pos.move(m);
        return m;
    }
};

TEST_F(NnueTest, HceIsUsedWhenNoNetworkIsLoaded)
{
    Position pos(true);
    loadFEN(pos, START_FEN);

    EXPECT_FALSE(nnue::network().ready());
    EXPECT_TRUE(pos.accStack.empty()) << "no network means no accumulators to keep";
    EXPECT_EQ(evaluate(pos), evaluateHCE(pos));
}

// The UCI sequence that used to corrupt evaluation: load a position with no
// network, switch the network on, then search. move() found an empty stack,
// refreshed it from the board *after* the move, called that the parent, and
// applied the move's delta a second time.
TEST_F(NnueTest, EnablingTheNetworkMidGameDoesNotApplyTheMoveTwice)
{
    Position pos(true);
    loadFEN(pos, START_FEN);
    playFirstLegal(pos);
    playFirstLegal(pos);

    ASSERT_TRUE(pos.accStack.empty());
    nnue::network().randomize(42);          // setoption, with a position already loaded
    ASSERT_FALSE(pos.accValid());

    playFirstLegal(pos);
    EXPECT_TRUE(pos.accValid()) << "the first move after enabling must rebuild the stack";
    EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos));

    // And it must keep agreeing once the incremental path takes over again.
    for (int i = 0; i < 4; i++)
    {
        playFirstLegal(pos);
        EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos)) << "ply " << i;
    }
}

// Replacing the weights invalidates every accumulator computed from the old
// ones. They are sums of specific weight rows; there is nothing to patch.
TEST_F(NnueTest, ReplacingTheNetworkInvalidatesTheStack)
{
    nnue::network().randomize(42);

    Position pos(true);
    loadFEN(pos, START_FEN);
    ASSERT_TRUE(pos.accValid());
    playFirstLegal(pos);
    ASSERT_TRUE(pos.accValid());

    nnue::network().randomize(43);          // a different network entirely
    EXPECT_FALSE(pos.accValid()) << "accumulators from the old weights must not be trusted";
    EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos));

    playFirstLegal(pos);
    EXPECT_TRUE(pos.accValid());
    EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos));
}

TEST_F(NnueTest, SwitchingBackToMaterialDropsTheStack)
{
    nnue::network().randomize(42);
    Position pos(true);
    loadFEN(pos, START_FEN);
    playFirstLegal(pos);
    ASSERT_TRUE(pos.accValid());

    nnue::network().clear();
    pos.refreshEval();                      // what the UCI handler does
    EXPECT_TRUE(pos.accStack.empty());
    EXPECT_FALSE(pos.accValid());
    EXPECT_EQ(evaluate(pos), evaluateHCE(pos));
}

// A fresh position under a live network is maintained from its own root.
TEST_F(NnueTest, ReloadingAPositionRebasesTheStack)
{
    nnue::network().randomize(42);
    Position pos(true);
    loadFEN(pos, START_FEN);
    for (int i = 0; i < 3; i++) playFirstLegal(pos);
    ASSERT_EQ(pos.accStack.size(), 4u);

    loadFEN(pos, START_FEN);
    EXPECT_EQ(pos.accStack.size(), 1u) << "a reloaded position starts a new stack";
    EXPECT_TRUE(pos.accValid());
    EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos));
}

// make/undo must leave the stack exactly as it was found, including when the
// stack root is deeper than the game root.
TEST_F(NnueTest, UndoRestoresTheAccumulatorStack)
{
    nnue::network().randomize(42);
    Position pos(true);
    loadFEN(pos, START_FEN);

    const int before = evaluate(pos);
    const size_t depth = pos.accStack.size();

    std::vector<Move> played;
    for (int i = 0; i < 3; i++) played.push_back(playFirstLegal(pos));
    for (int i = 2; i >= 0; i--) pos.undoMove(played[i]);

    EXPECT_EQ(pos.accStack.size(), depth);
    EXPECT_TRUE(pos.accValid());
    EXPECT_EQ(evaluate(pos), before);
    EXPECT_EQ(evaluate(pos), evaluateFullRefresh(pos));
}

// A position that never enables evaluation must not pay for one. This is what
// keeps perft and the movegen tests fast.
TEST_F(NnueTest, PositionsWithEvaluationDisabledKeepNoAccumulators)
{
    nnue::network().randomize(42);
    Position pos(false);
    loadFEN(pos, START_FEN);
    playFirstLegal(pos);

    EXPECT_TRUE(pos.accStack.empty());
    EXPECT_FALSE(pos.accValid());
}

// ---------------------------------------------------------------------------
// SNN1 weight files
//
// A rejected file must leave the engine exactly as it was: the alternative is
// playing on with a half-read network, which looks like a working engine and
// evaluates nonsense.
// ---------------------------------------------------------------------------

class NetworkFileTest : public ::testing::Test {
protected:
    void SetUp() override { path = std::string(::testing::TempDir()) + "snn1_test.bin"; }
    void TearDown() override { std::remove(path.c_str()); }

    void writeHeader(std::ofstream& f, const char* magic, int32_t nf, int32_t as, int32_t fs)
    {
        f.write(magic, 4);
        f.write(reinterpret_cast<const char*>(&nf), 4);
        f.write(reinterpret_cast<const char*>(&as), 4);
        f.write(reinterpret_cast<const char*>(&fs), 4);
    }

    std::string path;
};

TEST_F(NetworkFileTest, RejectsAMissingFile)
{
    nnue::Network net;
    std::string error;
    EXPECT_FALSE(net.load(path + ".does-not-exist", &error));
    EXPECT_NE(error.find("cannot open"), std::string::npos);
    EXPECT_FALSE(net.ready());
}

TEST_F(NetworkFileTest, RejectsTheWrongMagic)
{
    { std::ofstream f(path, std::ios::binary);
      writeHeader(f, "NNUE", nnue::NUM_FEATURES, nnue::ACC_SIZE, nnue::FF_SIZE); }

    nnue::Network net;
    std::string error;
    EXPECT_FALSE(net.load(path, &error));
    EXPECT_NE(error.find("SNN1"), std::string::npos);
}

TEST_F(NetworkFileTest, RejectsADifferentArchitecture)
{
    { std::ofstream f(path, std::ios::binary);
      writeHeader(f, "SNN1", nnue::NUM_FEATURES, nnue::ACC_SIZE * 2, nnue::FF_SIZE); }

    nnue::Network net;
    std::string error;
    EXPECT_FALSE(net.load(path, &error));
    EXPECT_NE(error.find("architecture mismatch"), std::string::npos);
}

TEST_F(NetworkFileTest, RejectsATruncatedFileWithoutDisturbingTheLoadedNetwork)
{
    // A correct header followed by far too few weights.
    { std::ofstream f(path, std::ios::binary);
      writeHeader(f, "SNN1", nnue::NUM_FEATURES, nnue::ACC_SIZE, nnue::FF_SIZE);
      const std::vector<float> some(1024, 0.5f);
      f.write(reinterpret_cast<const char*>(some.data()), 1024 * 4); }

    nnue::Network net;
    net.randomize(7);
    const uint32_t gen = net.generation();
    const float keep = net.ff2B;

    std::string error;
    EXPECT_FALSE(net.load(path, &error));
    EXPECT_NE(error.find("truncated"), std::string::npos);
    EXPECT_TRUE(net.ready())        << "the previous network must survive a bad load";
    EXPECT_EQ(net.generation(), gen) << "a rejected load is not a change";
    EXPECT_EQ(net.ff2B, keep);
    net.clear();
}

TEST_F(NetworkFileTest, GenerationChangesWithEveryWeightChange)
{
    nnue::Network net;
    const uint32_t g0 = net.generation();
    net.randomize(1);
    const uint32_t g1 = net.generation();
    net.randomize(2);
    const uint32_t g2 = net.generation();
    net.clear();
    const uint32_t g3 = net.generation();

    EXPECT_NE(g0, g1);
    EXPECT_NE(g1, g2);
    EXPECT_NE(g2, g3);
}

} // namespace
