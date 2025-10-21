#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
  constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
  constexpr std::size_t COUNTER_BITS = 2;
  constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

  std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
  std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> gs_history_table;

  std::size_t gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
  {
    std::size_t hash = bh_vector.to_ullong();
    hash ^= ip;
    hash ^= ip >> GLOBAL_HISTORY_LENGTH;
    hash ^= ip >> (GLOBAL_HISTORY_LENGTH * 2);

    return hash % GS_HISTORY_TABLE_SIZE;
  }
} // namespace

void O3_CPU::initialize_branch_predictor() {
  std::cout << "CPU " << cpu << " GSHARE branch predictor" << std::endl;
}

int O3_CPU::save_histogram(uint64_t pc, uint8_t level) {
}

std::pair<uint8_t, extra_branch_info> O3_CPU::predict_branch(uint64_t ip, uint8_t level, uint8_t branch_type, uint8_t branch_taken, bool need_only_h2p)
{
  extra_branch_info br_info;
  auto gs_hash = ::gs_table_hash(ip, ::branch_history_vector[this]);
  auto value = ::gs_history_table[this][gs_hash];

  // if (level == 2)
  //   std::cout << "Predict Branch ip " << std::hex << ip << " taken " << std::dec << (value.value() >= (value.maximum / 2)) << std::dec << " gs_hash " << gs_hash << std::endl;

  return { value.value() >= (value.maximum / 2),br_info };
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  //std::cout << "Update Branch ip " << std::hex << ip << std::dec << " taken " << int(taken) << " type " << int(branch_type) << std::endl;

  auto gs_hash = gs_table_hash(ip, ::branch_history_vector[this]);
  ::gs_history_table[this][gs_hash] += taken ? 1 : -1;

  // update branch history vector
  ::branch_history_vector[this] <<= 1;
  ::branch_history_vector[this][0] = taken;
}
void O3_CPU::br_speculative_begin(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, uint8_t level) {
}

void O3_CPU::br_speculative_end() {
}

int O3_CPU::get_yout(uint64_t pc, uint8_t level) {
  return 0;
}