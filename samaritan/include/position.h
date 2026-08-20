#pragma once

#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "board.h"
#include "nnue/accumulator.h"
#include "transpositiontable.h"

class GameState
{
public:
    PieceColor curTurn;
    int posScore = 0;
    Move bestMove;
    int castleRights = NO_CASTLING;
    int enpassants[4] = {-1 ,-1 ,-1, -1};
    PieceType lastCapturedPiece = NONE_PIECE;
    PieceColor lastCapturedPieceColor = NONE_COLOR;
    uint64_t zobristKey = 0;

    // Plies since the last capture or pawn move, for the 50-move rule.
    int halfmoveClock = 0;
    // Plies since the last null move. Repetition scanning must not look past
    // one: a null move changes the side to move without changing the board and
    // would otherwise fabricate repetitions.
    int pliesFromNull = 0;

    // Computed on first use by movegen, not by move(): a node that gets cut off
    // before it generates anything never pays for this.
    CheckInfo checkInfo;

    // What this move changed, for the incremental evaluator.
    nnue::DirtyState dirty;
};

class Position
{
public:
    // Reserved depth of the accumulator stack. Deep enough for any search this
    // engine runs; going past it only costs a reallocation, never correctness.
    static constexpr size_t MAX_ACC_STACK = 128;

    Board board;
    std::vector<GameState> gameStates;
    // Retained so existing call sites keep compiling. Evaluation is now cheap
    // and stateless, so nothing is allocated or skipped on account of it.
    const bool useEval;

    // Accumulator per ply. Maintained only when evaluation is enabled, so perft
    // and the tests never pay for it.
    //
    // The stack does not have to start at the root: accStack[i] describes
    // gameStates[accBase + i]. That lets a network switched on in the middle of
    // a game start maintaining accumulators from wherever the game happens to
    // be, instead of pretending it had been there all along.
    std::vector<nnue::Accumulators> accStack;
    // Index of the game state accStack[0] belongs to.
    size_t accBase = 0;
    // Which network the stack was built from. A network that is replaced, or
    // switched off and on again, invalidates every accumulator computed from
    // the old weights -- these are floats accumulated from specific weight
    // rows, not something that can be patched up.
    uint32_t accGen = 0;

    Position(bool eval = true) : board(), useEval(eval) {}

    bool evalActive() const { return useEval && nnue::network().ready(); }

    // Is accStack.back() a usable accumulator for the current position? It must
    // be non-empty, built from the current weights, and reach exactly the top of
    // the game-state stack.
    bool accValid() const
    {
        return evalActive()
            && !accStack.empty()
            && accGen == nnue::network().generation()
            && accBase + accStack.size() == gameStates.size();
    }

    // Rebuild the accumulator for the current position from scratch. Call this
    // whenever the position or the network is replaced wholesale.
    void refreshEval()
    {
        accStack.clear();
        if (!evalActive() || gameStates.empty()) return;
        // 64 plies of headroom: the search pushes one accumulator per ply, and
        // reallocating a vector of 2 KB elements mid-search is pure waste.
        accStack.reserve(MAX_ACC_STACK);
        accStack.emplace_back();
        accStack.back().refresh(board);
        accBase = gameStates.size() - 1;
        accGen  = nnue::network().generation();
    }

    // Debug-only whole-state check: every derived structure must agree with the
    // mailboxes, and the incremental Zobrist key must equal a fresh computation.
    // Written to be obviously correct rather than fast; call it from tests and
    // from corpus traversals, not from the search.
    bool isConsistent(std::string *why = nullptr) const
    {
        auto fail = [&](const char *msg) { if (why) *why = msg; return false; };

        if (!board.pieceListConsistent()) return fail("piece list disagrees with the mailboxes");

        // kingTracker: exactly the square holding that colour's king, or -1.
        for (int c = 0; c < 4; ++c)
        {
            const PieceColor col = static_cast<PieceColor>(1 << c);
            int found = -1;
            for (int sq = 0; sq < 224; ++sq)
                if (board.pieceMailbox[sq] == KING && board.colorMailbox[sq] == col)
                {
                    if (found != -1) return fail("two kings of the same colour");
                    found = sq;
                }
            if (board.kingTracker[c] != found) return fail("kingTracker disagrees with the board");
        }

        // nonPawnPieceCount: every non-pawn, non-empty square of that colour.
        int npc[4] = {0, 0, 0, 0};
        for (int sq = 0; sq < 224; ++sq)
        {
            const PieceType pie = board.pieceMailbox[sq];
            if (pie == NONE_PIECE || pie == PAWN) continue;
            if (board.colorMailbox[sq] == NONE_COLOR) continue;
            npc[ctz((unsigned int)board.colorMailbox[sq])]++;
        }
        for (int c = 0; c < 4; ++c)
            if (npc[c] != board.nonPawnPieceCount[c]) return fail("nonPawnPieceCount is wrong");

        // Zobrist: recompute from scratch and compare.
        if (gameStates.empty()) return fail("no game state");
        const GameState &st = gameStates.back();
        uint64_t key = 0;
        for (int sq = 0; sq < 224; ++sq)
        {
            if (board.pieceMailbox[sq] == NONE_PIECE) continue;
            if (board.colorMailbox[sq] == NONE_COLOR) return fail("piece with no colour");
            key ^= zobristPieces[board_table[sq]][board.pieceMailbox[sq] - 1]
                                [ctz((unsigned int)board.colorMailbox[sq])];
        }
        key ^= zobristTurn[ctz((unsigned int)st.curTurn)];
        for (int i = 0; i < 8; ++i)
            if (st.castleRights & (1 << i)) key ^= zobristCastle[i];
        for (int p = 0; p < 4; ++p)
            if (st.enpassants[p] != -1) key ^= zobristEnPassant[p][board_table[st.enpassants[p]]];
        if (key != st.zobristKey) return fail("incremental Zobrist key has drifted");

        return true;
    }

    void makeNullMove()
    {
        GameState state = GameState();
        state.castleRights = gameStates.back().castleRights;
        state.curTurn = gameStates.back().curTurn;
        state.curTurn++;

        state.zobristKey = gameStates.back().zobristKey;
        state.zobristKey ^= zobristTurn[ctz((unsigned int)state.curTurn)];
        state.zobristKey ^= zobristTurn[ctz((unsigned int)gameStates.back().curTurn)];


        // en passant
        std::memcpy(state.enpassants,
            gameStates.back().enpassants,
            sizeof(state.enpassants));
        state.enpassants[ctz((unsigned int)state.curTurn)] = -1;
        state.halfmoveClock = gameStates.back().halfmoveClock + 1;
        state.pliesFromNull = 0;
        gameStates.push_back(state);
    }

    void undoNullMove()
    {
        gameStates.pop_back();
    }

    void move(const Move &move)
    {
        // Make the evaluator's view current *before* the board changes. If the
        // stack is unusable -- evaluation was switched on mid-game, or the
        // network was replaced -- this is the one place where a full rebuild
        // still describes the position the delta below will be applied to.
        // Rebuilding afterwards instead, from the already-moved board, is what
        // made the first move after `setoption` count itself twice.
        if (evalActive() && !accValid()) refreshEval();

        const auto loc = move.from();
        const auto destination = move.to();
        const auto movingPiece = board.pieceMailbox[loc];
        // Colour of the mover, captured before the board is updated below.
        const auto movingColor = board.colorMailbox[loc];
        const auto movingColorIdx = ctz((unsigned int)movingColor);
        // Live view of the destination square: the captured piece before the
        // move, the moving piece (or its promotion) afterwards.
        auto &destPiece = board.pieceMailbox[destination];

        // Create the new game state
        GameState oldState = gameStates.back();
        GameState state = GameState();
        state.castleRights = oldState.castleRights;
        state.curTurn = oldState.curTurn;
        state.curTurn++;
        state.lastCapturedPiece = board.pieceMailbox[destination];
        state.lastCapturedPieceColor = board.colorMailbox[destination];

        state.zobristKey = oldState.zobristKey;

        // turn
        state.zobristKey ^= zobristTurn[ctz((unsigned int)state.curTurn)];
        state.zobristKey ^= zobristTurn[ctz((unsigned int)oldState.curTurn)];


        // Null-move pruning gates on this counter, so it must be maintained
        // whether or not the evaluator is running -- it used to live inside the
        // useEval block and silently froze in perft and test builds.
        if (state.lastCapturedPiece != NONE_PIECE && state.lastCapturedPiece != PAWN)
            board.nonPawnPieceCount[ctz((unsigned int) state.lastCapturedPieceColor)]--;

        std::memcpy(state.enpassants,
            oldState.enpassants,
            sizeof(state.enpassants));
        state.enpassants[ctz((unsigned int)state.curTurn)] = -1;

        // Piece list: drop the captured piece, then relocate the mover. Both
        // must happen while the mailboxes still describe the old position.
        if (state.lastCapturedPiece != NONE_PIECE)
            board.listRemove(destination, state.lastCapturedPieceColor);

        // A captured king leaves no king on the board for that colour. Recording
        // it as -1 is what makes the position detectably terminal; leaving the
        // old square would make it look like a king still stood there.
        if (state.lastCapturedPiece == KING)
            board.kingTracker[ctz((unsigned int)state.lastCapturedPieceColor)] = -1;
        board.listMove(loc, destination, movingColor);

        // Record what changed, for the incremental evaluator. A king move
        // invalidates only its own perspective, because kings are not features.
        if (movingPiece == KING)
            state.dirty.refreshColour = movingColorIdx;
        else
            state.dirty.add(loc, destination, movingPiece, movingColor);

        if (state.lastCapturedPiece != NONE_PIECE)
            state.dirty.add(destination, -1, state.lastCapturedPiece, state.lastCapturedPieceColor);

        // Draw-rule bookkeeping. A capture or a pawn move is irreversible and
        // resets the clock; everything else advances it.
        const bool irreversible = (state.lastCapturedPiece != NONE_PIECE) || (movingPiece == PAWN);
        state.halfmoveClock  = irreversible ? 0 : oldState.halfmoveClock + 1;
        state.pliesFromNull  = oldState.pliesFromNull + 1;

        // Move the piece
        board.pieceMailbox[destination] = board.pieceMailbox[loc];
        board.colorMailbox[destination] = board.colorMailbox[loc];
        board.pieceMailbox[loc] = NONE_PIECE;
        board.colorMailbox[loc] = NONE_COLOR;

        auto special = move.special_move();
        // Handle Promotion
        if(special == 1 || special == 3)
        {
            destPiece = static_cast<PieceType>(move.promotion());
            // A pawn became a piece: it now counts toward null-move pruning.
            board.nonPawnPieceCount[movingColorIdx]++;
            // The pawn left `loc`; what arrives at `destination` is the new piece.
            state.dirty.pieces[0].to = -1;
            state.dirty.add(-1, destination, destPiece, movingColor);
        }
        // Check castling
        else if (special == 4)
        {
            switch (movingColor)
            {
            case RED:
                if (destination == K14)
                {
                    state.zobristKey ^= zobristPieces[board_table[L14]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[J14]][ROOK-1][movingColorIdx];
                    state.dirty.add(L14, J14, ROOK, RED);
                    board.listMove(L14, J14, RED);
                    board.pieceMailbox[J14] = ROOK;
                    board.colorMailbox[J14] = RED;
                    board.pieceMailbox[L14] = NONE_PIECE;
                    board.colorMailbox[L14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    state.zobristKey ^= zobristPieces[board_table[E14]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[H14]][ROOK-1][movingColorIdx];
                    state.dirty.add(E14, H14, ROOK, RED);
                    board.listMove(E14, H14, RED);
                    board.pieceMailbox[H14] = ROOK;
                    board.colorMailbox[H14] = RED;
                    board.pieceMailbox[E14] = NONE_PIECE;
                    board.colorMailbox[E14] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == B6)
                {
                    state.zobristKey ^= zobristPieces[board_table[B4]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[B7]][ROOK-1][movingColorIdx];
                    state.dirty.add(B4, B7, ROOK, BLUE);
                    board.listMove(B4, B7, BLUE);
                    board.pieceMailbox[B7] = ROOK;
                    board.colorMailbox[B7] = BLUE;
                    board.pieceMailbox[B4] = NONE_PIECE;
                    board.colorMailbox[B4] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    state.zobristKey ^= zobristPieces[board_table[B11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[B9]][ROOK-1][movingColorIdx];
                    state.dirty.add(B11, B9, ROOK, BLUE);
                    board.listMove(B11, B9, BLUE);
                    board.pieceMailbox[B9] = ROOK;
                    board.colorMailbox[B9] = BLUE;
                    board.pieceMailbox[B11] = NONE_PIECE;
                    board.colorMailbox[B11] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == F1)
                {
                    state.zobristKey ^= zobristPieces[board_table[E1]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[G1]][ROOK-1][movingColorIdx];
                    state.dirty.add(E1, G1, ROOK, YELLOW);
                    board.listMove(E1, G1, YELLOW);
                    board.pieceMailbox[G1] = ROOK;
                    board.colorMailbox[G1] = YELLOW;
                    board.pieceMailbox[E1] = NONE_PIECE;
                    board.colorMailbox[E1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    state.zobristKey ^= zobristPieces[board_table[L1]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[I1]][ROOK-1][movingColorIdx];
                    state.dirty.add(L1, I1, ROOK, YELLOW);
                    board.listMove(L1, I1, YELLOW);
                    board.pieceMailbox[I1] = ROOK;
                    board.colorMailbox[I1] = YELLOW;
                    board.pieceMailbox[L1] = NONE_PIECE;
                    board.colorMailbox[L1] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == O5)
                {
                    state.zobristKey ^= zobristPieces[board_table[O4]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[O6]][ROOK-1][movingColorIdx];
                    state.dirty.add(O4, O6, ROOK, GREEN);
                    board.listMove(O4, O6, GREEN);
                    board.pieceMailbox[O6] = ROOK;
                    board.colorMailbox[O6] = GREEN;
                    board.pieceMailbox[O4] = NONE_PIECE;
                    board.colorMailbox[O4] = NONE_COLOR;
                }
                else if (destination == O9)
                {
                    state.zobristKey ^= zobristPieces[board_table[O11]][ROOK-1][movingColorIdx];
                    state.zobristKey ^= zobristPieces[board_table[O8]][ROOK-1][movingColorIdx];
                    state.dirty.add(O11, O8, ROOK, GREEN);
                    board.listMove(O11, O8, GREEN);
                    board.pieceMailbox[O8] = ROOK;
                    board.colorMailbox[O8] = GREEN;
                    board.pieceMailbox[O11] = NONE_PIECE;
                    board.colorMailbox[O11] = NONE_COLOR;
                }
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Update castling rights when king is moved
        if (destPiece == KING)
        {
            switch (movingColor)
            {
            case RED:
                state.castleRights = oldState.castleRights & ~RED_CASTLING;
                board.kingTracker[0] = destination;
                break;
            case BLUE:
                state.castleRights = oldState.castleRights & ~BLUE_CASTLING;
                board.kingTracker[1] = destination;
                break;
            case YELLOW:
                state.castleRights = oldState.castleRights & ~YELLOW_CASTLING;
                board.kingTracker[2] = destination;
                break;
            case GREEN:
                state.castleRights = oldState.castleRights & ~GREEN_CASTLING;
                board.kingTracker[3] = destination;
                break;
            default:
                throw std::runtime_error("Unknown color given");
            }
        }
        // Update castling right when rook is moved
        else if (destPiece == ROOK || state.lastCapturedPiece == ROOK)
        {
            int rook = destination;
            if(destPiece == ROOK)
            {
                rook = loc;
            }
            switch (movingColor)
            {
            case RED:
                if (rook == L14)
                    state.castleRights = oldState.castleRights & ~RED_OO;
                else if (rook == E14)
                    state.castleRights = oldState.castleRights & ~RED_OOO;
                break;
            case BLUE:
                if (rook == B4)
                    state.castleRights = oldState.castleRights & ~BLUE_OO;
                else if (rook == B11)
                    state.castleRights = oldState.castleRights & ~BLUE_OOO;
                break;
            case YELLOW:
                if (rook == E1)
                    state.castleRights = oldState.castleRights & ~YELLOW_OO;
                else if (rook == L1)
                    state.castleRights = oldState.castleRights & ~YELLOW_OOO;
                break;
            case GREEN:
                if (rook == O4)
                    state.castleRights = oldState.castleRights & ~GREEN_OO;
                else if (rook == O11)
                    state.castleRights = oldState.castleRights & ~GREEN_OOO;
                break;
            default:
                    throw std::runtime_error("Unknown color given");
            }
        }
        // Handle en passant
        else if(destPiece == PAWN)
        {
            if (special == 2 || special == 3)
            {
                switch(movingColor)
                {
                    case RED:
                        state.zobristKey ^= zobristPieces[board_table[loc + NORTH]][PAWN-1][ctz((unsigned int)board.colorMailbox[loc + NORTH])];
                        state.dirty.add(loc + NORTH, -1, PAWN, board.colorMailbox[loc + NORTH]);
                        board.listRemove(loc + NORTH, board.colorMailbox[loc + NORTH]);
                        board.pieceMailbox[loc + NORTH] = NONE_PIECE;
                        board.colorMailbox[loc + NORTH] = NONE_COLOR;
                        break;
                    case BLUE:
                        state.zobristKey ^= zobristPieces[board_table[loc + EAST]][PAWN-1][ctz((unsigned int)board.colorMailbox[loc + EAST])];
                        state.dirty.add(loc + EAST, -1, PAWN, board.colorMailbox[loc + EAST]);
                        board.listRemove(loc + EAST, board.colorMailbox[loc + EAST]);
                        board.pieceMailbox[loc + EAST] = NONE_PIECE;
                        board.colorMailbox[loc + EAST] = NONE_COLOR;
                        break;
                    case YELLOW:
                        state.zobristKey ^= zobristPieces[board_table[loc + SOUTH]][PAWN-1][ctz((unsigned int)board.colorMailbox[loc + SOUTH])];
                        state.dirty.add(loc + SOUTH, -1, PAWN, board.colorMailbox[loc + SOUTH]);
                        board.listRemove(loc + SOUTH, board.colorMailbox[loc + SOUTH]);
                        board.pieceMailbox[loc + SOUTH] = NONE_PIECE;
                        board.colorMailbox[loc + SOUTH] = NONE_COLOR;
                        break;
                    case GREEN:
                        state.zobristKey ^= zobristPieces[board_table[loc + WEST]][PAWN-1][ctz((unsigned int)board.colorMailbox[loc + WEST])];
                        state.dirty.add(loc + WEST, -1, PAWN, board.colorMailbox[loc + WEST]);
                        board.listRemove(loc + WEST, board.colorMailbox[loc + WEST]);
                        board.pieceMailbox[loc + WEST] = NONE_PIECE;
                        board.colorMailbox[loc + WEST] = NONE_COLOR;
                        break;
                    default:
                        throw std::runtime_error("Unknown color given");
                }
            }
            else
            {
                switch(destination - loc)
                {
                    case 2*NORTH:
                        state.enpassants[0] = loc + NORTH;
                        break;
                    case 2*EAST:
                        state.enpassants[1] = loc + EAST;
                        break;
                    case 2*SOUTH:
                        state.enpassants[2] = loc + SOUTH;
                        break;
                    case 2*WEST:
                        state.enpassants[3] = loc + WEST;
                        break;
                }
            }
        }


        // Zobrist: moving piece off origin, onto destination (promotion handled automatically)
        state.zobristKey ^= zobristPieces[board_table[loc]][movingPiece-1][movingColorIdx];
        state.zobristKey ^= zobristPieces[board_table[destination]][destPiece-1][movingColorIdx];

        // Zobrist: captured piece at destination
        if (state.lastCapturedPiece != NONE_PIECE)
        {
            int capIdx = ctz((unsigned int)state.lastCapturedPieceColor);
            state.zobristKey ^= zobristPieces[board_table[destination]][state.lastCapturedPiece-1][capIdx];
        }

        // Zobrist: castling rights — XOR only bits that changed
        int changedRights = oldState.castleRights ^ state.castleRights;
        for (int i = 0; i < 8; i++)
            if (changedRights & (1 << i))
                state.zobristKey ^= zobristCastle[i];

        // Zobrist: en passant squares — XOR out old, XOR in new
        for (int player = 0; player < 4; player++)
        {
            if (oldState.enpassants[player] != -1)
                state.zobristKey ^= zobristEnPassant[player][board_table[oldState.enpassants[player]]];
            if (state.enpassants[player] != -1)
                state.zobristKey ^= zobristEnPassant[player][board_table[state.enpassants[player]]];
        }

        gameStates.push_back(state);

        // Incremental accumulator update. The board already reflects the move,
        // which is what a king-move refresh needs. The parent is guaranteed
        // valid here: move() refreshed it at the top if it was not.
        if (evalActive())
        {
            if (accStack.capacity() == accStack.size()) accStack.reserve(accStack.size() * 2);
            accStack.emplace_back();
            // Reference taken after the push, so a reallocation cannot dangle
            // it. applyFrom copies the parent in, which is the only copy of the
            // 2 KB accumulator this path makes -- it used to be copied twice.
            const nnue::Accumulators &prev = accStack[accStack.size() - 2];
            accStack.back().applyFrom(prev, board, state.dirty);
        }
    }

    void undoMove(const Move &move)
    {
        const auto loc = move.from();
        const auto destination = move.to();
        const auto pieceAtDest = board.pieceMailbox[destination];
        const auto colorAtDest = board.colorMailbox[destination];
        // Live view of the origin square, which the mover is restored onto below.
        auto &fromPiece = board.pieceMailbox[loc];
        auto &fromColor = board.colorMailbox[loc];

        // Restore the old piece position
        GameState last = gameStates.back();
        gameStates.pop_back();
        // Pop in lockstep only while the stack actually describes the ply being
        // left. An accumulator whose root is this ply is dropped entirely rather
        // than left behind describing a position we have just undone.
        if (!accStack.empty() && accBase + accStack.size() == gameStates.size() + 1)
            accStack.pop_back();
        // Piece list: walk the mover back, then restore anything it captured.
        board.listMove(destination, loc, board.colorMailbox[destination]);
        if (last.lastCapturedPiece != NONE_PIECE)
            board.listAdd(destination, last.lastCapturedPieceColor);

        if (last.lastCapturedPiece == KING)
            board.kingTracker[ctz((unsigned int)last.lastCapturedPieceColor)] = destination;

        fromPiece = board.pieceMailbox[destination];
        fromColor = board.colorMailbox[destination];
        board.pieceMailbox[destination] = last.lastCapturedPiece;
        board.colorMailbox[destination] = last.lastCapturedPieceColor;

        if (last.lastCapturedPiece != NONE_PIECE && last.lastCapturedPiece != PAWN)
            board.nonPawnPieceCount[ctz((unsigned int) last.lastCapturedPieceColor)]++;
        // Restore promotion
        if (move.special_move() == 1 || move.special_move() == 3)
        {
            fromPiece = PAWN;
            board.nonPawnPieceCount[ctz((unsigned int)fromColor)]--;
        }

        // Restore rook when castling
        if (move.special_move() == 4)
        {
            switch (fromColor)
            {
            case RED:
                if (destination == K14)
                {
                    board.listMove(J14, L14, RED);
                    board.pieceMailbox[L14] = ROOK;
                    board.colorMailbox[L14] = RED;
                    board.pieceMailbox[J14] = NONE_PIECE;
                    board.colorMailbox[J14] = NONE_COLOR;
                }
                else if (destination == G14)
                {
                    board.listMove(H14, E14, RED);
                    board.pieceMailbox[E14] = ROOK;
                    board.colorMailbox[E14] = RED;
                    board.pieceMailbox[H14] = NONE_PIECE;
                    board.colorMailbox[H14] = NONE_COLOR;
                }
                break;
            case BLUE:
                if (destination == B6)
                {
                    board.listMove(B7, B4, BLUE);
                    board.pieceMailbox[B4] = ROOK;
                    board.colorMailbox[B4] = BLUE;
                    board.pieceMailbox[B7] = NONE_PIECE;
                    board.colorMailbox[B7] = NONE_COLOR;
                }
                else if (destination == B10)
                {
                    board.listMove(B9, B11, BLUE);
                    board.pieceMailbox[B11] = ROOK;
                    board.colorMailbox[B11] = BLUE;
                    board.pieceMailbox[B9] = NONE_PIECE;
                    board.colorMailbox[B9] = NONE_COLOR;
                }
                break;
            case YELLOW:
                if (destination == F1)
                {
                    board.listMove(G1, E1, YELLOW);
                    board.pieceMailbox[E1] = ROOK;
                    board.colorMailbox[E1] = YELLOW;
                    board.pieceMailbox[G1] = NONE_PIECE;
                    board.colorMailbox[G1] = NONE_COLOR;
                }
                else if (destination == J1)
                {
                    board.listMove(I1, L1, YELLOW);
                    board.pieceMailbox[L1] = ROOK;
                    board.colorMailbox[L1] = YELLOW;
                    board.pieceMailbox[I1] = NONE_PIECE;
                    board.colorMailbox[I1] = NONE_COLOR;
                }
                break;
            case GREEN:
                if (destination == O5)
                {
                    board.listMove(O6, O4, GREEN);
                    board.pieceMailbox[O4] = ROOK;
                    board.colorMailbox[O4] = GREEN;
                    board.pieceMailbox[O6] = NONE_PIECE;
                    board.colorMailbox[O6] = NONE_COLOR;
                }
                else if (destination == O9)
                {
                    board.listMove(O8, O11, GREEN);
                    board.pieceMailbox[O11] = ROOK;
                    board.colorMailbox[O11] = GREEN;
                    board.pieceMailbox[O8] = NONE_PIECE;
                    board.colorMailbox[O8] = NONE_COLOR;
                }
                break;
            
            default:
                throw std::runtime_error("Unknown color given");
            }
        }

        // Restore en passant
        else if(move.special_move() == 2 || move.special_move() == 3)
        {
            switch(fromColor)
            {
                case RED:
                    if(destination == loc + NORTH + WEST)
                    {
                        board.pieceMailbox[loc + NORTH] = PAWN;
                        board.colorMailbox[loc + NORTH] = BLUE;
                        board.listAdd(loc + NORTH, BLUE);
                    }
                    else
                    {
                       board.pieceMailbox[loc + NORTH] = PAWN;
                       board.colorMailbox[loc + NORTH] = GREEN;
                       board.listAdd(loc + NORTH, GREEN);
                    }
                    break;
                case BLUE:
                    if(destination == loc + EAST + NORTH)
                    {
                        board.pieceMailbox[loc + EAST] = PAWN;
                        board.colorMailbox[loc + EAST] = YELLOW;
                        board.listAdd(loc + EAST, YELLOW);
                    }
                    else
                    {
                       board.pieceMailbox[loc + EAST] = PAWN;
                       board.colorMailbox[loc + EAST] = RED;
                       board.listAdd(loc + EAST, RED);
                    }
                    break;
                case YELLOW:
                    if(destination == loc + SOUTH + WEST)
                    {
                        board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = BLUE;
                        board.listAdd(loc + SOUTH, BLUE);
                    }
                    else
                    {
                       board.pieceMailbox[loc + SOUTH] = PAWN;
                        board.colorMailbox[loc + SOUTH] = GREEN;
                        board.listAdd(loc + SOUTH, GREEN);
                    }
                    break;
                case GREEN:
                    if(destination == loc + WEST + NORTH)
                    {
                        board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = YELLOW;
                        board.listAdd(loc + WEST, YELLOW);
                    }
                    else
                    {
                       board.pieceMailbox[loc + WEST] = PAWN;
                        board.colorMailbox[loc + WEST] = RED;
                        board.listAdd(loc + WEST, RED);
                    }
                    break;
                default:
                    throw std::runtime_error("Unknown color given");
            }
        }

        if (fromPiece == KING)
            board.kingTracker[ctz((unsigned int)(fromColor))] = loc;

    }
};
