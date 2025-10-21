/*
 * This file implements a basic (gshare like) indirect target branch prediction.
 */

#include "ooo_cpu.h"

struct target_table_m {
  uint64_t target;
  uint64_t bb_size;
  uint64_t lru;
};

template <uint32_t SETS, uint32_t WAYS>

class TargetTable {
private:

  target_table_m m_tt[SETS][WAYS];
  uint64_t lru_counter;

  uint64_t set_index(uint64_t target) {
    return ((target >> 2) & (SETS - 1));
  }

  target_table_m* find_entry(uint64_t target) {
    uint64_t set = set_index(target);
    for (uint32_t i = 0; i < WAYS; i++) {
      if (m_tt[set][i].target == target) {
        return &(m_tt[set][i]);
      }
    }
    return NULL;
  }

  target_table_m* get_lru_entry(uint64_t set) {
    uint32_t lru_way = 0;
    uint64_t lru_value = m_tt[set][lru_way].lru;
    for (uint32_t i = 0; i < WAYS; i++) {
      if (m_tt[set][i].lru < lru_value) {
        lru_way = i;
        lru_value = m_tt[set][lru_way].lru;
      }
    }
    return &(m_tt[set][lru_way]);
  }

  void update_lru(target_table_m* btb_entry) {
    btb_entry->lru = lru_counter;
    lru_counter++;
  }

public:

  void initialize() {
    for (uint32_t i = 0; i < SETS; i++) {
      for (uint32_t j = 0; j < WAYS; j++) {
        m_tt[i][j].target = 0;
        m_tt[i][j].bb_size = 0;
        m_tt[i][j].lru = 0;
      }
    }
  }

  uint64_t get_bb_size(uint64_t target) {
    auto entry = find_entry(target);
    if (entry != NULL) {
      update_lru(entry);
      return entry->bb_size;
    }
    return 0;
  }

  void m_update_bb_size(uint64_t target, uint64_t bb_size) {
    auto entry = find_entry(target);
    if (entry == NULL) {
      auto victim = get_lru_entry(set_index(target));
      victim->target = target;
      victim->bb_size = bb_size;
      update_lru(victim);
    }
  }

};
