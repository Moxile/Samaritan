#include "utility.h"
#include "movegen.h"
#include "nnue/features.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
int main(int argc,char**argv){
  initZobrist(); Position pos(false);
  std::ifstream in(argv[1]); std::string line;
  while(std::getline(in,line)){
    if(line.empty()||line[0]=='#') continue;
    std::stringstream ss(line); std::string lab,pc,mv,fen;
    std::getline(ss,lab,'\t');std::getline(ss,pc,'\t');std::getline(ss,mv,'\t');std::getline(ss,fen);
    loadFEN(pos,fen);
    printf("POS %s %d\n", lab.c_str(), ctz((unsigned)pos.gameStates.back().curTurn));
    for(int p=0;p<4;p++){ std::vector<int> f;
      nnue::emitFeatures(pos.board,p,[&](int i){f.push_back(i);});
      printf("P%d",p); for(int i:f) printf(" %d",i); printf("\n"); }
  }
}
