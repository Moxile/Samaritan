#pragma once

#include "nnue/dense.h"
#include "nnue/accumulator.h"

class NNUE
{
    public:
        DenseLayer<float> hidden;
        DenseLayer<float> output;
        int multiplier;
        Accumulator accumulator;
        int evaluation;
        std::vector<float> hidden_output_;

        NNUE(size_t hiddensize) : 
            hidden(DenseLayer<float>(hiddensize, accumulator.FEATURE_COUNT)), 
            output(DenseLayer<float>(1, hiddensize)), multiplier(1000)
        {
            accumulator = Accumulator();
            accumulator.reset();
        }

        void init_eval()
        {
            // get the eval
            auto input = accumulator.input;
            auto hidden_output = hidden.forward(input);
            hidden_output_ = hidden_output;
            for(size_t i = 0; i < hidden_output.size(); ++i) {
                hidden_output[i] = ReLu(hidden_output[i]);
            }
            auto output_value = output.forward(hidden_output);
            evaluation = static_cast<int>(output_value[0] * multiplier);
        }

        void incremental_update()
        {
            for(const auto &change : accumulator.changes)
            {
                if(accumulator.input[change] == 0)
                {
                    accumulator.input[change] = 1;
                    hidden.single_forward_add(hidden_output_, change);
                }
                else
                {
                    accumulator.input[change] = 0;
                    hidden.single_forward_rem(hidden_output_, change);
                }
            }
            accumulator.changes.clear();

            auto output_value = output.forward(hidden_output_);
            evaluation = static_cast<int>(output_value[0] * multiplier);
        }
};


