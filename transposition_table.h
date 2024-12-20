#include <atomic>
#include <cstdint>
#include <optional>

#include "position.h"

enum ScoreBound {
  EXACT = 0, LOWER_BOUND = 1, UPPER_BOUND = 2,
};

struct HashTableEntry {
  int64_t key;
  int depth;
  Move move;
  int score;
  ScoreBound bound;
  bool is_pv;
};

class TranspositionTable {
 public:
   TranspositionTable(size_t table_size);

   const HashTableEntry* Get(int64_t key);

   void Save(int64_t key, int depth, Move move,
             int score, ScoreBound bound, bool is_pv);

  ~TranspositionTable() {
    if (hash_table_ != nullptr) {
      free(hash_table_);
    }
  }

 private:
  HashTableEntry* hash_table_ = nullptr;
  size_t table_size_ = 0;
};
