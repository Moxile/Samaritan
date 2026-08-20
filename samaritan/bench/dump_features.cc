#include "utility.h"
#include "movegen.h"
#include "nnue/features.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
int main(int argc,char**argv){
  initZobrist(); Position pos(false); loadFEN(pos, argv[1]);
  // dump the raw position for the python side: colour*6+type , dense square
  printf("PIECES");
  for(int loc=0; loc<224; ++loc){
    if(pos.board.pieceMailbox[loc]==NONE_PIECE) continue;
    const int c = ctz((unsigned)pos.board.colorMailbox[loc]);
    printf(" %d:%d", c*6 + nnue::trainerPieceType(pos.board.pieceMailbox[loc]),
           (int)nnue::MAILBOX_TO_DENSE[loc]);
  }
  printf("\n");
  for(int p=0;p<4;p++){
    std::vector<int> f;
    nnue::emitFeatures(pos.board, p, [&](int i){ f.push_back(i); });
    std::sort(f.begin(), f.end());
    printf("P%d", p);
    for(int i : f) printf(" %d", i);
    printf("\n");
  }
}
