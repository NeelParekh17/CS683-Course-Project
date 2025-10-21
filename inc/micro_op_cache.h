
#ifndef MICROOPCACHE
#define MICROOPCACHE

#include <algorithm>
#include <deque>
#include <iostream>
#include <math.h>
#include <tgmath.h>
#include <vector>

#include "champsim_constants.h"
#include "defines.h"
#include "instruction.h"
#include "memory_class.h" 
#include "operable.h"
#include "profiler.h"

// 8 micro-ops per cycle instead of 6 and can hold 4K micro-ops, up from 2.25K before
#define NUM_WAYS 8
#define SIZE_WINDOWS 32

const int MAX_IPS_PER_WINDOW = SIZE_WINDOWS / 4; // remember to change it when the window changes

struct uop_cache_entry_t {
  uint64_t ip = 0;
  uint8_t hotness = 0;
  uint64_t last_used = 0;
  bool is_critical = false;
  bool used_once = false;
  SaturationCounter critical_hotness = SaturationCounter(3);
  uint64_t ips_in_window[MAX_IPS_PER_WINDOW] = {0};
  bool terminated_by_taken = false;
  bool evict_on_demand = false;
  bool pref = false;
  bool is_valid = true;
  bool on_wrong_h2p_path = false;
  bool stats_already_counted = false;
  bool was_from_miss_br = false;
  bool pref_stats_counter = false;
  bool permanent_pref_flag = false;
  uint8_t num_br = 0;
  bool active = true;

  // ADDED THIS NEW FIELD:
  bool alternate_path_prefetch = false;  // Set for branches predicted NOT TAKEN
};

using uop_cache_t = std::vector<uop_cache_entry_t>;

class MicroOpCache
{
public:
  MicroOpCache()
  {
    cout << "Uop cache of sets " << UOP_CACHE_NUM_SETS << " ways " << NUM_WAYS << " window " << SIZE_WINDOWS << " Total entries "
         << (UOP_CACHE_NUM_SETS * NUM_WAYS * (SIZE_WINDOWS / 4)) << endl;
    sets = UOP_CACHE_NUM_SETS;
    window = SIZE_WINDOWS;
    ways = NUM_WAYS;
    UOP.resize(UOP_CACHE_NUM_SETS * NUM_WAYS);
  }

  ~MicroOpCache() {}

  void UpdateHotness()
  {
    for_each(UOP.begin(), UOP.end(), [](uop_cache_entry_t& x) {
      x.hotness--;
      x.critical_hotness.decrement();
    });
  }

  void Invalidate(uint64_t ip)
  {
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);

    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip)) {
        way->evict_on_demand = true;
      }
    }
  }

  void UpdateStats(uint64_t ip, bool dib_hit, bool in_mshr, bool pref_completed, bool recently_pref, uint64_t current_cycle, bool on_critial_path)
  {
    // if same window then return
    // the stats are calculated per window (cache line in a micro-op cache is same size as window)
    if (ip >> lg2(window) == last_ip_to_check_dib >> lg2(window)) {
      return;
    }

    bool is_prefetched = false;
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);
    bool hit = false;
    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip)) {
        if (way->pref) {
          is_prefetched = true;
        }
        way->pref = false;
      }
    }

    get_profiler_ptr->stats_table[((ip >> lg2(window)) & STATS_TABLE_MASK)].accesses++;
    if (!dib_hit) {
      get_profiler_ptr->stats_table[((ip >> lg2(window)) & STATS_TABLE_MASK)].misses++;
      if (in_mshr && !pref_completed) {
        get_profiler_ptr->stats_table[((ip >> lg2(window)) & STATS_TABLE_MASK)].late++;
      }
      if (!in_mshr && recently_pref) {
        get_profiler_ptr->stats_table[((ip >> lg2(window)) & STATS_TABLE_MASK)].early++;
      }
    }
    if (dib_hit && is_prefetched) {
      get_profiler_ptr->stats_table[((ip >> lg2(window)) & STATS_TABLE_MASK)].hits++;
    }

    last_ip_to_check_dib = ip;
  }

  bool Ishit(uint64_t ip)
  {
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);
    bool hit = false;
    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip)) {
        hit = true;
      }
    }
    return hit;
  }

  bool Lookup(uint64_t ip, uint64_t current_cycle)
  {
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);
    bool hit = false;
    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip)) {
        hit = true;
        // ADDED THIS: Reset alternate path bit on access
        if (way->alternate_path_prefetch) {
          way->alternate_path_prefetch = false;  // RESET ON ACCESS
          
          // Optional: Track statistics
          // get_profiler_ptr->uop_pref_stats.alternate_path_useful++;
        }
        if (way->is_critical) {
          way->critical_hotness.increment();
        }
        if (way->pref && !way->used_once) {
          way->used_once = true;
          if (way->was_from_miss_br) {
            get_profiler_ptr->hpca.br_miss_used++;
          }
        }
        if (way->last_used != current_cycle) {
          way->hotness++;
          way->last_used = current_cycle;
        }
        break;
      }
    }

    return hit;
  }

  // NEW signature - ADDED is_alternate_path parameter:
  void Insert(uint64_t ip, uint64_t current_cycle, bool critical, bool taken_end, bool pref, bool was_br_miss, bool is_branch, bool is_alternate_path)
  {
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);

    bool tag_found = false;
    bool ip_found = false;

    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      // check for the termination condition here
      // we check with the window granularity these termination conditions are here to simulate evictions

      // if we want to add in a entry and it is active (meaning it is not terminated yet) then check for termination conditions
      if (getTag(ip) == getTag(way->ip) && way->active && (way->terminated_by_taken || way->num_br == 2)) {
        //  the termination by taken branch should keep the taken branch and then allocate the next up coming entries to new entry

        // for termination by JUMP we could assume that the next instruction will map to the new line as it's a jump
        // but it is possible that the JUMP might be over a very small distance like 1 or instruction, in such case
        // the entry might map to the old entry so the condition below can be useful.

        if (way->terminated_by_taken) { // if it is still ended by taken branch then allocate a new entry
          way->active = false;
        }

        if (way->num_br == 2) {
          way->active = false;
        }
      }

      // if next occurance the branch is predicted not taken then we do not need to terminate it anymore
      // if it is terminated we need to rest it
      // if the entry is terminated by 2 branches it will be rest at evictions
      // mostly taken branches will map to another entry anyways
      if (getTag(ip) == getTag(way->ip) && !way->active && way->num_br < 2 && !taken_end) {
        // the branch is not taken this time so activate the entry
        way->active = true;
      }
    }

    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip) && way->active) {

        // update if this entry should be terminated by the predicted taken branch.
        way->terminated_by_taken = taken_end;

        // update the total number of branches in this entry
        if (is_branch) {
          way->num_br++;
          assert(way->num_br <= 2); // each way can have max 2 branches
        }

        tag_found = true;

        if (critical)
          way->is_critical = critical;

        if (way->last_used != current_cycle) {
          way->hotness++;
          way->last_used = current_cycle;
          way->critical_hotness.increment();
        }
        // if not already pref mark pref
        if (!way->pref && pref) {
          way->pref = pref;
          get_profiler_ptr->uop_pref_stats.window_prefetched++;
        }
        way->was_from_miss_br = was_br_miss;
        way->permanent_pref_flag = pref;

        // ADDED THIS: Set alternate path bit 
        if (pref && is_alternate_path) {
          way->alternate_path_prefetch = true;
          // way->last_used=0; // to increase hotness faster
        }
        break;  
      }
    }

    if (!tag_found) {
      auto victim = uop_set_begin;
#ifdef UOP_CACHE_LRU_RP
      // victim = EvictLRU(uop_set_begin, uop_set_end);
      // ADDED: EvictLRU_AlternatePath
      victim = EvictLRU_AlternatePath(uop_set_begin, uop_set_end);
#elif defined(UOP_CACHE_SMARTLRU_RP)
      victim = SmartLRU(uop_set_begin, uop_set_end);
#elif defined(UOP_CACHE_HOTLOOP_RP)
      victim = Hotloop(uop_set_begin, uop_set_end);
#elif defined(UOP_CACHE_LRUHOTLOOP_RP)
      victim = EvictLRUHotness(uop_set_begin, uop_set_end);
#elif defined(UOP_CACHE_LRUCRITICAL_RP)
      victim = EvictLRUCritical(uop_set_begin, uop_set_end, critical);
#elif defined(UOP_CACHE_HOTLOOPCRITICAL_RP)
      victim = EvictHotloopCritical(uop_set_begin, uop_set_end, critical);
#endif

      if (victim->pref) {
        if (victim->used_once) {
          get_profiler_ptr->m_eviction_stats.by_pref_used++;
        } else {
          get_profiler_ptr->m_eviction_stats.by_pref_not_used++;
        }
      } else {
        get_profiler_ptr->m_eviction_stats.by_demand++;
      }

      // prefetch that were on the not the alternate path but is used
      if (!victim->stats_already_counted) {
        victim->stats_already_counted = true;

        // reuse even on wrong path
        if (victim->on_wrong_h2p_path) {
          get_profiler_ptr->hpca.total_wrong_pref++;
          if (victim->used_once)
            get_profiler_ptr->hpca.wrong_uop_pref_used++;
        }
      }

      if (victim->is_critical) {
        get_profiler_ptr->victim_critical_ways++;
        if (victim->used_once)
          get_profiler_ptr->victim_critical_ways_used++;
      }

      victim->ip = ip;
      victim->last_used = current_cycle;
      victim->hotness = UINT8_MAX;
      victim->is_critical = critical;
      victim->critical_hotness.max();
      victim->terminated_by_taken = false;
      victim->pref = pref;
      victim->was_from_miss_br = was_br_miss;
      victim->pref_stats_counter = false;
      victim->permanent_pref_flag = pref;
      victim->stats_already_counted = false;
      victim->on_wrong_h2p_path = false;
      victim->num_br = 0;
      victim->active = true;

      // ADDED THIS: Set alternate path bit for new entry
      victim->alternate_path_prefetch = (pref && is_alternate_path);

      if (pref) {
        get_profiler_ptr->uop_pref_stats.window_prefetched++;
      }
    }
  }

  void invalidateEntries(std::vector<uint64_t> ips, uint64_t by_instr)
  {
    for (auto& ip : ips) {
      auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
      auto uop_set_end = std::next(uop_set_begin, ways);
      for (auto way = uop_set_begin; way != uop_set_end; ++way) {
        if ((getTag(ip) == getTag(way->ip)) && way->pref) { // only invalidate the prefetched ways
          way->is_valid = false;
        }
      }
    }
  }

  // void markWrongPrefetch(auto ips, uint64_t by_instr)
  // {
  //   for (auto& entry : ips) {
  //     auto uop_set_begin = std::next(UOP.begin(), ((entry.ip >> lg2(window)) % sets) * ways);
  //     auto uop_set_end = std::next(uop_set_begin, ways);
  //     for (auto way = uop_set_begin; way != uop_set_end; ++way) {
  //       if ((getTag(entry.ip) == getTag(way->ip)) && way->pref && !way->stats_already_counted) { // only invalidate the prefetched ways
  //         way->on_wrong_h2p_path = true;
  //         get_profiler_ptr->hpca.total_pref_on_wrong_h2p_path++;
  //       }
  //     }
  //   }
  // }

  bool isPrefetched(uint64_t ip)
  {
    auto uop_set_begin = std::next(UOP.begin(), ((ip >> lg2(window)) % sets) * ways);
    auto uop_set_end = std::next(uop_set_begin, ways);

    bool tag_found = false;

    for (auto way = uop_set_begin; way != uop_set_end; ++way) {
      if (getTag(ip) == getTag(way->ip)) {
        return way->permanent_pref_flag;
      }
    }
    return false;
  }

private:
  uint64_t sets;
  uint64_t ways;
  uint64_t window;

  std::vector<unsigned long long> top_4;

  uop_cache_t UOP{sets * ways};

  uint64_t last_ip_to_check_dib = 0;

  uop_cache_t::iterator SmartLRU(uop_cache_t::iterator begin, uop_cache_t::iterator end)
  {
    for (auto it = begin; it != end; ++it) {
      if (!it->is_valid) {
        return it;
      }
    }

    return std::min_element(begin, end, [](uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.last_used < b.last_used; });
  }

  uop_cache_t::iterator EvictLRU(uop_cache_t::iterator begin, uop_cache_t::iterator end)
  {
    return std::min_element(begin, end, [](uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.last_used < b.last_used; });
  }

  // ADDED: LRU with alternate path priority
  uop_cache_t::iterator EvictLRU_AlternatePath(uop_cache_t::iterator begin, uop_cache_t::iterator end)
  {
    // Check if any alternate path prefetch entries exist in this set
    bool has_alternate_path = std::any_of(begin, end, 
      [](const uop_cache_entry_t& entry) { 
        return entry.alternate_path_prefetch; 
      });

    // Find victim: prioritize alternate path entries, then use LRU within category
    return std::min_element(begin, end, 
      [has_alternate_path](const uop_cache_entry_t& a, const uop_cache_entry_t& b) {
        // If alternate path entries exist in this set
        if (has_alternate_path) {
          // Prioritize evicting alternate path entries
          if (a.alternate_path_prefetch != b.alternate_path_prefetch) {
            return a.alternate_path_prefetch;  // Evict 'a' if it has alternate bit
          }
        }
        // Among same category (both alternate or both regular), use LRU
        return a.last_used < b.last_used;
      });
  }
  
  uop_cache_t::iterator Hotloop(uop_cache_t::iterator begin, uop_cache_t::iterator end)
  {
    return std::min_element(begin, end, [](uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.hotness < b.hotness; });
  }

  uop_cache_t::iterator EvictLRUHotness(uop_cache_t::iterator begin, uop_cache_t::iterator end)
  {
    std::vector<uint64_t> last_used_vector;

    for (auto it = begin; it != end; ++it) {
      last_used_vector.push_back((*it).last_used);
    }

    assert(last_used_vector.size() == ways);

    std::sort(last_used_vector.begin(), last_used_vector.end());
    std::vector<uint64_t> least_4(last_used_vector.begin(), last_used_vector.begin() + 4);

    auto victim = begin;

    for (auto it = begin; it != end; ++it) {
      for (auto& x : least_4) {
        if ((*it).last_used == x && (*it).hotness < (*victim).hotness) {
          victim = it;
        }
      }
    }

    assert(begin <= victim);
    assert(victim < end);
    return victim;
  }

  uop_cache_t::iterator EvictLRUCritical(uop_cache_t::iterator begin, uop_cache_t::iterator end, bool critical)
  {
    std::vector<uop_cache_entry_t> set_vec;
    for (auto it = begin; it != end; ++it) {
      set_vec.push_back(*it);
    }
    assert(set_vec.size() == ways);
    std::sort(set_vec.begin(), set_vec.end(), compare_lru);
    std::vector<uop_cache_entry_t> least_4(set_vec.begin(), set_vec.begin() + 4);

    // try to remove least 4 lru which is not critical
    for (auto x = least_4.begin(); x != least_4.end(); ++x) {
      for (auto it = begin; it != end; ++it) {
        if ((x->ip == it->ip && x->last_used == it->last_used && !it->is_critical)) {
          return it;
        }
      }
    }

    // if above does not find any way try to remove any lru with no confident
    for (auto x = set_vec.begin(); x != set_vec.end(); ++x) {
      for (auto it = begin; it != end; ++it) {
        if ((x->ip == it->ip && x->last_used == it->last_used && it->is_critical && !it->critical_hotness.confident())) {
          return it;
        }
      }
    }

    // if all fails remove the lru
    return std::min_element(begin, end, [](uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.last_used < b.last_used; });
  }

  uop_cache_t::iterator EvictHotloopCritical(uop_cache_t::iterator begin, uop_cache_t::iterator end, bool critical)
  {
    std::vector<uop_cache_entry_t> set_vec;
    for (auto it = begin; it != end; ++it) {
      set_vec.push_back(*it);
    }
    assert(set_vec.size() == ways);
    std::sort(set_vec.begin(), set_vec.end(), compare_hotness);
    std::vector<uop_cache_entry_t> least_4(set_vec.begin(), set_vec.begin() + 4);

    for (auto x = least_4.begin(); x != least_4.end(); ++x) {
      for (auto it = begin; it != end; ++it) {
        if (x->ip == it->ip && x->hotness == it->hotness && !it->is_critical)
          return it;
      }
    }

    for (auto x = set_vec.begin(); x != set_vec.end(); ++x) {
      for (auto it = begin; it != end; ++it) {
        if ((x->ip == it->ip && x->hotness == it->hotness && it->is_critical && !it->critical_hotness.confident())) {
          return it;
        }
      }
    }

    return std::min_element(begin, end, [](uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.hotness < b.hotness; });
  }

  static bool compare_lru(uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.last_used < b.last_used; }

  static bool compare_hotness(uop_cache_entry_t& a, uop_cache_entry_t& b) { return a.hotness < b.hotness; }

  uint64_t getTag(uint64_t ip) { return (ip >> lg2(window)); }
};

inline MicroOpCache* m_microop_cache_ptr = new MicroOpCache();

#endif // MICROOPCACHE