/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets.
 */

#include "ooo_cpu.h"
#include "branch_info.h"

template <uint32_t SETS, uint32_t WAYS>
class BasicBTB {

private:

  struct ENTRY {
    uint64_t ip_tag;
    uint64_t target;
    uint8_t branch_info;
    uint64_t lru;
    uint64_t taken_bb_size;
    uint64_t not_taken_bb_size;
    int branch_type;
  };

  ENTRY basic_btb[SETS][WAYS];
  uint64_t lru_counter;

  uint64_t set_index(uint64_t ip) {
    return ((ip >> 2) & (SETS - 1));
  }

  ENTRY* find_entry(uint64_t ip) {
    uint64_t set = set_index(ip);
    for (uint32_t i = 0; i < WAYS; i++) {
      if (basic_btb[set][i].ip_tag == ip) {
        return &(basic_btb[set][i]);
      }
    }
    return NULL;
  }

  ENTRY* get_lru_entry(uint64_t set) {
    uint32_t lru_way = 0;
    uint64_t lru_value = basic_btb[set][lru_way].lru;
    for (uint32_t i = 0; i < WAYS; i++) {
      if (basic_btb[set][i].lru < lru_value) {
        lru_way = i;
        lru_value = basic_btb[set][lru_way].lru;
      }
    }
    //&& issuing_instr.ip == 0xffffa8232408
    // if (basic_btb[set][lru_way].ip_tag == 0xffffa8232408)
    //   cout << "Evicted! " << std::hex << basic_btb[set][lru_way].target << std::dec << endl;
    return &(basic_btb[set][lru_way]);
  }

  void update_lru(ENTRY* btb_entry) {
    btb_entry->lru = lru_counter;
    lru_counter++;
  }

public:

  void initialize() {
    std::cout << "Basic BTB sets: " << SETS << " ways: " << WAYS << std::endl;
    for (uint32_t i = 0; i < SETS; i++) {
      for (uint32_t j = 0; j < WAYS; j++) {
        basic_btb[i][j].ip_tag = 0;
        basic_btb[i][j].target = 0;
        basic_btb[i][j].branch_info = BRANCH_INFO_ALWAYS_TAKEN;
        basic_btb[i][j].lru = 0;
        basic_btb[i][j].taken_bb_size = 0;
        basic_btb[i][j].not_taken_bb_size = 0;
        basic_btb[i][j].branch_type = 0;
      }
    }
    lru_counter = 0;
  }

  std::pair<uint64_t, uint64_t> get_bb_size(uint64_t ip) {
    auto btb_entry = find_entry(ip);
    if (btb_entry == NULL) {
      return std::pair{ 0, 0 };
    }
    return std::pair{ btb_entry->taken_bb_size, btb_entry->not_taken_bb_size };
  }

  uint8_t get_branch_type(uint64_t ip)
  {
    auto btb_entry = find_entry(ip);
    if (btb_entry != NULL)
      return btb_entry->branch_type;
    else
      return 0;
  }

  void m_update_bb_size(uint64_t ip, uint64_t bb_size, bool taken) {
    auto btb_entry = find_entry(ip);
    if (btb_entry == NULL) {
      return;
    }
    if (taken)
      btb_entry->taken_bb_size = bb_size;
    else
      btb_entry->not_taken_bb_size = bb_size;
  }

  std::pair<uint64_t, uint8_t> predict(uint64_t ip) {
    auto btb_entry = find_entry(ip);
    if (btb_entry == NULL) {
      // no prediction for this IP
      //if (ip == 0xffffa8232408) cout << "Miss " << endl;
      return std::make_pair(0, BRANCH_INFO_ALWAYS_TAKEN);
    }
    update_lru(btb_entry);
    //if (ip == 0xffffa8232408) cout << "Prediction " << std::hex << (btb_entry->target) << std::dec << endl;
    return std::make_pair(btb_entry->target, btb_entry->branch_info);
  }

  void update(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    auto btb_entry = find_entry(ip);
    if (btb_entry == NULL) {
      if ((branch_target == 0) || !taken) {
        return;
      }
      // no prediction for this entry so far, so allocate one
      uint64_t set = set_index(ip);
      btb_entry = get_lru_entry(set);
      btb_entry->ip_tag = ip;
      btb_entry->branch_info = BRANCH_INFO_ALWAYS_TAKEN;
      btb_entry->branch_type = int(branch_type);
      //if (ip == 0xffffa821bda8) cout << "Updated with type " << int(branch_type) << endl;
      update_lru(btb_entry);
    }
    // update btb entry
    if (branch_target != 0) btb_entry->target = branch_target;

    //if (ip == 0xffffa8232408) cout << "target updated to " << std::hex << btb_entry->target << std::dec << endl;

    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
      btb_entry->branch_info = BRANCH_INFO_INDIRECT;
    } else if (branch_type == BRANCH_RETURN) {
      btb_entry->branch_info = BRANCH_INFO_RETURN;
    } else if (branch_type == BRANCH_CONDITIONAL) {
      btb_entry->branch_info = BRANCH_INFO_CONDITIONAL;
    }
  }
};
