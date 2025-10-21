#ifndef H2P_H
#define H2P_H

#include "champsim_constants.h"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "profiler.h"
#include "defines.h"
#include "way_associative.h"
#include "ooo_cpu_modules.inc"

#define H2P_SAT_COUNTER_BITS 16

//32Ke
#define H2P_SETS 4096
#define H2P_WAYS 8

class H2P {
private:
  struct ENTRY {
    uint64_t ip_tag;
    uint64_t lru;
    uint64_t miss_history = 0;
  };

  ENTRY ptable[H2P_SETS][H2P_WAYS];
  uint64_t lru_counter;


public:

  H2P() {
    for (uint32_t i = 0; i < H2P_SETS; i++) {
      for (uint32_t j = 0; j < H2P_WAYS; j++) {
        ptable[i][j].ip_tag = 0;
        ptable[i][j].lru = 0;
        ptable[i][j].miss_history = 0;
      }
    }
    lru_counter = 0;
  }

  ENTRY* find_entry(uint64_t ip)
  {
    uint64_t set = set_index(ip);
    for (uint32_t i = 0; i < H2P_WAYS; i++) {
      if (ptable[set][i].ip_tag == ip) {
        return &(ptable[set][i]);
      }
    }
    return NULL;
  }

  ENTRY* get_lru_entry(uint64_t set)
  {
    uint32_t lru_way = 0;
    uint64_t lru_value = ptable[set][lru_way].lru;
    for (uint32_t i = 0; i < H2P_WAYS; i++) {
      if (ptable[set][i].lru < lru_value) {
        lru_way = i;
        lru_value = ptable[set][lru_way].lru;
      }
    }
    return &(ptable[set][lru_way]);
  }

  void update_lru(ENTRY* ptable)
  {
    ptable->lru = lru_counter;
    lru_counter++;
  }

  bool is_h2p_branch_tage(ooo_model_instr& instr) {
    if (instr.branch_type == BRANCH_CONDITIONAL) {
      if (instr.bp_prediction_src.source != 0 && instr.bp_prediction_src.source != 1 && instr.bp_prediction_src.source != 2 && instr.bp_prediction_src.source != 3 && instr.bp_prediction_src.source != 4) {
        cout << "Instr id " << instr.instr_id << " instr.bp_prediction_src.source " << instr.bp_prediction_src.source << endl;
        assert(false);
      }
    }

    //     Targets:
    //          BTB miss: low
    //          BTB hit and provides target: high
    //          BTB hit and redirects to indirect predictor: check ctr bits
    //          BTB hit and redirects to RAS: high

    //     Condition:
    //         follow tage.

    //     Both:
    //         If the condition is low, the branch is h2p
    //         If the condition is high and not taken is not h2p
    //         if the condition is high and taken then: 
    //                 if target is low is h2p
    //                 if target is high is not h2p

    bool low_conf_target = false;
    bool low_conf_condition = false;

    if (instr.btb_miss) { //BTB miss: low
      low_conf_target = true;
    } else if (!instr.btb_miss  //BTB hit and redirects to indirect predictor: check ctr bits
      && (instr.branch_type == BRANCH_INDIRECT || instr.branch_type == BRANCH_INDIRECT_CALL)) {
      if (instr.btb_prediction_src.source == ALT_BANK || ((instr.btb_prediction_src.source == HIT_BANK
        && instr.btb_prediction_src.ctr_value != -4 && instr.btb_prediction_src.ctr_value != 3))) {
        low_conf_target = true;
      }
    }

    //condition miss now so follow tage
    if (
      //(instr.bp_prediction_src.source == ALT_BANK) || //ALTBANK
      //((instr.bp_prediction_src.source == HIT_BANK && (instr.bp_prediction_src.ctr_value > -4 && instr.bp_prediction_src.ctr_value < 3)))
      ((instr.bp_prediction_src.source == HIT_BANK && (instr.bp_prediction_src.ctr_value == -3)))
      //|| (instr.bp_prediction_src.source == HIT_BANK && hitbank_miss_hist > 0)) //HITBANK
      // || ((instr.bp_prediction_src.source == BIMODAL && (instr.bp_prediction_src.ctr_value > 0 && instr.bp_prediction_src.ctr_value < 3)) ||
      //   (instr.bp_prediction_src.source == BIMODAL && bimodal_miss_hist > 0)) //BiModal
      ) {
      low_conf_condition = true;
    }

    //low conf directions are always h2p
    if (low_conf_condition) {
      return true;
    }

    //if the direction confident is high and predicted as taken but the target is low conf then its a h2p
    if (!low_conf_condition && instr.branch_prediction) {
      if (low_conf_target)
        return true;
    }
    return false;
  }

  bool is_h2p_branch_table(uint64_t ip, int level) {
    auto entry = find_entry(ip);
    if (entry == NULL) {
      return false;
    } else {
      update_lru(entry);
      return bitSelect(entry->miss_history, H2P_SAT_COUNTER_BITS);
    }
  }

  void UpdateBranchHistory(const ooo_model_instr instr) {
    if (instr.bp_prediction_src.source == BIMODAL) {
      bimodal_miss_hist = bimodal_miss_hist << 1;
      if (instr.branch_miss) {
        bimodal_miss_hist += 1;
      }
      //cout << "Bimodal Miss? " << instr.branch_miss << " hist " << std::hex << unsigned(bimodal_miss_hist) << std::dec << endl;
    } else if (instr.bp_prediction_src.source == HIT_BANK) {
      hitbank_miss_hist = hitbank_miss_hist << 1;
      if (instr.branch_miss) {
        hitbank_miss_hist += 1;
      }
    }

    auto entry = find_entry(instr.ip);
    if (entry == NULL) {
      uint64_t set = set_index(instr.ip);
      entry = get_lru_entry(set);
      entry->ip_tag = instr.ip;
      update_lru(entry);
    }

    entry->miss_history = entry->miss_history << 1;
    if (instr.branch_miss) {
      entry->miss_history += 1;
    }
  }

  uint64_t set_index(uint64_t ip) {
    return (((ip >> 2)) & (H2P_SETS - 1));
  }

  uint64_t bitSelect(uint64_t ip, int last_n)
  {
    uint64_t mask = (1 << last_n) - 1;
    return ip & mask;
  }

private:
  std::map<uint64_t, unsigned> l1_branch_history;
  std::map<uint64_t, unsigned> l2_branch_history;
  uint8_t bimodal_miss_hist = 0;
  uint8_t hitbank_miss_hist = 0;
};
#endif