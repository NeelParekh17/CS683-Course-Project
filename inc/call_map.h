#ifndef CALL_MAP
#define CALL_MAP

#include "champsim_constants.h"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "profiler.h"
#include "defines.h"
#include "way_associative.h"
#include <deque>

struct call_map {
  SaturationCounter br_counter = SaturationCounter(H2P_SAT_COUNTER_BITS);
  SaturationCounter miss_counter = SaturationCounter(H2P_SAT_COUNTER_BITS);
  SaturationCounter hard_to_predict = SaturationCounter(2);
  std::vector<std::pair<uint64_t, uint64_t>> taken_path_info;
  std::vector<std::pair<uint64_t, uint64_t>> not_taken_path_info;
  uint64_t ip = 0;
  bool save_taken_path = false;
  bool save_not_taken_path = false;
  bool saving_path_done = false;
  uint64_t last_ip_filled = 0;
  uint64_t num_ip_saved = 0;
  uint64_t history_ip = 0;
  uint64_t history_instr_id = 0;
  bool new_bb = false;
  bool indirect_branch = false;
  uint64_t index = 0;
  uint64_t tag = 0;
};

class call_hist {
public:

  call_hist(uint64_t m_dib_widnow) {
    uop_cache_windows_recovered = 0;
    num_of_branch_recoverd = 0;
    total_critical_instruction = 0;
    begin_branch_recovery = false;
    fill_taken = false;
    fill_not_taken = false;
    dib_window = m_dib_widnow;
    last_ip = 0;
    last_target = 0;
    new_bb = false;
    end_after_h2p = false;
  }

  void UpdateMissBranchHistory(uint64_t ip, bool is_branch, bool branch_mispredicted, bool taken, ooo_model_instr instr) {

    // cout << __func__
    //   << std::hex << " ip " << instr.ip << std::dec
    //   << " instr_id " << instr.instr_id
    //   << " branch " << int(instr.is_branch)
    //   << " h2p " << int(instr.hard_to_predict_branch)
    //   << " branch miss " << int(instr.branch_mispredicted)
    //   << " h2p_hist_hit? " << int(instr.h2p_hist_hit)
    //   << " taken? " << int(instr.branch_taken)
    //   << " hit? " << int(instr.uop_cache_hit)
    //   << endl;

    if (ip >> lg2(dib_window) != last_ip >> lg2(dib_window)) {
      if (ip == last_ip) assert(false);
      uop_cache_windows_recovered++;
    }
    last_ip = ip;

    //update the windows
    if (is_branch)
    {
      //save last branch data
      for (auto& x : ongoing_branch_history) {
        if (x.save_taken_path && !x.saving_path_done) {
          if (x.taken_path_info.size() < NUM_BRANCHES_SAVED) {
            if (uop_cache_windows_recovered == 0) uop_cache_windows_recovered++;
            if (x.taken_path_info.empty()) {
              x.taken_path_info.push_back(std::pair{last_target, uop_cache_windows_recovered});
            } else {
              if (new_bb) { //not-taken branch so do not need to save the target, add it to last window`
                x.taken_path_info.push_back(std::pair{last_target, uop_cache_windows_recovered});
              } else {
                x.taken_path_info.back().second += uop_cache_windows_recovered;
              }
            }
          } else {
            if (instr.branch_taken) {
              x.saving_path_done = true;
            } else {
              x.taken_path_info.back().second += uop_cache_windows_recovered;
            }
          }
        }

        if (x.save_not_taken_path && !x.saving_path_done) {
          if (x.not_taken_path_info.size() < NUM_BRANCHES_SAVED) {
            if (uop_cache_windows_recovered == 0) uop_cache_windows_recovered++;
            if (x.not_taken_path_info.empty()) {
              x.not_taken_path_info.push_back(std::pair{x.history_ip + 4, uop_cache_windows_recovered});
            } else {
              if (new_bb) { //not-taken branch so do not need to save the target, add it to last window
                x.not_taken_path_info.push_back(std::pair{last_target, uop_cache_windows_recovered});
              } else {
                x.not_taken_path_info.back().second += uop_cache_windows_recovered;
              }
            }
          } else {
            if (instr.branch_taken) {
              x.saving_path_done = true;
            } else {
              x.not_taken_path_info.back().second += uop_cache_windows_recovered;
            }
          }
        }
      }

      while (!ongoing_branch_history.empty()) {
        auto to_add = ongoing_branch_history.front();
        if (to_add.saving_path_done) {
          path_info[to_add.index].ip = to_add.history_ip;
          path_info[to_add.index].history_ip = to_add.history_ip;
          path_info[to_add.index].history_instr_id = to_add.history_instr_id;
          path_info[to_add.index].tag = to_add.tag;
          path_info[to_add.index].index = to_add.index;
          if (to_add.save_taken_path) {
            //cout << __func__ << " Added Taken ip " << std::hex << to_add.history_ip << " index " << std::dec << to_add.index  << endl;
            path_info[to_add.index].taken_path_info = to_add.taken_path_info;
          } else {
            //cout << __func__ << " Added Not-Taken ip " << std::hex << to_add.history_ip << " index " << std::dec << to_add.index  << endl;
            path_info[to_add.index].not_taken_path_info = to_add.not_taken_path_info;
          }
          ongoing_branch_history.pop_front();
        } else {
          break;
        }
      }

      //save the history for the ongoing branches
      uop_cache_windows_recovered = 0;
      call_map entry;
      if (instr.branch_type == BRANCH_DIRECT_CALL || instr.branch_type == BRANCH_INDIRECT_CALL) {
        last_target = instr.branch_target;
        entry.save_taken_path = true;
        entry.taken_path_info.clear();
        entry.history_ip = ip;
        entry.saving_path_done = false;
        entry.history_instr_id = instr.instr_id;
        new_bb = true;
        entry.index = getIndexPath(ip);
        entry.tag = getTag(ip);
      }
      ongoing_branch_history.push_back(entry);
    }
  }

  bool HistoryExist(uint64_t ip) {
    if (path_info[getIndexPath(ip)].tag == getTag(ip)) {
      //cout << __func__ << " matched ip " << std::hex << ip << " index " << std::dec << getIndexPath(ip)  << endl;
      return true;
    } else {
      //cout << __func__ << " not-matched ip " << std::hex << ip << " index " << std::dec << getIndexPath(ip)  << endl;
      return false;
    }
    //return true;
  }

  std::vector<std::pair<uint64_t, uint64_t>> GetTakenPath(uint64_t ip) {
    return path_info[getIndexPath(ip)].taken_path_info;
  }

  uint64_t getIndexPath(uint64_t ip) {
    return ip;
    //return bitSelect(ip, m_num_bits) ^ bitSelect(m_history, 8);
    //return bitSelect(ip, m_num_bits);
  }

  uint64_t getTag(uint64_t ip) {
    return ip;
  }

  uint64_t bitSelect(uint64_t ip, int last_n)
  {
    uint64_t mask = (1 << last_n) - 1;
    return ip & mask;
  }

private:
  std::map<unsigned long long, call_map> branch_history;
  std::map<uint64_t, path_history_t> path_info;
  std::deque<path_history_t> ongoing_branch_history;
  uint64_t uop_cache_windows_recovered;
  uint64_t num_of_branch_recoverd;
  uint64_t total_critical_instruction;
  uint64_t last_ip;
  uint64_t last_target;
  bool begin_branch_recovery;
  bool fill_taken;
  bool fill_not_taken;
  uint64_t dib_window;
  bool new_bb;
  bool end_after_h2p = false;
  unsigned m_history;
  uint64_t m_num_bits = NUM_HIST_PATH_BITS;
};
#endif