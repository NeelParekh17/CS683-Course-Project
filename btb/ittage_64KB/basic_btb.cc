/*
 * This file implements a basic Branch Target Buffer (BTB) structure, a Return Address Stack (RAS), and an indirect target branch prediction.
 */

#include "ooo_cpu.h"
#include "basic_btb.h"
#include "ittage_64KB.h"
#include "ras.h"

 //BasicBTB<8*1024, 8> btb[NUM_CPUS]; //64k
BasicBTB<4 * 1024, 8> btb[NUM_CPUS]; //32k
my_predictor* ittage[NUM_CPUS];
//RAS<128, 4096> ras[NUM_CPUS];
RAS<64, 4096> ras[NUM_CPUS];

void O3_CPU::initialize_btb()
{
  std::cout << "BTB:" << std::endl;
  btb[cpu].initialize();
  std::cout << "Indirect:" << std::endl;
  ittage[cpu] = new my_predictor();
  std::cout << "RAS:" << std::endl;
  ras[cpu].initialize();
}

extra_branch_info br_info_ittage;
std::pair<std::pair<uint64_t, uint8_t>, extra_branch_info>  O3_CPU::btb_prediction(uint64_t ip, uint8_t level)
{
  auto btb_pred = btb[cpu].predict(ip);
  if (btb_pred.first == 0) {
    // no prediction for this IP
    return std::make_pair(std::make_pair(0, false), br_info_ittage);
  }
  if (btb_pred.second == BRANCH_INFO_INDIRECT) {
    return std::make_pair(std::make_pair(ittage[cpu]->predict_brindirect(ip), true), br_info_ittage);
  } else if (btb_pred.second == BRANCH_INFO_RETURN) {
    return std::make_pair(std::make_pair(ras[cpu].predict(), true), br_info_ittage);
  } else {
    return std::make_pair(std::make_pair(btb_pred.first, btb_pred.second != BRANCH_INFO_CONDITIONAL), br_info_ittage);
  }
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, bool alt_path)
{
  ittage[cpu]->update_brindirect(ip, branch_type, taken, branch_target);
  ittage[cpu]->fetch_history_update(ip, branch_type, taken, branch_target);
  ras[cpu].update(ip, branch_target, taken, branch_type);
  btb[cpu].update(ip, branch_target, taken, branch_type);
}
void O3_CPU::update_bb_size(uint64_t ip, uint8_t level, uint64_t bb_size, bool taken, uint8_t branch_type, uint64_t branch_history)
{

}
std::pair<uint64_t, uint64_t> O3_CPU::btb_bb_size(uint64_t ip, uint8_t level)
{

}
void O3_CPU::speculative_begin(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, bool alt_path)
{

}
void O3_CPU::speculative_end()
{

}
uint8_t O3_CPU::get_branch_type(uint64_t ip)
{

}
uint8_t O3_CPU::get_type(uint64_t ip)
{
}