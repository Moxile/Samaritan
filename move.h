#pragma once
#include <cstdint>
#include <string>
#include <sstream>
//#include <sys/types.h>

//constexpr std::string ranks[14] = {"14", "13", "12", "11", "10", "9", "8" ,"7", "6", "5", "4", "3", "2", "1"};
constexpr int ranks[14] = {14, 13, 12, 11, 10, 9, 8 ,7, 6, 5, 4, 3, 2, 1};
constexpr char files[14] = {'a',  'b',  'c',  'd',  'e',  'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'};

class Move
{
    // A move needs 32 bits to be stored
    //
    // bit  0- 8: destination square (from 0 to 63)
    // bit  8-16: origin square (from 0 to 63)
    // bit 17-24: promotion piece type
    // bit 25-32: special move flag: promotion (1), en passant (2), en passant and promotion(3)   0 -> no special move
    // NOTE: every information has 8 bits reserved as a layout

public:
    enum Field
    {
        DESTINATION = 0,
        ORIGIN = 8,
        PROMOTION = 16,
        SPECIAL = 24
    };

    explicit Move() = default;

    Move(int destination, int origin, int promotion_type, int special_move)
    {
        data = destination;
        data |= origin << 8;
        data |= promotion_type << 16;
        data |= special_move << 24;
    }

    const std::string toUCI() const
    {
        std::stringstream uci;
        uci << files[(from() % 16)-1] << ranks[from() / 16];
        uci << files[(to() % 16)-1] << ranks[to() / 16];
        return uci.str();
    }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr int from() const { return (data >> 8) & 0xFF; }

    constexpr int to() const { return data & 0xFF; }

    constexpr int promotion() const { return (data >> 16) & 0xFF; }

    constexpr int special_move() const { return (data >> 24) & 0xFFl; }

    constexpr uint32_t getRawData() const { return data; }

    template <Field F>
    constexpr int get() { return (data >> F) & 0xFF; }

protected:
    uint32_t data;
};

struct ExtMove : public Move
{
    int value;
    MoveType gen_type;

    void operator=(Move m) { data = m.getRawData(); }

    // Inhibit unwanted implicit conversions to Move
    // with an ambiguity that yields to a compile error.
    operator float() const = delete;
};

inline bool operator<(const ExtMove &f, const ExtMove &s) { return f.value < s.value; }
inline bool operator>(const ExtMove &f, const ExtMove &s) { return f.value > s.value; }