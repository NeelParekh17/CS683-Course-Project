/*
 * This file implements a tagged (gshare like) associative indirect target branch prediction.
 */

#include "ooo_cpu.h"

template<uint32_t SETS, uint32_t WAYS>
class TaggedIndirect {

private:

  struct ENTRY {
    uint64_t ip_tag;
    uint64_t target;
    uint64_t lru;
    uint64_t taken_bb_size;
    uint64_t not_taken_bb_size;
    uint8_t conf = 0;
  };

  ENTRY indirect[SETS][WAYS];
  uint64_t lru_counter;
  uint64_t conditional_history;
  uint64_t last_conditional_history;

  ENTRY spec_indirect[SETS][WAYS];
  uint64_t spec_lru_counter;
  uint64_t spec_conditional_history;

  bool speculative_state = false;

  uint64_t set_index(uint64_t ip) {
    return (((ip >> 2) ^ (conditional_history)) & (SETS - 1));
  }

  uint64_t set_index_last(uint64_t ip) {
    return (((ip >> 2) ^ (last_conditional_history)) & (SETS - 1));
  }

  ENTRY* find_entry_last(uint64_t ip) {
    uint64_t set = set_index_last(ip);
    for (uint32_t i = 0; i < WAYS; i++) {
      if (indirect[set][i].ip_tag == ip) {
        return &(indirect[set][i]);
      }
    }
    return NULL;
  }

  ENTRY* find_entry(uint64_t ip) {
    uint64_t set = set_index(ip);
    for (uint32_t i = 0; i < WAYS; i++) {
      if (indirect[set][i].ip_tag == ip) {
        return &(indirect[set][i]);
      }
    }
    return NULL;
  }

  ENTRY* get_lru_entry(uint64_t set) {
    uint32_t lru_way = 0;
    uint64_t lru_value = indirect[set][lru_way].lru;
    for (uint32_t i = 0; i < WAYS; i++) {
      if (indirect[set][i].lru < lru_value) {
        lru_way = i;
        lru_value = indirect[set][lru_way].lru;
      }
    }
    return &(indirect[set][lru_way]);
  }

  void update_lru(ENTRY* indirect) {
    indirect->lru = lru_counter;
    lru_counter++;
  }

public:

  void initialize() {
    std::cout << "Indirect buffer sets: " << SETS << " ways: " << WAYS << std::endl;
    for (uint32_t i = 0; i < SETS; i++) {
      for (uint32_t j = 0; j < WAYS; j++) {
        indirect[i][j].ip_tag = 0;
        indirect[i][j].target = 0;
        indirect[i][j].lru = 0;
        indirect[i][j].taken_bb_size = 0;
        indirect[i][j].not_taken_bb_size = 0;
        indirect[i][j].conf = 0;

        spec_indirect[i][j].ip_tag = 0;
        spec_indirect[i][j].target = 0;
        spec_indirect[i][j].lru = 0;
        spec_indirect[i][j].taken_bb_size = 0;
        spec_indirect[i][j].not_taken_bb_size = 0;
      }
    }
    lru_counter = 0;
    conditional_history = 0;
    last_conditional_history = 0;

    spec_lru_counter = 0;
    spec_conditional_history = 0;

    speculative_state = false;
  }

  std::pair<uint64_t, uint64_t> get_bb_size(uint64_t ip) {
    auto ind_entry = find_entry(ip);
    if (ind_entry == NULL) {
      // no prediction for this IP
      return  std::pair{ 0, 0 };
    }
    return std::pair{ ind_entry->taken_bb_size, ind_entry->not_taken_bb_size };
  }

  void m_update_bb_size(uint64_t ip, uint64_t bb_size, uint8_t branch_type, bool taken, uint64_t global_history) {
    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
      //cout << __func__ << " ip " << ip << " hist " << conditional_history << endl;
      auto ind_entry = find_entry(ip);
      if (ind_entry == NULL) return;
      if (taken)
        ind_entry->taken_bb_size = bb_size;
      else
        ind_entry->not_taken_bb_size = bb_size;
    }
  }

  uint64_t predict(uint64_t ip) {
    auto ind_entry = find_entry(ip);
    if (ind_entry == NULL) {
      // no prediction for this IP
      return 0;
    }
    update_lru(ind_entry);
    return ind_entry->target;
  }

  void update(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    //cout << __func__ << " type " << int(branch_type) << endl;
    //assert(false);
    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
      //cout << __func__ << " ip " << ip << " hist " << conditional_history << endl;
      auto ind_entry = find_entry(ip);
      if (ind_entry == NULL) {
        // no prediction for this entry so far, so allocate one
        uint64_t set = set_index(ip);
        ind_entry = get_lru_entry(set);
        ind_entry->ip_tag = ip;
        ind_entry->conf = 0;
        update_lru(ind_entry);
      }
      //if (ip == 0xffffa809827c) cout << __func__ << " index " << set_index(ip) << " target? " << std::hex << branch_target << std::dec << endl;
      if (ind_entry->target == branch_target)
      {
        if (ind_entry->conf < 3) {
          ind_entry->conf++;
        }
      } else {
        ind_entry->conf = 0;
      }
      ind_entry->target = branch_target;
    }

    last_conditional_history = conditional_history;

    if (branch_type == BRANCH_CONDITIONAL) {
      //cout << "Updating the conditional history " << ip << endl;
      conditional_history <<= 1;
      if (taken) {
        conditional_history |= 1;
      }
    }
  }

  void update_only_history(uint8_t branch_type, bool taken) {
    if (branch_type == BRANCH_CONDITIONAL) {
      conditional_history <<= 1;
      if (taken) {
        conditional_history |= 1;
      }
    }
  }

  void speculative_begin() {
    assert(!speculative_state);
    speculative_state = true;
    spec_lru_counter = lru_counter;
    spec_conditional_history = conditional_history;
    //cout << "Begin speculative: hist " << spec_conditional_history << endl;
    for (uint32_t i = 0; i < SETS; i++) {
      for (uint32_t j = 0; j < WAYS; j++) {
        spec_indirect[i][j].ip_tag = indirect[i][j].ip_tag;
        spec_indirect[i][j].target = indirect[i][j].target;
        spec_indirect[i][j].lru = indirect[i][j].lru;
        spec_indirect[i][j].taken_bb_size = indirect[i][j].taken_bb_size;
        spec_indirect[i][j].not_taken_bb_size = indirect[i][j].not_taken_bb_size;
      }
    }
  }

  void speculative_end() {
    assert(speculative_state);
    speculative_state = false;
    lru_counter = spec_lru_counter;
    conditional_history = spec_conditional_history;
    //cout << "Restoring speculative: hist " << conditional_history << endl;
    for (uint32_t i = 0; i < SETS; i++) {
      for (uint32_t j = 0; j < WAYS; j++) {
        indirect[i][j].ip_tag = spec_indirect[i][j].ip_tag;
        indirect[i][j].target = spec_indirect[i][j].target;
        indirect[i][j].lru = spec_indirect[i][j].lru;
        indirect[i][j].taken_bb_size = spec_indirect[i][j].taken_bb_size;
        indirect[i][j].not_taken_bb_size = spec_indirect[i][j].not_taken_bb_size;
      }
    }
  }

  uint8_t get_conf(uint64_t ip) {
    auto ind_entry = find_entry(ip);
    if (ind_entry != NULL) {
      ind_entry->conf;
    } else
      return 0;
  }

};
