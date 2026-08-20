// Builds corpus positions for the rule categories the random-play corpus misses:
// castling, promotion, en passant, stalemate, king capture.
// Each is self-checked: it must actually generate the move type it claims.
#include "utility.h"
#include "movegen.h"
#include "test_helpers.h"
#include <cstdio>
#include <random>
#include <vector>
#include <string>

static const std::vector<Piece> KINGS = {
    {216, KING, RED}, {113, KING, BLUE}, {7, KING, YELLOW}, {110, KING, GREEN},
};

static int countType(Position& p, int special){
    MoveList m(p); int n=0;
    for(auto&x:m) if(x.special_move()==special) n++;
    return n;
}
static int countPromo(Position& p){
    MoveList m(p); int n=0;
    for(auto&x:m) if(x.promotion()!=0) n++;
    return n;
}
static void emit(const char* label, Position& p, int pieces, const char* want, int got){
    if(got==0){ fprintf(stderr,"!! %s produced no %s moves -- skipped\n",label,want); return; }
    printf("%s\t%d\t%zu\t%s\n", label, pieces, MoveList(p).size(), positionToFEN(p).c_str());
}
static int npieces(Position&p){int n=0;for(int i=0;i<224;i++)if(p.board.pieceMailbox[i]!=NONE_PIECE)n++;return n;}

int main(){
    initZobrist();
    Position p(false);

    // ---- castling: king + both rooks on home squares, paths clear ----
    struct C { const char* lab; PieceColor col; int king, rookOO, rookOOO, rights; };
    const C castles[] = {
        {"cas_r", RED,    216, 219, 212, RED_CASTLING},
        {"cas_b", BLUE,   113,  49, 161, BLUE_CASTLING},
        {"cas_y", YELLOW,   7,   4,  11, YELLOW_CASTLING},
        {"cas_g", GREEN,  110,  62, 174, GREEN_CASTLING},
    };
    for (const auto& c : castles) {
        std::vector<Piece> ps;
        for (const auto& k : KINGS) if (k.color != c.col) ps.push_back(k);
        ps.push_back({c.king, KING, c.col});
        ps.push_back({c.rookOO, ROOK, c.col});
        ps.push_back({c.rookOOO, ROOK, c.col});
        setupPosition(p, c.col, ps, c.rights);
        emit(c.lab, p, npieces(p), "castling", countType(p, 4));
    }

    // ---- promotion: a pawn one push from promoting, plus an enemy to capture ----
    struct P { const char* lab; PieceColor col; int pawn, victim; PieceColor vcol; };
    const P promos[] = {
        {"pro_r", RED,     71,  54, BLUE},    // red  pushes north from row 4
        {"pro_b", BLUE,   122, 107, RED},     // blue pushes east  from col 10
        {"pro_y", YELLOW, 151, 168, GREEN},   // yellow pushes south from row 9
        {"pro_g", GREEN,  117, 100, RED},     // green pushes west  from col 5
    };
    for (const auto& q : promos) {
        std::vector<Piece> ps(KINGS.begin(), KINGS.end());
        ps.push_back({q.pawn, PAWN, q.col});
        ps.push_back({q.victim, KNIGHT, q.vcol});
        setupPosition(p, q.col, ps);
        emit(q.lab, p, npieces(p), "promotion", countPromo(p));
    }

    // ---- en passant: play a real double push, then hand the move to the capturer ----
    struct E { const char* lab; PieceColor mover; int from, to; PieceColor capt; int cpawn; };
    const E eps[] = {
        {"ep_r", RED,    200, 168, GREEN,  169},   // red e2e4 style, green captures
        {"ep_b", BLUE,    98, 100, RED,    116},
        {"ep_y", YELLOW,  23,  55, BLUE,    54},
        {"ep_g", GREEN,  109, 107, YELLOW,  91},
    };
    for (const auto& e : eps) {
        std::vector<Piece> ps(KINGS.begin(), KINGS.end());
        ps.push_back({e.from, PAWN, e.mover});
        ps.push_back({e.cpawn, PAWN, e.capt});
        setupPosition(p, e.mover, ps);
        MoveList probe(p);
        Move dbl(e.to, e.from, 0, 0);
        if (!probe.contains(dbl)) { fprintf(stderr,"!! %s double push %d->%d not legal\n", e.lab, e.from, e.to); continue; }
        p.move(dbl);
        // hand the turn to the capturing colour
        while (p.gameStates.back().curTurn != e.capt) p.makeNullMove();
        emit(e.lab, p, npieces(p), "en passant", countType(p, 2) + countType(p, 3));
    }

    // ---- king capture: in check, but an enemy king is takeable ----
    {
        std::vector<Piece> ps = {
            {216, KING, RED}, {200, ROOK, BLUE}, {113, KING, BLUE},
            {7, KING, YELLOW}, {110, KING, GREEN}, {109, QUEEN, RED},
        };
        setupPosition(p, RED, ps);
        MoveList m(p); int kc=0; for(auto&x:m) if(p.board.pieceMailbox[x.to()]==KING) kc++;
        emit("kingcap", p, npieces(p), "king-capture", kc);
    }

    // ---- stalemate: search random games for a position whose child is stalemate ----
    {
        static const std::string START =
          "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
          "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/3,yP,yP,yP,yP,yP,yP,yP,yP,3/14/"
          "bR,bP,10,gP,gR/bN,bP,10,gP,gN/bB,bP,10,gP,gB/bQ,bP,10,gP,gK/"
          "bK,bP,10,gP,gQ/bB,bP,10,gP,gB/bN,bP,10,gP,gN/bR,bP,10,gP,gR/14/"
          "3,rP,rP,rP,rP,rP,rP,rP,rP,3/3,rR,rN,rB,rQ,rK,rB,rN,rR,3";
        int found = 0;
        for (unsigned seed = 1; seed <= 3000 && found < 1; ++seed) {
            loadFEN(p, START);
            std::mt19937 rng(seed);
            for (int ply = 0; ply < 400; ++ply) {
                MoveList m(p);
                if (m.size() == 0) break;
                // does any child stalemate the next player?
                for (auto& x : m) {
                    p.move(x);
                    bool term = MoveList(p).size() == 0;
                    bool chk  = inCheck(p, p.gameStates.back().curTurn);
                    p.undoMove(x);
                    if (term && !chk) {
                        emit("stale", p, npieces(p), "stalemate-parent", 1);
                        found = 1; break;
                    }
                }
                if (found) break;
                std::vector<Move> pick;
                for (auto& x : m) if (p.board.pieceMailbox[x.to()] != KING) pick.push_back(x);
                if (pick.empty()) break;
                p.move(pick[rng() % pick.size()]);
            }
        }
        if (!found) fprintf(stderr, "!! no stalemate parent found\n");
    }
    return 0;
}
