#ifndef H2P_MAP_H
#define H2P_MAP_H

#include "champsim_constants.h"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "profiler.h"
#include "defines.h"
#include "way_associative.h"
#include <deque>


#define H2P_SAT_COUNTER_BITS 16

class H2P_MAP {
public:

  H2P_MAP(uint64_t m_dib_widnow) {

  }

  bool IsHardToPredictBranch(uint64_t ip, int level) {
    if (level == 1)
      return bitSelect(l1_branch_history[ip], H2P_SAT_COUNTER_BITS);
    else if (level == 2)
      return bitSelect(l2_branch_history[ip], H2P_SAT_COUNTER_BITS);
    else
      assert(false);
  }

  void UpdateBranchHistory(uint64_t ip, bool miss, int level) {
    if (level == 1) {
      l1_branch_history[ip] = (l1_branch_history[ip] << 1);
      if (miss)
        l1_branch_history[ip]++;
    } else if (level == 2) {
      l2_branch_history[ip] = (l2_branch_history[ip] << 1);
      if (miss)
        l2_branch_history[ip]++;
    } else
      assert(false);
  }

  uint64_t bitSelect(uint64_t ip, int last_n)
  {
    uint64_t mask = (1 << last_n) - 1;
    return ip & mask;
  }

private:
  std::map<uint64_t, unsigned> l1_branch_history;
  std::map<uint64_t, unsigned> l2_branch_history;
};
#endif