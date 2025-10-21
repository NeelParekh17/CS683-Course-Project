/*
 * This file implements a two-level Branch Target Buffer (BTB), a Return Address Stack (RAS), and a two-level indirect target branch prediction.
 */

#include "alt_ittage.h"
#include "basic_btb.h"
#include "h2p_btb.h"
#include "h2p_indirect.h"
#include "h2p_ras.h"
#include "ittage_64KB.h"
#include "ooo_cpu.h"
#include "ras.h"
#include "tagged_indirect.h"
#include "defines.h"

BasicBTB<8192, 8> m_btb[NUM_CPUS]; // L2 BTB 64Ke
//BasicBTB<1600, 8> m_btb[NUM_CPUS]; // L2 BTB 128Ke
my_predictor* m_ittage[NUM_CPUS];  // 64KB ITTAGE
RAS<64, 1024> ras[NUM_CPUS];

#ifdef USE_ALT_INDIRECT_PREDICTOR
alt_ittage* h2p_ind;
#endif

H2PBasicBTB<64, 8> h2p_btb[NUM_CPUS]; // 4KB

//alternate RAS size comes from the bin generation script
H2P_RAS<ALT_RAS_SIZE, 1024> h2p_ras[NUM_CPUS];
bool speculate_state = false;

void O3_CPU::initialize_btb()
{
  std::cout << "BTB:" << std::endl;
  m_btb[cpu].initialize();
  std::cout << "Indirect:" << std::endl;
  std::cout << "RAS:" << std::endl;
  ras[cpu].initialize();
  m_ittage[cpu] = new my_predictor();

#ifdef USE_ALT_INDIRECT_PREDICTOR
  h2p_ind = new alt_ittage();
#endif

  h2p_btb[cpu].initialize();
  h2p_ras[cpu].initialize();
}

extra_branch_info br_info;
std::pair<std::pair<uint64_t, uint8_t>, extra_branch_info> O3_CPU::btb_prediction(uint64_t ip, uint8_t level)
{

  // // //do a 2 bit shift
  // ip = ip >> 2;

  br_info.is_h2p = false;

  if (level == 1) {
    // auto btb_pred = h2p_btb[cpu].predict(ip); //use alternate BTB
    auto btb_pred = m_btb[cpu].predict(ip); // use main BTB
    if (btb_pred.first == 0) {
      // no prediction for this IP
      return std::make_pair(std::make_pair(0, false), br_info); // no prediction for this IP
    }
    if (btb_pred.second == BRANCH_INFO_INDIRECT) {
#ifdef USE_ALT_INDIRECT_PREDICTOR
      return std::make_pair(std::make_pair(h2p_ind->predict_brindirect(ip), true), br_info);
#endif
    } else if (btb_pred.second == BRANCH_INFO_RETURN) {
      return std::make_pair(std::make_pair(h2p_ras[cpu].predict(), true), br_info);
    } else {
      return std::make_pair(std::make_pair(btb_pred.first, btb_pred.second != BRANCH_INFO_CONDITIONAL), br_info);
    }
  } else {
    auto btb_pred = m_btb[cpu].predict(ip);
    if (btb_pred.first == 0) {
      // no prediction for this IP
      return std::make_pair(std::make_pair(0, false), br_info); // no prediction for this IP
    }
    if (btb_pred.second == BRANCH_INFO_INDIRECT) {
      return std::make_pair(std::make_pair(m_ittage[cpu]->predict_brindirect(ip), true), br_info);
    } else if (btb_pred.second == BRANCH_INFO_RETURN) {
      return std::make_pair(std::make_pair(ras[cpu].predict(), true), br_info);
    } else {
      return std::make_pair(std::make_pair(btb_pred.first, btb_pred.second != BRANCH_INFO_CONDITIONAL), br_info);
    }
  }
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, bool alt_path)
{
  // //do a 2 bit shift
  // ip = ip >> 2;

  // cout << __func__ << " ip " << ip << endl;
  if (speculate_state) {

#ifdef USE_ALT_INDIRECT_PREDICTOR
    // update Indirect predictor
    h2p_ind->update_brindirect(ip, branch_type, taken, branch_target);
    h2p_ind->fetch_history_update(ip, branch_type, taken, branch_target);
#endif
    // alternate RAS
    h2p_ras[cpu].update(ip, branch_target, taken, branch_type);

  } else {

    // train only on alt path
    if (alt_path) {
#ifdef USE_ALT_INDIRECT_PREDICTOR
      h2p_ind->update_brindirect(ip, branch_type, taken, branch_target);
      h2p_ind->fetch_history_update(ip, branch_type, taken, branch_target);
#endif
    } else {
#ifdef USE_ALT_INDIRECT_PREDICTOR
      h2p_ind->fetch_history_update(ip, branch_type, taken, branch_target);
#endif
    }

    // RAS should be updated all the time
    h2p_ras[cpu].update(ip, branch_target, taken, branch_type);

    // main BTB updates
    m_btb[cpu].update(ip, branch_target, taken, branch_type);
    m_ittage[cpu]->update_brindirect(ip, branch_type, taken, branch_target);
    m_ittage[cpu]->fetch_history_update(ip, branch_type, taken, branch_target);
    ras[cpu].update(ip, branch_target, taken, branch_type);
  }
}

void O3_CPU::update_bb_size(uint64_t ip, uint8_t level, uint64_t bb_size, bool taken, uint8_t branch_type, uint64_t branch_history)
{
  // //cout << __func__ << " ip " << ip << endl;
  // l2_btb[cpu].m_update_bb_size(ip, bb_size, taken);
  // l2_ind[cpu].m_update_bb_size(ip, bb_size, branch_type, taken, branch_history);
}

std::pair<uint64_t, uint64_t> O3_CPU::btb_bb_size(uint64_t ip, uint8_t level)
{
  // auto btb_pred = l2_btb[cpu].predict(ip);
  // if (btb_pred.second == BRANCH_INFO_INDIRECT) {
  //   if (speculate_state)
  //     return spec_l2_ind[cpu].get_bb_size(ip);
  //   else
  //     return l2_ind[cpu].get_bb_size(ip);
  // } else {
  //   return l2_btb[cpu].get_bb_size(ip);
  // }
  return std::pair{0, 0};
}

void O3_CPU::speculative_begin(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type, bool alt_path)
{
  // update the history of prefetching branch and then move forward
  speculate_state = true;
#ifdef USE_ALT_INDIRECT_PREDICTOR
  h2p_ind->speculative_begin();
#endif
  h2p_ras->speculative_begin();
}

void O3_CPU::speculative_end()
{
  speculate_state = false;
#ifdef USE_ALT_INDIRECT_PREDICTOR
  h2p_ind->speculative_end();
#endif
  h2p_ras->speculative_end();
}

uint8_t O3_CPU::get_branch_type(uint64_t ip) { return m_btb->get_branch_type(ip); }

uint8_t O3_CPU::get_type(uint64_t ip) {}
