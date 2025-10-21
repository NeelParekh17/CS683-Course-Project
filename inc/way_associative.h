#ifndef WAYASSOCIATIVE
#define WAYASSOCIATIVE

#include "ooo_cpu.h"
#include "defines.h"
#include "sat_counter.h"

struct path_history_t {
  bool save_taken_path = false;
  bool save_not_taken_path = false;
  std::vector<std::pair<uint64_t, uint64_t>> taken_path_info;
  std::vector<std::pair<uint64_t, uint64_t>> not_taken_path_info;
  uint64_t history_ip = 0;
  uint64_t history_instr_id = 0;
  bool saving_path_done = false;
  uint64_t index = 0;
  bool target_miss = false;
  bool last_target_taken = false;
  bool mru = false;
  uint64_t ip = 0;
  uint64_t tag = 0;
  bool is_h2p_path = false;
  uint64_t last_accessed = 0;
};

class WayAssociative {

public:
  WayAssociative()
  {
    m_ways_ptr.resize(m_way_associtivity);
    for (auto& ways : m_ways_ptr) {
      ways.ip = 0;
      ways.tag = 0;
      ways.mru = false;
    }
  }

  ~WayAssociative() {}

  vector<path_history_t> m_ways_ptr;
  int m_way_associtivity = 8;
};

#endif // WAYASSOCIATIVE