#include <algorithm>
#include <array>
#include <bitset>
#include <map>
#include "batage.h"

#include "msl/fwcounter.h"
#include "ooo_cpu.h"
using namespace single_level_batage;

batage pred;
histories hist;

void O3_CPU::initialize_branch_predictor() {
  std::cout << "CPU " << cpu << " BATAGE branch predictor Hist: " << (hist.size() / 8) << "B Pred: " << (pred.size() / 8) / 1000
    << "KB total:" << ((hist.size() / 8) / 1000) + ((pred.size() / 8) / 1000) << "KB "
    << std::endl;
}

int O3_CPU::save_histogram(uint64_t pc, uint8_t level) {
}

std::pair<uint8_t, extra_branch_info> O3_CPU::predict_branch(uint64_t ip, uint8_t level, uint8_t branch_type, uint8_t branch_taken, bool need_only_h2p)
{
  extra_branch_info br_info;
  return { pred.predict(ip,hist, ip),br_info };
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  
  if (branch_type == BRANCH_CONDITIONAL) {
    pred.update(ip, taken, hist, false);
    hist.update(branch_target, taken);
  } else {
    //update for non conditional branches
    hist.update(branch_target, taken);
  }
}

void O3_CPU::br_speculative_begin(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, uint8_t level) {
}

void O3_CPU::br_speculative_end() {
}

int O3_CPU::get_yout(uint64_t pc, uint8_t level) {
  return 0;
}