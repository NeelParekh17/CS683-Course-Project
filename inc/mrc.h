
#ifndef _MRC
#define _MRC

#include <algorithm>
#include <deque>
#include <iostream>
#include <math.h>
#include <tgmath.h>
#include <vector>

#include "champsim_constants.h"
#include "defines.h"
#include "instruction.h"
#include "operable.h"
#include "profiler.h"


// Size calculations
// 4bytes of uops per entry (total 64 )  = 256bytes
// 8 bytes of tag, target address, replacement = 8 Bytes
// each entry = 264 Bytes == 0.2578125 KB
// 

struct mrc_entry {
  uint64_t target = 0;
  bool taken = false;
  std::deque<uint64_t> ips_in_entry;
  uint64_t last_used = 0;
};

using mrc_cache = std::vector<mrc_entry>;

class MRC
{
public:
  MRC()
  {
    std::cout << "MRC with entries: " << NUM_OF_ENTRIES << " each having " << NUM_OF_INSTR_PER_ENTRY << " instructiions " << std::endl;
    m_MRC.resize(NUM_OF_ENTRIES);
  }

  ~MRC() {}

  void AddEntry(uint64_t target)
  {
    // if (target == 281473508472968)
    //   cout << __func__ << " target " << target << endl;
    if (target == 0)
      assert(false);
    auto entry = std::find_if(m_MRC.begin(), m_MRC.end(), [&](const mrc_entry& entry) { return entry.target == target; });
    if (entry != m_MRC.end()) {
      entry->target = target;
      entry->ips_in_entry.clear();
    } else {
      auto victim = EvictLRU(m_MRC.begin(), m_MRC.end());
      victim->target = target;
      victim->ips_in_entry.clear();
    }
  }

  void AddInstruction(uint64_t ip, uint64_t target, uint64_t current_cycle)
  {
    // if (target == 281473508472968)
    //   cout << __func__ << " ip " << ip << " target " << target << " @" << current_cycle << endl;
    auto entry = std::find_if(m_MRC.begin(), m_MRC.end(), [&](const mrc_entry& entry) { return entry.target == target; });
    if (entry != m_MRC.end()) {
      if (entry->ips_in_entry.size() < NUM_OF_INSTR_PER_ENTRY) {
        entry->ips_in_entry.push_back(ip);
        entry->last_used = current_cycle;
      }
    } else {
      // entry should be there
      assert(false);
    }
  }

  std::deque<uint64_t> Lookup(uint64_t target, uint64_t current_cycle)
  {
    auto entry = std::find_if(m_MRC.begin(), m_MRC.end(), [&](const mrc_entry& entry) { return entry.target == target; });
    if (entry != m_MRC.end()) {
      entry->last_used = current_cycle;
      return entry->ips_in_entry;
      // if (target == 281473508472968)
      //   cout << __func__ << " target " << target << " HIT @" << current_cycle << endl;
    }
    // if (target == 281473508472968)
    //   cout << __func__ << " target " << target << " MISS @" << current_cycle << endl;
    return std::deque<uint64_t>();
  }

private:
  mrc_cache m_MRC{NUM_OF_ENTRIES};

  mrc_cache::iterator EvictLRU(mrc_cache::iterator begin, mrc_cache::iterator end)
  {
    return std::min_element(begin, end, [](mrc_entry& a, mrc_entry& b) { return a.last_used < b.last_used; });
  }

  int lg2(uint64_t n) { return n < 2 ? 0 : 1 + lg2(n / 2); }
};

inline MRC* m_mrc_ptr = new MRC();

#endif // _MRC 