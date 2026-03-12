#pragma once

#include "nnue/dense.h"
#include "nnue/accumulator.h"

class NNUE
{
    public:
        DenseLayer<float> hidden;
        DenseLayer<float> output;
        int multiplier;
        Accumulator accumulators[4];
        int evaluation;
        std::vector<float> hidden_output_[4];

        NNUE(size_t hiddensize) : 
            hidden(DenseLayer<float>(hiddensize, Accumulator::FEATURE_COUNT)), 
            output(DenseLayer<float>(1, hiddensize)), multiplier(1000)
        {
            accumulators[0] = Accumulator(static_cast<PieceColor>(1));
            accumulators[1] = Accumulator(static_cast<PieceColor>(2));
            accumulators[2] = Accumulator(static_cast<PieceColor>(4));
            accumulators[3] = Accumulator(static_cast<PieceColor>(8));
            for (size_t i = 0; i < 4; ++i)
                accumulators[i].reset();
        }

        void init_eval(PieceColor turn)
        {
            // get the eval
            for(auto &accumulator : accumulators)
            {
                auto input = accumulator.input;
                auto hidden_outputs = hidden.forward(input);
                for(size_t i = 0; i < hidden_outputs.size(); ++i) {
                    hidden_outputs[i] = ReLu(hidden_outputs[i]);
                }
                hidden_output_[accumulator.perspective] = hidden_outputs;
            }
            auto output_value = output.forward(hidden_output_[__builtin_ctz((unsigned int)turn)]);
            evaluation = static_cast<int>(output_value[0] * multiplier);
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
                        hidden.single_forward_add(hidden_output_[accumulator.perspective], change);
                    }
                    else
                    {
                        accumulator.input[change] = 0;
                        hidden.single_forward_rem(hidden_output_[accumulator.perspective], change);
                    }
                }
                accumulator.changes.clear();
            }

            auto output_value = output.forward(hidden_output_[__builtin_ctz((unsigned int)turn)]);
            evaluation = static_cast<int>(output_value[0] * multiplier);
        }
};


