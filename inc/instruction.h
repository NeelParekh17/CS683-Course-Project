/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>
#include <deque>

#include "trace_instruction.h"

 // branch types
enum branch_type {
  NOT_BRANCH = 0,
  BRANCH_DIRECT_JUMP = 1,
  BRANCH_INDIRECT = 2,
  BRANCH_CONDITIONAL = 3,
  BRANCH_DIRECT_CALL = 4,
  BRANCH_INDIRECT_CALL = 5,
  BRANCH_RETURN = 6,
  BRANCH_OTHER = 7
};

inline const char* printBR[] = {
    "NOT_BRANCH" ,
  "BRANCH_DIRECT_JUMP" ,
  "BRANCH_INDIRECT" ,
  "BRANCH_CONDITIONAL",
  "BRANCH_DIRECT_CALL",
  "BRANCH_INDIRECT_CALL" ,
  "BRANCH_RETURN" ,
  "BRANCH_OTHER"
};


enum pref_end_mode {
  WRONG_BRANCH_PRED = 1,
  WRONG_TARGET_PRED = 2,
  NO_TARGET = 3,
  NO_BB_SIZE = 4,
  SUCCESS = 5
};


struct pref_entry
{
  uint64_t branch_ip = 0;
  uint64_t event_cycle = 0;
  std::vector<std::pair<uint64_t, uint64_t>> path;
  std::vector<uint64_t> ips;
  std::vector<uint64_t> cache_lines;
  bool in_progress = false;
  bool taken = false;
  bool recovered_taken_path = false;

  void reset() {
    branch_ip = 0;
    event_cycle = 0;
    path.clear();
    ips.clear();
    cache_lines.clear();
    in_progress = false;
    taken = false;
    recovered_taken_path = false;
  }
};

struct uop_pref_entry {
  uint64_t pref_ip = 0;
  uint64_t target = 0;
  uint64_t bb_size = 0;
  uint64_t by_instr_id = 0;
  int8_t branch_type = 0;
  bool prefetch_taken_path = false;
};

enum tagged_bank_prediction_type {
  Wtag, //weak counter
  NWtag, //nearly weak counter
  NStag, //nearly saturated counter
  Stag, //saturated counter
  NONEtag
};
 
enum prediction_source {
  ALT_BANK,
  HIT_BANK,
  BIMODAL,
  LOOP,
  SAT_PRED,
  NONE
};

inline const char* printSRC[] = {
    "ALT_BANK",
  "HIT_BANK",
  "BIMODAL",
  "LOOP",
  "SAT_PRED",
  "NONE"
};


struct extra_branch_info
{
  prediction_source source = NONE;
  tagged_bank_prediction_type type = NONEtag;
  int ctr_value = 0;
  int ctr_value_sat_counter = 0;
  int u_bit = 0;
  bool is_h2p = false;
  int yout = 0;
};

struct alt_br_info {
  uint64_t br_ip = 0;
  bool prediction = false;
  bool checked = false;
  int br_type = 0;
};

struct ooo_model_instr {

  std::deque<uop_pref_entry> nested_pref_info;
  bool issue_nested_pref = false;

  extra_branch_info btb_prediction_src;
  extra_branch_info bp_prediction_src;

  bool btb_miss = false;

  uint64_t instr_id = 0;
  uint64_t instr_id_4b = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  uint64_t ftq_cycle = 0;
  uint64_t resolve_cycle = 0;

  bool two_byte_instr = false;

  uint64_t dispatched_cycle = 0;
  uint64_t branch_prediction_cycle = 0;

  uint8_t pref_end_mode = SUCCESS;

  bool wait_due_to_l1i_counted = false;
  bool wait_due_build = false;

  bool is_branch = 0;
  bool branch_taken = 0;
  bool branch_prediction = 0;
  bool branch_mispredicted = 0; // A branch can be mispredicted even if the direction prediction is correct when the predicted target is not correct

  int tage_source = 99;
  int tage_ctr_value = 99;
  int tage_sat_counter = 99;
  tagged_bank_prediction_type tage_pred_type = NONEtag;

  uint64_t alt_predicted_target = 0;
  bool alt_prediction = false;
  bool alt_bp_prediction = false;

  bool checked = false;
  uint64_t prev_miss_instr_id = 0;
  bool was_l1i_hit = false;
  bool already_fetched = false;
  bool decode_full = false;

  bool mrc_hit = false;

  uint8_t asid[2] = { std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max() };

  uint8_t branch_type = NOT_BRANCH;
  uint64_t branch_target = 0;
  uint64_t predicted_target = 0;

  bool predicted_always_taken = false;

  bool target_miss = false;
  bool bp_miss = false;
  bool h2p_hist_hit = false;
  bool br_resolved = false;

  uint8_t dib_checked = 0;
  uint8_t fetched = 0;
  uint8_t decoded = 0;
  uint8_t scheduled = 0;
  uint8_t executed = 0;

  bool l1i_hit = false;
  bool tag_checked = true;

  bool from_prefetch = false;
  bool alt_miss = false;
  uint64_t global_history;

  std::vector<std::pair<uint64_t, uint64_t>> taken_pref_path;
  std::vector<std::pair<uint64_t, uint64_t>> not_taken_pref_path;

  uint64_t l1i_inflight_time = 0;

  std::vector<std::pair<uint64_t, uint64_t>> taken_path;
  std::vector<std::pair<uint64_t, uint64_t>> not_taken_path;
  std::vector<uint64_t> call_path;

  std::vector<alt_br_info> prefetched_branch_info;

  bool hard_to_predict_branch = false;
  bool uop_cache_miss_panelty = false;
  bool uop_cache_hit = false;
  bool instr_after_branch_mispred = false;

  int yout = 0;

  bool branch_miss = false;
  pref_entry prefetch_info;

  bool is_critical_branch = false;

  bool wrong_path = false;
  bool critical_branch = false;

  uint64_t cycle_added_in_ftq = 0;
  bool is_critical = false;
  bool on_critial_path = false;
  bool after_br_miss = false;
  bool track_for_stats = false;
  bool started_branch_recovery = false;

  uint64_t num_window_update = 0;

  unsigned completed_mem_ops = 0;
  int num_reg_dependent = 0;

  std::vector<uint8_t> destination_registers = {}; // output registers
  std::vector<uint8_t> source_registers = {};      // input registers

  std::vector<uint64_t> destination_memory = {};
  std::vector<uint64_t> source_memory = {};

  // these are indices of instructions in the ROB that depend on me
  std::vector<std::reference_wrapper<ooo_model_instr>> registers_instrs_depend_on_me;

private:
  template <typename T>
  ooo_model_instr(T instr) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken)
  {
    std::remove_copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::back_inserter(this->destination_registers), 0);
    std::remove_copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::back_inserter(this->source_registers), 0);
    std::remove_copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::back_inserter(this->destination_memory), 0);
    std::remove_copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::back_inserter(this->source_memory), 0);
  }

public:
public:
  ooo_model_instr() {
    issue_nested_pref = false;
    instr_id = 0;
    ip = 0;
    event_cycle = 0;

    wait_due_to_l1i_counted = false;
    wait_due_build = false;

    is_branch = 0;
    branch_taken = 0;
    branch_prediction = 0;
    branch_mispredicted = 0;

    branch_type = NOT_BRANCH;
    branch_target = 0;

    dib_checked = 0;
    fetched = 0;
    decoded = 0;
    scheduled = 0;
    executed = 0;

    l1i_inflight_time = 0;

    uop_cache_miss_panelty = false;
    uop_cache_hit = false;

    wrong_path = false;
    cycle_added_in_ftq = 0;
    completed_mem_ops = 0;
    num_reg_dependent = 0;

    destination_registers = {}; // output registers
    source_registers = {};      // input registers

    destination_memory = {};
    source_memory = {};

    registers_instrs_depend_on_me = {};

    asid[0] = std::numeric_limits<uint8_t>::max();
    asid[1] = std::numeric_limits<uint8_t>::max();

  }

  ooo_model_instr(uint8_t cpu, input_instr instr) : ooo_model_instr(instr)
  {
    asid[0] = cpu;
    asid[1] = cpu;
  }

  ooo_model_instr(uint8_t, cloudsuite_instr instr) : ooo_model_instr(instr)
  {
    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
  }

  std::size_t num_mem_ops() const {
    return std::size(destination_memory) + std::size(source_memory);
  }

  static bool program_order(const ooo_model_instr& lhs, const ooo_model_instr& rhs) {
    return lhs.instr_id < rhs.instr_id;
  }
};

#endif
