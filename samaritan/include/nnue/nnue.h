#pragma once

#include "nnue/dense.h"
#include "nnue/accumulator.h"

class NNUE
{
    public:
        AccumulatorLayer hidden;
        OutputLayer output;
        int multiplier;
        Accumulator accumulators[4];
        int evaluation;
        std::vector<int32_t> hidden_output_[4];

        NNUE(size_t hiddensize) : 
            hidden(AccumulatorLayer()),
            output(OutputLayer()), multiplier(1000)
        {
            accumulators[0] = Accumulator(static_cast<PieceColor>(1));
            accumulators[1] = Accumulator(static_cast<PieceColor>(2));
            accumulators[2] = Accumulator(static_cast<PieceColor>(4));
            accumulators[3] = Accumulator(static_cast<PieceColor>(8));
            for (size_t i = 0; i < 4; ++i)
            {
                hidden_output_[i].resize(hiddensize, 0);
                accumulators[i].reset();
            }
        }

        void init_eval(PieceColor turn)
        {
            // get the eval
            for(auto &accumulator : accumulators)
            {
                hidden.refresh(hidden_output_[accumulator.perspective], accumulator.input);
            }
            evaluation = output.forward(hidden_output_[__builtin_ctz((unsigned int)turn)]);
        }

        void incremental_update(PieceColor turn)
        {
            for(auto &accumulator : accumulators)
            {
                for(const auto &change : accumulator.changes)
                {
                    if(accumulator.input[change] == 0)
                    {
                        accumulator.input[change] = 1;
                        hidden.add(hidden_output_[accumulator.perspective], change);
                    }
                    else
                    {
                        accumulator.input[change] = 0;
                        hidden.rem(hidden_output_[accumulator.perspective], change);
                    }
                }
                accumulator.changes.clear();
            }
            
            evaluation = output.forward(hidden_output_[__builtin_ctz((unsigned int)turn)]);
        }
};


