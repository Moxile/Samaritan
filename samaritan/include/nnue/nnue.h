#pragma once

#include <fstream>
#include <cmath>

#include "nnue/dense.h"
#include "nnue/accumulator.h"

class NNUE
{
    public:
        AccumulatorLayer hidden;
        OutputLayer output;
        Accumulator accumulators[4];
        int evaluation;
        AlignedArr16 hidden_output_[4];

        void loadWeights(const std::string& path) {
            std::ifstream f(path, std::ios::binary);
            if (!f) throw std::runtime_error("Cannot open weights: " + path);

            char magic[4]; f.read(magic, 4);
            if (std::string(magic, 4) != "NNUE") throw std::runtime_error("Bad magic");

            int32_t version, hs;
            f.read(reinterpret_cast<char*>(&version), 4);
            f.read(reinterpret_cast<char*>(&hs), 4);

            if (hs != HIDDEN_SIZE)
                throw std::runtime_error("Model hidden size mismatch");

            size_t l1_w = Accumulator::FEATURE_COUNT * hs;
            std::vector<float> w(l1_w), b(hs), ow(hs);
            float ob;

            f.read(reinterpret_cast<char*>(w.data()),  l1_w * 4);
            f.read(reinterpret_cast<char*>(b.data()),  hs   * 4);
            f.read(reinterpret_cast<char*>(ow.data()), hs   * 4);
            f.read(reinterpret_cast<char*>(&ob),       4);

            hidden.loadFromFloats(w.data(), b.data());
            output.loadFromFloats(ow.data(), ob);
        }

        NNUE() :
            hidden(AccumulatorLayer(Accumulator::FEATURE_COUNT)),
            output(OutputLayer())
        {
            loadWeights("./include/nnue/models/model.bin");

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


