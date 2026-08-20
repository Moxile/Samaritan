#include "utility.h"
#include "movegen.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
static uint64_t caps,eps,castles,promos,kingcaps,mates,stales,nodes;
static void walk(int d, Position&p){
  MoveList m(p);
  nodes++;
  if(m.size()==0){ if(inCheck(p,p.gameStates.back().curTurn)) mates++; else stales++; return; }
  // Count at every ply, not just the leaves: a feature available at the root
  // may well be gone by the deepest ply.
  for(Move x:m){
    if(p.board.pieceMailbox[x.to()]!=NONE_PIECE) caps++;
    if(p.board.pieceMailbox[x.to()]==KING) kingcaps++;
    int s=x.special_move();
    if(s==2||s==3) eps++;
    if(s==4) castles++;
    if(s==1||s==3) promos++;
    if(d>1){ p.move(x); walk(d-1,p); p.undoMove(x); }
  }
}
int main(int argc,char**argv){
  initZobrist(); Position pos(false);
  std::ifstream in(argv[1]); std::string line; int depth=argc>2?atoi(argv[2]):4;
  printf("%-8s %10s %10s %8s %8s %8s %8s %10s\n","pos","nodes","caps","ep","castle","promo","kingcap","mate/stale");
  uint64_t Tc=0,Te=0,Tk=0,Tp=0,Tm=0,Ts=0,Tkc=0;
  while(std::getline(in,line)){
    if(line.empty()||line[0]=='#') continue;
    std::stringstream ss(line); std::string lab,pc,mv,fen;
    std::getline(ss,lab,'\t');std::getline(ss,pc,'\t');std::getline(ss,mv,'\t');std::getline(ss,fen);
    caps=eps=castles=promos=kingcaps=mates=stales=nodes=0;
    loadFEN(pos,fen); walk(depth,pos);
    printf("%-8s %10llu %10llu %8llu %8llu %8llu %8llu %6llu/%llu\n",lab.c_str(),
      (unsigned long long)nodes,(unsigned long long)caps,(unsigned long long)eps,
      (unsigned long long)castles,(unsigned long long)promos,(unsigned long long)kingcaps,
      (unsigned long long)mates,(unsigned long long)stales);
    Tc+=caps;Te+=eps;Tk+=castles;Tp+=promos;Tm+=mates;Ts+=stales;Tkc+=kingcaps;
  }
  printf("%-8s %10s %10llu %8llu %8llu %8llu %8llu %6llu/%llu\n","TOTAL","-",
    (unsigned long long)Tc,(unsigned long long)Te,(unsigned long long)Tk,
    (unsigned long long)Tp,(unsigned long long)Tkc,(unsigned long long)Tm,(unsigned long long)Ts);
}
