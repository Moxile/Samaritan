#include "engine.h"
#include "perft.h"
#include "utility.h"

namespace samaritan
{
    void Engine::launch()
    {
        Position pos;
        loadFEN(pos, "R-0,0,0,0-1,1,1,1-1,1,1,1-0,0,0,0-0-"
                     "3,yR,yN,yB,yK,yQ,yB,yN,yR,3/"
                     "3,yP,yP,yP,yP,yP,yP,yP,yP,3/"
                     "14/"
                     "bR,bP,10,gP,gR/"
                     "bN,bP,10,gP,gN/"
                     "bB,bPP,10,gP,gB/"
                     "bQ,bP,10,gP,gK/"
                     "bK,bP,10,gP,gQ/"
                     "bB,bP,10,gP,gB/"
                     "bN,bP,10,gP,gN/"
                     "bR,bP,10,gP,gR/"
                     "14/"
                     "3,rP,rP,rP,rP,rP,rP,rP,rP,3/"
                     "3,rR,rN,rB,rQ,rK,rB,rN,rR,3");
    
        perft(7, pos);
    }
}