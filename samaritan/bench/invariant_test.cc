#include "utility.h"
#include "movegen.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
static long nodes=0, fails=0;
static void walk(int d, Position&p){
  nodes++;
  std::string why;
  if(!p.isConsistent(&why)){ if(fails<5) printf("  INCONSISTENT (%s): %s\n", why.c_str(), positionToFEN(p).c_str()); fails++; }
  if(d==0) return;
  MoveList m(p);
  for(Move x:m){ p.move(x); walk(d-1,p); p.undoMove(x);
                 std::string w2;
                 if(!p.isConsistent(&w2)){ if(fails<5) printf("  BAD AFTER UNDO of %s (%s)\n", x.toUCI().c_str(), w2.c_str()); fails++; } }
}
int main(int argc,char**argv){
  initZobrist(); Position pos(false);
  std::ifstream in(argv[1]); std::string line; int depth=argc>2?atoi(argv[2]):4;
  while(std::getline(in,line)){
    if(line.empty()||line[0]=='#') continue;
    std::stringstream ss(line); std::string lab,pc,mv,fen;
    std::getline(ss,lab,'\t');std::getline(ss,pc,'\t');std::getline(ss,mv,'\t');std::getline(ss,fen);
    loadFEN(pos,fen); long b=fails; walk(depth,pos);
    printf("%-6s %s\n", lab.c_str(), fails==b?"ok":"FAILED");
  }
  printf("\n%ld nodes, %ld inconsistencies\n", nodes, fails);
  return fails?1:0;
}
