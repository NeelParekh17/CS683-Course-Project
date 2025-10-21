#include <algorithm>
#include <map>
#include <vector>

#include "cache.h"

struct hotloop_entry {
  uint64_t hotness_counter;
  uint64_t last_used;
};

namespace
{
  std::map<CACHE*, std::vector<hotloop_entry>> hotness;
}

void CACHE::initialize_replacement() {
  ::hotness[this] = std::vector<hotloop_entry>(NUM_SET * NUM_WAY);
  for (auto& x : ::hotness[this]) {
    x.hotness_counter = 0;
    x.last_used = 0;
  }
}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  auto begin = std::next(std::begin(::hotness[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant

  std::vector<uint64_t> last_used_vector;

  for (auto it = begin; it != end; it++) {
    last_used_vector.push_back((*it).last_used);
  }

  assert(last_used_vector.size() == NUM_WAY);

  std::sort(last_used_vector.begin(), last_used_vector.end());
  std::vector<uint64_t> least_4(last_used_vector.begin(), last_used_vector.begin() + 4);

  auto victim = begin;

  for (auto it = begin; it != end; ++it) {
    for (auto& x : least_4) {
      if ((*it).last_used == x && (*it).hotness_counter < (*victim).hotness_counter) {
        victim = it;
      }
    }
  }

  assert(begin <= victim);
  assert(victim < end);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by prior asserts
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
  uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (!hit || type != WRITE) // Skip this for writeback hits
  {
    hotloop_entry& update = ::hotness[this].at(set * NUM_WAY + way);

    update.hotness_counter++;
    update.last_used = current_cycle;

    if (update.hotness_counter >= 512) {
      auto begin = std::next(std::begin(::hotness[this]), set * NUM_WAY);
      auto end = std::next(begin, NUM_WAY);
      std::for_each(begin, end, [](auto& x) {
        hotloop_entry& entry = x;
      entry.hotness_counter = entry.hotness_counter / 2;
        });
      update.hotness_counter++;
    }
  }
}
 
void CACHE::replacement_final_stats() {}
