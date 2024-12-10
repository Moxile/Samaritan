#include <iostream>
#include <unistd.h>

#include "uci.h"

int main()
{
   std::string fen = "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
                     "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
                     "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
                     "14/"
                     "bR,bP,10,gP,gR/"
                     "bN,bP,10,gP,gN/"
                     "bB,bP,10,gP,gB/"
                     "bQ,bP,10,gP,gK/"
                     "bK,bP,10,gP,gQ/"
                     "bB,bP,10,gP,gB/"
                     "bN,bP,10,gP,gN/"
                     "bR,bP,10,gP,gR/"
                     "14/"
                     "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
                     "3,rR,rN,rB,rQ,rK,rB,rN,rR,3";

   Position pos;
   UCI uci = UCI(pos);
   uci.uci_loop();
#if 0
   pos.loadFEN(fen);

//#if 0

   search(pos, 5);

//#else

   std::cout << "Choose Gamemode (newgame)" << std::endl;
   std::string input;
   std::cin >> input;
   if (input == "newgame")
   {
      std::cout << "Which color will the engine play (RY or BG)" << std::endl;
      std::cin >> input;
      PieceColor engine;

      if (input == "RY")
         engine = TEAM_RY;
      else
      {
         engine = TEAM_BG;
      }
      while (MoveList(pos).size() != 0)
      {
         if (getTeam(pos.curTurn) == engine)
         {
            search(pos, 4);
            pos.move(pos.bestMove);
            pos.print();
         }
         else
         {
            std::cout << "Please Move" << std::endl;
            std::cin >> input;
            char m1, m2;
            int i1, i2;
            std::sscanf(input.data(), "%c%d%c%d", &m1, &i1, &m2, &i2);
            int from = (m1 - '`') + (14 - i1) * 16;
            int to = (m2 - '`') + ((14 - i2) * 16);
            std::cout << "M " << from << " (" << m1 << "," << i1 << ")" << " " << to << std::endl;
            if (MoveList(pos).contains(Move(to, from, 0, 0)))
            {
               pos.move(Move(to, from, 0, 0));
               pos.print();
            }
            else
            {
               std::cout << "Not a legal move! Try again" << std::endl;
               continue;
            }
         }
      }
      if (pos.isSquareAttacked(pos.kingTracker[__builtin_ctz(pos.curTurn)], pos.curTurn, getTeam(pos.curTurn)))
      {
         std::cout << "Game Over" << std::endl;
      }
      else
      {
         std::cout << "Draw" << std::endl;
      }

      return 0;
   }

   // engine plays game against itself
   int total_moves = 100;
   int start_wait_at = 0;
   if (MoveList(pos).size() == 0)
   {
      std::cout << "Game Over" << std::endl;
   }
   for (int i = 1; i <= total_moves; i++)
   {
      //auto start = std::chrono::steady_clock::now();
      search(pos, 1);
      //std::cout << "Player Turn: " << pos.curTurn << " Move: " << i << " score: " << search(pos, 1) << " best Move: " << pos.bestMove.toUCI() << " static eval: " << evaluate(pos) << std::endl;
      //std::cout << "Took(ms)=" << since(start).count() << std::endl;
      pos.move(pos.bestMove);
      if (start_wait_at < i)
      {
         pos.print();
         sleep(0);
      }
      if (MoveList(pos).size() == 0)
      {
         std::cout << "Game Over" << std::endl;
         break;
      }
   }
#endif
   return 0;
}