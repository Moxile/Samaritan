#pragma once

#include "nnue/dense.h"
#include "nnue/accumulator.h"
#include "position.h"

class NNUE
{
    public:
        DenseLayer<float> hidden;
        DenseLayer<float> output;
        int multiplier;
        Accumulator accumulator;
        int evaluation;
        std::vector<float> hidden_output_;

        NNUE(size_t hiddensize, const Position &pos)
        {
            hidden = DenseLayer<float>(hiddensize, accumulator.FEATURE_COUT);
            output = DenseLayer<float>(1, hiddensize);
        }

        void init_eval(const Position &pos)
        {
            // init accumulator
            for (int i = 0; i < 225; i++)
            {
                if (!isInvalidLocation(i))
                {
                    accumulator.input[accumulator.get_board_feat(i, pos.board.pieceMailbox[i], pos.board.colorMailbox[i])] = 1;
                }
            }

            accumulator.input[accumulator.get_turn_fen(pos.gameStates.back().curTurn)] = 1;

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
}


