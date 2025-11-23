/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "defines.h"
#include "instruction.h"
#include "micro_op_cache.h"
#include "mrc.h"
#include "ooo_cpu.h"
#include "profiler.h"

using namespace std;

constexpr uint64_t DEADLOCK_CYCLE = 1000000;
constexpr uint64_t UPDATE_CYCLE = 1000000;
constexpr uint64_t UOP_MSHR_DEADLOCK_CYCLE = 1000000;

constexpr uint64_t MAX_NON_TAKEN_BRANCHES_PER_CYCLE = 2;

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time();
#define EXTRACT_BITS(x, n) ((x) & ((1ULL << (n)) - 1ULL))

void O3_CPU::operate()
{
  if (!IFETCH_BUFFER.front().checked)
  {
    IFETCH_BUFFER.front().checked = true;
    get_profiler_ptr->total_ftq_stalls++;
    IFETCH_BUFFER.front().already_fetched = IFETCH_BUFFER.front().fetched;
    if (IFETCH_BUFFER.front().fetched != COMPLETED && !IFETCH_BUFFER.front().uop_cache_hit)
    { // FTQ head is not fetched so it will stall
      get_profiler_ptr->hpca.ftq_head_stalled++;
    }
    if (std::size(DECODE_BUFFER) >= DECODE_BUFFER_SIZE)
    {
      IFETCH_BUFFER.front().decode_full = true;
    }
    else
    {
      IFETCH_BUFFER.front().decode_full = false;
    }
  }

  if (IFETCH_BUFFER.front().fetched != COMPLETED && !IFETCH_BUFFER.front().uop_cache_hit)
  {
    get_profiler_ptr->avg_time_ftq_head_stalled++;
  }

  num_branch_this_cycle = 0;

  if (current_cycle % UOP_CACHE_NUM_TICKS == 0)
  {
    m_microop_cache_ptr->UpdateHotness();
  }

  instrs_to_read_this_cycle = std::min<std::size_t>(FETCH_WIDTH, IFETCH_BUFFER_SIZE - std::size(IFETCH_BUFFER));
  if (IFETCH_BUFFER_SIZE == std::size(IFETCH_BUFFER))
  {
    get_profiler_ptr->cpu_stalls.ftq_full++;
  }

  allow_pref_to_use_decoders_cp = false;
  allow_pref_to_use_decoders_wp = false;

  operate_on_uop_mshr_queue();     // issue prefetch requests from MSHR
  retire_rob();                    // retire
  complete_inflight_instruction(); // finalize execution
  execute_instruction();           // execute instructions
  schedule_instruction();          // schedule instructions
  handle_memory_return();          // finalize memory transactions
  operate_lsq();                   // execute memory transactions

  promote_to_dispatch_from_decoders();

  // stream_instruction(); // uop cache instructions goes to dispatch buffer
  decode_instruction(); // decode
  promote_to_decode();

  // if we had a branch mispredict, turn fetching back on after the branch
  // mispredict penalty

  if ((fetch_stall == 1) && (current_cycle < fetch_resume_cycle))
  { // fetch stoped due to mispredicted branch
    branch_miss_front_end_stall = true;
  }

  if ((fetch_stall == 1) && (current_cycle >= fetch_resume_cycle) && (fetch_resume_cycle != 0))
  {
    fetch_stall = 0;
    fetch_resume_cycle = 0;
    assert(!mark_next_instr);
    mark_next_instr = true;
    branch_miss_front_end_stall = false;

    bool begin_recovery = false;

#ifdef IDEAL_BRANCH_RECOVERY
    begin_recovery = true;
#endif

#ifdef REAL_BRANCH_RECOVERY
    begin_recovery = true;
#endif

    // #ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND
    if (last_br_miss_type == BRANCH_CONDITIONAL)
    {
      begin_recovery = true;
      get_profiler_ptr->number_of_brs++;
      start_stats = true;
      stats_bin = 0;
      miss_bins = 0;
    }
// #endif
#ifdef IDEAL_BRANCH_RECOVERY_ONLY_IND
    if (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL)
    {
      begin_recovery = true;
    }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND_IND
    if (last_br_miss_type == BRANCH_CONDITIONAL || last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL)
    {
      begin_recovery = true;
    }
#endif

    if (begin_recovery)
    {
      get_profiler_ptr->total_instruction_recovered += total_critical_instruction;
      get_profiler_ptr->num_of_recoveries++;
      last_recovered_ip = 0;
      uop_cache_windows_recovered = 0;
      total_critical_instruction = 0;
      total_branches_saved = 0;
      begin_branch_recovery = true;
    }
  }
  fetch_instruction(); // fetch
  check_dib();
  initialize_instruction();
  increment_alternate_path();
  decode_prefetched_instruction();

  // heartbeat
  if (show_heartbeat && (num_retired >= next_print_instruction))
  {
    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();

    auto heartbeat_instr{std::ceil(num_retired - last_heartbeat_instr)};
    auto heartbeat_cycle{std::ceil(current_cycle - last_heartbeat_cycle)};

    auto phase_instr{std::ceil(num_retired - begin_phase_instr)};
    auto phase_cycle{std::ceil(current_cycle - begin_phase_cycle)};

    std::cout << "Heartbeat CPU " << cpu << " instructions: " << num_retired << " cycles: " << current_cycle;
    std::cout << " heartbeat IPC: " << heartbeat_instr / heartbeat_cycle;
    std::cout << " cumulative IPC: " << phase_instr / phase_cycle;
    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
    next_print_instruction += STAT_PRINTING_PERIOD;

    last_heartbeat_instr = num_retired;
    last_heartbeat_cycle = current_cycle;
    get_profiler_ptr->test_print();
  }
}

void O3_CPU::initialize()
{
  // BRANCH PREDICTOR & BTB
  impl_initialize_branch_predictor();
  impl_initialize_btb();
}

void O3_CPU::begin_phase()
{
  begin_phase_instr = num_retired;
  begin_phase_cycle = current_cycle;

  // Record where the next phase begins
  stats_type stats;
  stats.name = "CPU " + std::to_string(cpu);
  stats.begin_instrs = num_retired;
  stats.begin_cycles = current_cycle;
  sim_stats.push_back(stats);
}

void O3_CPU::end_phase(unsigned finished_cpu)
{
  // Record where the phase ended (overwrite if this is later)
  sim_stats.back().end_instrs = num_retired;
  sim_stats.back().end_cycles = current_cycle;

  if (finished_cpu == this->cpu)
  {
    finish_phase_instr = num_retired;
    finish_phase_cycle = current_cycle;

    roi_stats.push_back(sim_stats.back());
  }
}

void O3_CPU::initialize_instruction()
{
  int demand_this_cycle = instrs_to_read_this_cycle;
  uint64_t start_ip = input_queue.front().ip;
  bool needs_decoders = false;

  // Saving demand IP on branch misprediction for bank conflict
  if (fetch_stall == 1)
  {
    auto fetch_w = FETCH_WIDTH;
    demand_ip_info to_add;
    while (fetch_w > 0)
    {
      auto btb_info = impl_btb_prediction(last_demand_ip, 2);
      get_profiler_ptr->detailed_energy.addr_gen_events++;
      auto m_target = btb_info.first.first;
      auto always_taken = btb_info.first.second;
      // cout << "Spec generate! " << last_demand_ip << endl;
      // FIXME
      // this following will disrupt the branch predictor
      // but as after this another prediction is called which overlaps all the updates done by the below
      auto [m_prediction, add_br_info] = impl_predict_branch(last_demand_ip, 2, 0, 0, true);
      if ((always_taken || m_prediction) && m_target > 0)
      {
        to_add.ip = m_target;
        to_add.btb_contention_checked = false;
        to_add.dib_contention_checked = false;
        demand_ips.push_back(to_add);
        if (demand_ips.size() > 1024)
        {
          demand_ips.pop_front();
        }
        last_demand_ip = m_target;
      }
      else
      {
        get_profiler_ptr->detailed_energy.addr_gen_events++;
        to_add.ip = last_demand_ip + 4;
        to_add.btb_contention_checked = false;
        to_add.dib_contention_checked = false;
        demand_ips.push_back(to_add);
        if (demand_ips.size() > 1024)
        {
          demand_ips.pop_front();
        }
        last_demand_ip = last_demand_ip + 4;
      }

      // check the uop cache to see which fetch mode we are
      auto hit = m_microop_cache_ptr->Lookup(to_add.ip, current_cycle);
      if (!hit)
      {
        // cout << "Miss found in WP " << needs_decoders << endl;
        needs_decoders = true;
      }
      fetch_w--;
    }

    if (!needs_decoders)
    {
      allow_pref_to_use_decoders_wp = true;
      // cout << "Allow alternate to decode WP! " << current_cycle << endl;
    }
  }
  else
  { // if it is not branch miss then just add number of instructions possible depending on the size of the FTQ
    while (demand_this_cycle > 0)
    {
      get_profiler_ptr->detailed_energy.addr_gen_events++;
      demand_ip_info to_add;
      to_add.ip = start_ip;
      to_add.btb_contention_checked = false;
      to_add.dib_contention_checked = false;
      demand_ips.push_back(to_add);
      if (demand_ips.size() > 1024)
      {
        demand_ips.pop_front();
      }
      start_ip = start_ip + 4;
      demand_this_cycle--;
    }
  }

  check_contention();

  // cout << "contention updated " << endl;

  if (fetch_stall == 0)
  {
    get_profiler_ptr->demand_btb_cycle++;
  }

  // Save regular demand IP's for bank conflict
  if (!alt_got_priority)
  {
    while (fetch_stall == 0 && instrs_to_read_this_cycle > 0 && !std::empty(input_queue))
    {
      do_init_instruction(input_queue.front());
      get_profiler_ptr->demand_check_btb++;
      last_demand_ip = input_queue.front().ip;
      input_queue.pop_front();
      // cout << "do_init_instruction " << alt_got_priority << " instrs_to_read_this_cycle " << int(instrs_to_read_this_cycle) << endl;
    }
  }
  else
  { // do not issue demand from conflict banks
    get_profiler_ptr->pref_got_pref++;
    while (fetch_stall == 0 && instrs_to_read_this_cycle > 0 && !std::empty(input_queue))
    {
      if (!conflict_banks[EXTRACT_BITS((input_queue.front().ip >> 2), lg2(BTB_BANKS))])
      {
        do_init_instruction(input_queue.front());
        get_profiler_ptr->demand_check_btb++;
        last_demand_ip = input_queue.front().ip;
        input_queue.pop_front();
        // cout << "do_init_instruction " << alt_got_priority << " instrs_to_read_this_cycle " << int(instrs_to_read_this_cycle) << endl;
      }
      else
      {
        break;
      }
    }
  }

  // cout << __func__ << " @" << current_cycle << endl;

  alt_got_priority = false;
}

#define MIN_ALT 8
#define MIN_DMD 12

#define DEBUG_BTB_CONTENTION

O3_CPU::BTB_Priority O3_CPU::determinePriority(uint64_t alt_start_ip, uint64_t demand_start_ip)
{
  int conflictStart = BTB_BANKS;
  for (int i = 0; i < BTB_BANKS; ++i)
  {
    if (conflict_banks[i])
    {
      if (i < conflictStart)
      {
        conflictStart = i;
      }
    }
  }

  // alternate start should be the first bank of the first ip?
  auto alternateStart = EXTRACT_BITS((alt_start_ip >> 2), lg2(BTB_BANKS));
  auto demandStart = EXTRACT_BITS((demand_start_ip >> 2), lg2(BTB_BANKS));

  // case 1 (demand access starting IP's) : demand needs banks 5 - 20, alt needs banks 25 - 7, so banks 5 - 7 have a conflict
  // Case 2 (alternate access starting IP's) : demand needs 25 - 7, alt needs 5 - 30, 5 - 7 have a conflict
  // If Case 1 and alt can do at least N_1 if we let demand win the conflict banks, the let demand win
  // Else if Case 2 and demand can do at least N_2 if we let alt win the conflict banks, then let alt win

  // Case 1 where demand can do at least MIN_DMD if all the conflict is given to alternate
  // as average BB size is about 8 to 9 so if we can give conflict banks to prefetch it should be fine

  // for example demand start from 15, and conflict occur at 23.
  // now if we give all conflict to alternate the demand can still do 23-15= 8 which is == MIN_DMD so give aways the
  // conflict banks with some probability
  if (demandStart < conflictStart && (conflictStart - demandStart) >= MIN_DMD)
  {
    if (rand() % 100 < BTB_CONF_PROB)
    {
      return ALTERNATE_PRIORITY;
    }
  }
  return NO_PRIORITY;
}

void O3_CPU::check_contention()
{
  if (ips_to_prefetch.empty())
  {
    return;
  }

  // cout << __func__ << " @" << current_cycle << endl;

  bool conflict_free = false;
  int num_of_branches = 0;
  int num_of_taken_branches = 0;
  priority_this_cycle = NO_PRIORITY;
  alt_got_priority = false;
  bool is_head = true;

  // btb contention check
  for (int i = 0; i < BTB_BANKS; i++)
  {
    demand_banks[i] = false;
    alternate_banks[i] = false;
    conflict_banks[i] = false;
  }

  // first check which banks are used by the demand
  auto demand_begin_ip = std::find_if(std::begin(demand_ips), std::end(demand_ips), [](const demand_ip_info &x)
                                      { return !x.btb_contention_checked; });
  uint8_t alternate_fetch_width = FETCH_WIDTH;
  while (alternate_fetch_width > 0)
  {
    if (demand_ips.empty())
    {
      break;
    }
    auto demand_begin = std::find_if(std::begin(demand_ips), std::end(demand_ips), [](const demand_ip_info &x)
                                     { return !x.btb_contention_checked; });
    //  check which bank is used
    if (demand_begin != demand_ips.end())
    {
      demand_banks[EXTRACT_BITS((demand_begin->ip >> 2), lg2(BTB_BANKS))] = true;
      // cout << "Demand IP " << demand_begin->ip << " Bank " << EXTRACT_BITS((demand_begin->ip >> 2), lg2(BTB_BANKS)) << endl;
      demand_begin->btb_contention_checked = true;
    }
    alternate_fetch_width--;
  }

  alternate_fetch_width = FETCH_WIDTH;
  auto pref_begin = std::find_if(std::begin(ips_to_prefetch), std::end(ips_to_prefetch), [](const ip_pref_entry &x)
                                 { return !x.btb_checked; });
  uint64_t alt_start_ip = pref_begin->ip;
  while (alternate_fetch_width > 0)
  {
    alternate_banks[EXTRACT_BITS((alt_start_ip >> 2), lg2(BTB_BANKS))] = true;
    // cout << "Alternate IP " << alt_start_ip << " Bank " << EXTRACT_BITS((alt_start_ip >> 2), lg2(BTB_BANKS)) << endl;
    alt_start_ip += 4;
    alternate_fetch_width--;
  }

  for (int i = 0; i < BTB_BANKS; i++)
  {
    if (demand_banks[i] && (demand_banks[i] == alternate_banks[i]))
    {
      conflict_banks[i] = true;
      // cout << "Conflict Bank " << i << endl;
    }
  }

  // priority_this_cycle = determinePriority(pref_begin->ip, demand_begin_ip->ip);
  assert(priority_this_cycle == NO_PRIORITY);

  // now try to process the prefetch requests that uses a bank that is free
  alternate_fetch_width = FETCH_WIDTH;
  while (alternate_fetch_width > 0)
  {
    bool btb_conflict_free = false;
    if (demand_ips.empty())
    {
      btb_conflict_free = true;
    }

    auto pref_begin = std::find_if(std::begin(ips_to_prefetch), std::end(ips_to_prefetch), [](const ip_pref_entry &x)
                                   { return !x.btb_checked; });
    // all checked
    if (pref_begin == ips_to_prefetch.end())
    {
      break;
    }

    // update the cycle the entry tries to check for BTB
    if (is_head && pref_begin->btb_check_cycle == 0)
    {
      // cout << "Head checked " << pref_begin->ip << " @" << current_cycle << endl;
      pref_begin->btb_check_cycle = current_cycle;
      pref_begin->is_head = true;
    }

    if (priority_this_cycle == NO_PRIORITY)
    // if bank is not used then only use it
    {
      if (!demand_banks[EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS))])
      {
        btb_conflict_free = true;
      }
      else if (alt_got_priority && conflict_banks[EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS))])
      {
        btb_conflict_free = true;
      }
      else if (pref_begin->is_head && pref_begin->last_cycle_to_try_btb != current_cycle)
      { // conflict so update the probability
        pref_begin->probability += BTB_CONF_PROB;
        if (pref_begin->probability >= 100)
        {
          btb_conflict_free = true;
          alt_got_priority = true;
          //   cout << "COnflict free because of the prob " << pref_begin->ip << " Bank " << EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS)) << "
          //   btb_conflict_free " << int(btb_conflict_free)
          //  << endl;
        }
      }
    }

    // } else if (priority_this_cycle == ALTERNATE_PRIORITY) {
    //   if (!demand_banks[EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS))]) {
    //     btb_conflict_free = true;
    //     // cout << "COnflict free DMD " << pref_begin->ip << " Bank " << EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS)) << " alternate_fetch_width "
    //     //      << int(alternate_fetch_width) << endl;
    //   }
    //   // alternate gets the conflict banks
    //   if (conflict_banks[EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS))]) {
    //     // cout << "COnflict free CNF " << pref_begin->ip << " Bank " << EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS)) << " alternate_fetch_width "
    //     //      << int(alternate_fetch_width) << endl;
    //     btb_conflict_free = true;
    //   }

    //   alt_got_priority = true;
    // }

    pref_begin->last_cycle_to_try_btb = current_cycle;

#ifdef IDEAL_BTB_BANKING
    btb_conflict_free = true;
#endif

    // cout << "COnflict  " << pref_begin->ip << " Bank " << EXTRACT_BITS((pref_begin->ip >> 2), lg2(BTB_BANKS)) << " btb_conflict_free " <<
    // int(btb_conflict_free)
    //      << endl;

    if (btb_conflict_free)
    {
      pref_begin->btb_checked = true;
      get_profiler_ptr->detailed_energy.addr_gen_events++;
      if (is_head && pref_begin->btb_check_cycle != current_cycle)
      {
        // if (!warmup)
        // cout << "Head stalled for " << pref_begin->ip << std::dec << " cycles " << current_cycle - pref_begin->btb_check_cycle << endl;
        assert(current_cycle >= pref_begin->btb_check_cycle);
        get_profiler_ptr->btb_banking_stats[pref_begin->ip].first += current_cycle - pref_begin->btb_check_cycle;
        get_profiler_ptr->btb_banking_stats[pref_begin->ip].second++;
      }
    }

    get_profiler_ptr->conflict_stats.total_btb_conflict_check++;
    if (!btb_conflict_free)
    {
      get_profiler_ptr->conflict_stats.conflict_btb++;
    }

    if (pref_begin->is_branch)
    {
      num_of_branches++;
      if (pref_begin->is_taken)
      {
        num_of_taken_branches++;
      }
    }

    // we stop at 2 branches and 1 taken branches
    if (num_of_branches >= MAX_NON_TAKEN_BRANCHES_PER_CYCLE || num_of_taken_branches >= 1)
    {
      // cout << "Stoping at br " << endl;
      alternate_fetch_width = 0;
      break;
    }

    is_head = false;
    alternate_fetch_width--;
  }

  // cout << "check done " << endl;
  //  dib contention check
  //  Ok so 2 ports 2 banks means we will have 2 access per banks and once the port
  //  is used we do not do any more checks in that cycle.

  // And basically, at the start of the cycle, you know the 4 addresses you want to access, and you have 4 resources (2 in bank 0, 2 in bank 1)
  // First you process demands and this consumes resources, and because there are two ports those demand request are guaranteed to be performed
  // And then you check if there is enough resource this cycle for the prefetch ones, using the bank conflict logic you have(but with bit 5)

  // For instance let's say demand wants two windows that map to bank 0 and bank 0
  // Then first pf request wants window that maps to bank 1, and second pf request maps to bank 0
  // Then the two demand each get one of the two ports of bank 0
  // Then the first pf req gets first port of bank 1

  // meaning we will have 2 accesses for bank 0 in one cycle and 2 accesses for bank 1 in one cycle

  bool port0[2] = {false};
  bool port1[2] = {false};

  int port0_index = 0;
  int port1_index = 0;

  conflict_free = false;

  if (demand_ips.empty())
  {
    conflict_free = true;
  }

  uint8_t dib_ports = 2;
  // check the ports and bank used by the demand IP
  while (dib_ports > 0)
  {

    if (demand_ips.empty())
    {
      break;
    }

    auto demand_begin = std::find_if(std::begin(demand_ips), std::end(demand_ips), [](const demand_ip_info &x)
                                     { return !x.dib_contention_checked; });
    auto no_match_ip = [find_ip = demand_begin->ip](const demand_ip_info &x)
    {
      return ((find_ip >> lg2(SIZE_WINDOWS)) != (x.ip >> lg2(SIZE_WINDOWS)));
    };

    auto demand_end = std::find_if(demand_begin, std::end(demand_ips), no_match_ip);

    // if no demand ip found
    if (demand_begin == demand_ips.end())
    {
      conflict_free = true;
    }

    for (auto i = demand_begin; i != demand_end; i++)
    {
      i->dib_contention_checked = true;
    }

    // update which bank is used
    if (((demand_begin->ip >> 5) & ((1ULL << 1) - 1)) == 0)
    {
      port0[port0_index] = true;
      port0_index++;
    }
    else
    {
      port1[port1_index] = true;
      port1_index++;
    }

    dib_ports--;
  }

  // now check which port and bank is free to be used
  dib_ports = 2;
  while (dib_ports > 0)
  {
    auto pref_begin = std::find_if(std::begin(ips_to_prefetch), std::end(ips_to_prefetch), [](const ip_pref_entry &x)
                                   { return !x.dib_check_done; });
    auto pref_no_match_ip = [find_ip = pref_begin->ip](const ip_pref_entry &x)
    {
      return ((find_ip >> lg2(SIZE_WINDOWS)) != (x.ip >> lg2(SIZE_WINDOWS)));
    };

    auto pref_end = std::find_if(pref_begin, std::end(ips_to_prefetch), pref_no_match_ip);

    if (pref_begin == ips_to_prefetch.end())
    {
      break;
    }

    // if the port 1 is needed check that it is free
    if (((pref_begin->ip >> 5) & ((1ULL << 1) - 1)))
    {
      for (int i = 0; i < 2; i++)
      {
        if (!port1[i])
        {
          conflict_free = true;
          break;
        }
      }
    }
    else
    {
      for (int i = 0; i < 2; i++)
      {
        if (!port0[i])
        {
          conflict_free = true;
          break;
        }
      }
    }

#ifdef IDEAL_UOP_BANKING
    conflict_free = true;
#endif

    if (conflict_free)
    {
      for (auto i = pref_begin; i != pref_end; i++)
      {
        i->dib_check_done = true;
        if (m_microop_cache_ptr->Lookup(i->ip, current_cycle))
        {
          get_profiler_ptr->detailed_energy.dib_check++;
          i->dib_hit = true;
        }
        else
        {
          i->dib_hit = false;
        }
      }
    }

    get_profiler_ptr->conflict_stats.total_window_conflict_check++;
    if (!conflict_free)
    {
      get_profiler_ptr->conflict_stats.conflict_window++;
    }

    dib_ports--;
  }

  // cout << "DIB check done " << endl;
}

void O3_CPU::increment_alternate_path()
{
  uint64_t alternate_fetch_width = FETCH_WIDTH;
  uint8_t num_of_br_on_alt_path = 0;
  uint8_t num_of_taken_br_on_alt_path = 0;
  while (alternate_fetch_width > 0 && !ips_to_prefetch.empty())
  {

    auto prefetch_ready = [](const ip_pref_entry &x)
    {
      return !x.in_mshr && x.dib_check_done && x.btb_checked && !x.dib_hit;
    };

    auto pref_request = std::find_if(std::begin(ips_to_prefetch), std::end(ips_to_prefetch), prefetch_ready);
    auto no_match_ip = [find_ip = pref_request->ip](const ip_pref_entry &x)
    {
      return ((find_ip >> LOG2_BLOCK_SIZE) != (x.ip >> LOG2_BLOCK_SIZE));
    };

    auto pref_end = std::find_if(pref_request, std::end(ips_to_prefetch), no_match_ip);

    // nothing to prefetch here now as dib check needs to be done
    if (pref_request == pref_end)
    {
      return;
    }

    // add to MSHR if needed or update the ips to prefetch
    bool should_add = true;
    bool already_resolved = false;

    // cout << "Start prefetch with " << pref_request->ip << " end with " << pref_end->ip << endl;

    for (auto i = pref_request; i != pref_end; i++)
    {
      // cout << "Checking the range " << i->ip << " dib_checked " << i->dib_check_done << " dib_hit " << i->dib_hit << endl;
      if (i->br_already_resolved)
      {
        already_resolved = true;
        break;
      }
    }

    for (auto &mshr : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
    {
      // line is already so just add the ips to add and do not allocate a new MSHR entry
      if ((mshr.cache_line_addr) == (pref_request->ip >> LOG2_BLOCK_SIZE))
      {
        should_add = false;
        for (auto i = pref_request; i != pref_end; i++)
        {
          i->in_mshr = true;
          mshr.ips_in_line.push_back({i->ip, i->br_type});
          if (i->by_br_miss)
            mshr.pq_pkt.by_br_miss = i->by_br_miss;

          // ADDED: Even if one ip is on alternate path, set the entire corresponding MSHR entry as alternate path
          mshr.is_alternate_path = i->is_alternate_path;
        }
        break;
      }
    }

    if (should_add)
    {

      // restrict the number of entries in the uop cache mshr and try again
      if (static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.size() >= UOP_MSHR)
      {
        return;
      }

      // create the prefetch packet
      PACKET read_pkt;
      read_pkt.v_address = pref_request->ip;
      read_pkt.pref_by_instr = pref_request->by_isntr;
      read_pkt.instr_id = pref_request->by_isntr;
      read_pkt.ip = pref_request->ip;
      read_pkt.uop_pref = true;
      read_pkt.nested_prefetch = pref_request->nested_pref;
      read_pkt.by_ip = pref_request->ip;
      read_pkt.by_br_miss = pref_request->by_br_miss;

      // Add to MSHR to keep the inflight status
      CACHE::uop_mshr_entry to_mshr;
      to_mshr.cache_line_addr = (pref_request->ip >> LOG2_BLOCK_SIZE);
      to_mshr.event_cycle = current_cycle;
      to_mshr.is_valid = true; // by default we plan to add all the lines
      to_mshr.by_instr = pref_request->by_isntr;

      // ADDED THIS: Set alternate path flag in MSHR as false initially, in below loop if any ip is on alternate path, set it to true
      to_mshr.is_alternate_path = false;

      for (auto i = pref_request; i != pref_end; i++)
      { // add the ips to prefetch
        to_mshr.ips_in_line.push_back({i->ip, i->br_type});
        if (i->is_alternate_path)
        {
          to_mshr.is_alternate_path = true;
        }
        if (i->is_taken)
        {
          to_mshr.predicted_taken_branches.push_back(i->ip);
        }
        i->in_mshr = true;
      }

      // cout << "Adding done " << endl;
      to_mshr.pref_done = false;
      to_mshr.branch_resolved = already_resolved;
      to_mshr.ip = pref_request->ip;
      to_mshr.added_to_pq = false;
      to_mshr.pq_pkt = read_pkt;
      assert(to_mshr.ip != 0);
      static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.push_back(to_mshr);
    }

    for (auto i = pref_request; i != pref_end; i++)
    {

      // if (br_type == BRANCH_RETURN) {
      //   get_profiler_ptr->histogram_of_returns[i->by_isntr]++;
      // }

      if (i->is_branch)
      {
        num_of_br_on_alt_path++;
        if (i->is_taken)
        {
          num_of_taken_br_on_alt_path++;
        }
      }
    }

    // remove the prefetched part
    ips_to_prefetch.erase(pref_request, pref_end);

    // Note: here as we already have all the IPS to prefetch, we try to model the stopping condition
    //  as accurately as possible. So in some cycles we might fetch beyond 2 branches or beyond 1 take branch as far as
    //  they belong to same cache line (we process cache lines).

    // max number of branches we can process in one cycle
    if (num_of_br_on_alt_path >= MAX_NON_TAKEN_BRANCHES_PER_CYCLE)
    {
      alternate_fetch_width = 0;
      return;
    }

    // taken br should stop fetch
    if (num_of_taken_br_on_alt_path >= 1)
    {
      alternate_fetch_width = 0;
      return;
    }
    alternate_fetch_width--;
  }
}

void O3_CPU::do_init_instruction(ooo_model_instr &arch_instr)
{
  if ((arch_instr.ip - last_instr.ip) == 2)
  {
    // cout << "2B instructions! Curr IP " << arch_instr.ip << " last_instr_was_two_byte " << last_instr_was_two_byte << endl;
    // IFETCH_BUFFER.back().two_byte_instr = true;
    if (!last_instr_was_two_byte)
    {
      last_instr_was_two_byte = false;
      arch_instr.two_byte_instr = true;
    }
    last_instr_was_two_byte = true;
  }
  else
  {
    last_instr_was_two_byte = false;
  }
  last_instr = arch_instr;

  instrs_to_read_this_cycle--;

  // cout << __func__ << " instr_unique_id " << instr_unique_id << endl;
  arch_instr.instr_id = instr_unique_id;
  if (!arch_instr.two_byte_instr)
  {
    arch_instr.instr_id_4b = instr_unique_id_4b;
  }

  bool writes_sp = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
  bool writes_ip = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_INSTRUCTION_POINTER);
  bool reads_sp = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_STACK_POINTER);
  bool reads_flags = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_FLAGS);
  bool reads_ip = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_INSTRUCTION_POINTER);
  bool reads_other = std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), [](uint8_t r)
                                   { return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER; });

#define NEW_TRACE
  // New traces
  // determine what kind of branch this is, if any
#ifdef NEW_TRACE
  if (!reads_sp && !reads_flags && writes_ip && !reads_other)
  {
    // direct jump
    arch_instr.is_branch = true;
    arch_instr.branch_taken = true;
    arch_instr.branch_type = BRANCH_DIRECT_JUMP;
  }
  else if (!reads_sp && !reads_ip && !reads_flags && writes_ip && reads_other)
  { // } else if (!reads_sp && !reads_flags && writes_ip && reads_other) {
    // indirect branch
    arch_instr.is_branch = true;
    arch_instr.branch_taken = true;
    arch_instr.branch_type = BRANCH_INDIRECT;
  }
  else if (!reads_sp && reads_ip && !writes_sp && writes_ip && (reads_flags || reads_other))
  { // conditional branch
    arch_instr.is_branch = true;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_CONDITIONAL;
  }
  else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other)
  {
    // direct call
    arch_instr.is_branch = true;
    arch_instr.branch_taken = true;
    arch_instr.branch_type = BRANCH_DIRECT_CALL;
  }
  else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other)
  { // indirect call
    arch_instr.is_branch = true;
    arch_instr.branch_taken = true;
    arch_instr.branch_type = BRANCH_INDIRECT_CALL;
  }
  else if (reads_sp && !reads_ip && writes_sp && writes_ip)
  {
    // return
    arch_instr.is_branch = true;
    arch_instr.branch_taken = true;
    arch_instr.branch_type = BRANCH_RETURN;
  }
  else if (writes_ip)
  { // some other branch type that doesn't fit the above categories
    arch_instr.is_branch = true;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_OTHER;
  }
  else
  {
    arch_instr.branch_taken = false;
  }
#endif

#ifdef OLD_TRACE
  // old traces
  //  determine what kind of branch this is, if any
  if (!reads_sp && !reads_flags && writes_ip && !reads_other)
  {
    // direct jump
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_DIRECT_JUMP;
  }
  else if (!reads_sp && !reads_flags && writes_ip && reads_other)
  {
    // indirect branch
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_INDIRECT;
  }
  else if (!reads_sp && reads_ip && !writes_sp && writes_ip && reads_flags && !reads_other)
  {
    // conditional branch
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_CONDITIONAL;
  }
  else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other)
  {
    // direct call
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_DIRECT_CALL;
    get_profiler_ptr->call_map[arch_instr.ip]++;
  }
  else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other)
  {
    // indirect call
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_INDIRECT_CALL;
    get_profiler_ptr->call_map[arch_instr.ip]++;
  }
  else if (reads_sp && !reads_ip && writes_sp && writes_ip)
  {
    // return
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_RETURN;
  }
  else if (writes_ip)
  {
    // some other branch type that doesn't fit the above categories
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_OTHER;
  }
  else
  {
    assert(!arch_instr.is_branch);
    assert(arch_instr.branch_type == NOT_BRANCH);
    arch_instr.branch_taken = 0;
  }
#endif

  if (arch_instr.branch_taken != 1)
  {
    // clear the branch target for non-taken instructions
    arch_instr.branch_target = 0;
  }
  sim_stats.back().total_branch_types[arch_instr.branch_type]++;

  // Stack Pointer Folding
  // The exact, true value of the stack pointer for any given instruction can
  // usually be determined immediately after the instruction is decoded without
  // waiting for the stack pointer's dependency chain to be resolved.
  // We're doing it here because we already have writes_sp and reads_other
  // handy, and in ChampSim it doesn't matter where before execution you do it.
  if (writes_sp)
  {
    // Avoid creating register dependencies on the stack pointer for calls,
    // returns, pushes, and pops, but not for variable-sized changes in the
    // stack pointer position. reads_other indicates that the stack pointer is
    // being changed by a variable amount, which can't be determined before
    // execution.
    if ((arch_instr.is_branch != 0) || !(std::empty(arch_instr.destination_memory) && std::empty(arch_instr.source_memory)) || (!reads_other))
    {
      auto nonsp_end = std::remove(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
      arch_instr.destination_registers.erase(nonsp_end, std::end(arch_instr.destination_registers));
    }
  }

  // handle branch prediction for all instructions as at this point we do not know if the instruction is a branch
  sim_stats.back().total_branch_types[arch_instr.branch_type]++;

  bool alt_always_taken = false;

  auto btb_info_level_1 = impl_btb_prediction(arch_instr.ip, 1);

  arch_instr.alt_predicted_target = btb_info_level_1.first.first;
  alt_always_taken = btb_info_level_1.first.second;

  // cout << " Alt pred " << arch_instr.ip << endl;
  arch_instr.alt_prediction = alt_bp_predictor->GetPrediction(arch_instr.ip);

  arch_instr.alt_bp_prediction = arch_instr.alt_prediction;
  get_profiler_ptr->total_predictions++;

  if (!warmup && arch_instr.branch_type == BRANCH_CONDITIONAL)
  {
    arch_instr.yout = impl_get_yout(arch_instr.ip, 1);
    get_profiler_ptr->miss_histogram[arch_instr.yout].predictions++;
    if (arch_instr.branch_taken != arch_instr.alt_prediction)
    {
      get_profiler_ptr->miss_histogram[arch_instr.yout].misses++;
    }
    else
    {
      get_profiler_ptr->hit_histogram[arch_instr.yout]++;
    }
  }

  if (alt_always_taken)
  {
    arch_instr.alt_prediction = true;
  }

  if (arch_instr.alt_prediction == 0)
  {
    arch_instr.alt_predicted_target = 0;
  }

  if (arch_instr.branch_type == BRANCH_CONDITIONAL)
  {
    get_profiler_ptr->alt_bp_btb_stats.total_cond_pred++;
    if ((arch_instr.alt_prediction != arch_instr.branch_taken))
    {
      get_profiler_ptr->alt_bp_btb_stats.total_cond_miss++;
    }
  }

  auto btb_info_level_2 = impl_btb_prediction(arch_instr.ip, 2);

  auto [predicted_branch_target, always_taken] = btb_info_level_2.first; // base uses the L2 btb
  arch_instr.btb_prediction_src = btb_info_level_2.second;

  if (arch_instr.is_branch && predicted_branch_target == 0)
  {
    arch_instr.btb_miss = true;
  }

  auto bp_pred_level_2 = impl_predict_branch(arch_instr.ip, 2, arch_instr.branch_type, arch_instr.branch_taken, false);

  arch_instr.branch_prediction = bp_pred_level_2.first;
  arch_instr.bp_prediction_src = bp_pred_level_2.second;

  if (always_taken)
  {
    arch_instr.predicted_always_taken = true;
    arch_instr.branch_prediction = true;
  }

  if (arch_instr.branch_prediction == 0)
  {
    predicted_branch_target = 0;
  }

  if (arch_instr.branch_target != 0)
  {
    instrs_to_read_this_cycle = 0;
  }

  instr_in_btwn++;

  if (arch_instr.is_branch)
  {
    num_branch++;
    num_branch_this_cycle++;
    assert(num_branch_this_cycle <= MAX_NON_TAKEN_BRANCHES_PER_CYCLE);
    if (num_branch_this_cycle == MAX_NON_TAKEN_BRANCHES_PER_CYCLE)
    {
      instrs_to_read_this_cycle = 0;
    }

    // NOTE:
    // the taken  branch should stop and start fetching from the next cycle for ip based BTB design
    // past taken branches are fetch in the next cycle
    if (arch_instr.branch_taken)
    {
      instrs_to_read_this_cycle = 0;
    }

    // call code prefetcher every time the branch predictor is used
    static_cast<CACHE *>(L1I_bus.lower_level)->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);

    if ((arch_instr.branch_type == BRANCH_CONDITIONAL && arch_instr.branch_taken != arch_instr.branch_prediction))
    {
      get_profiler_ptr->stats_branch.bp_miss++;
      arch_instr.bp_miss = true;
      // if (!warmup)
      //   cout << "BP miss " << arch_instr.instr_id << endl;
    }

    if (arch_instr.alt_predicted_target != arch_instr.branch_target || (arch_instr.branch_type == BRANCH_CONDITIONAL && arch_instr.branch_taken != arch_instr.alt_prediction))
    {
      get_profiler_ptr->alt_bp_btb_stats.mispredictions++;
      get_profiler_ptr->alt_bp_btb_stats.main_path_misses[arch_instr.branch_type]++;
      arch_instr.alt_miss = true;
    }

    arch_instr.predicted_target = predicted_branch_target;

    // //ideal BTB
    // arch_instr.predicted_target = arch_instr.branch_target;

    if (arch_instr.predicted_target != arch_instr.branch_target || (arch_instr.branch_type == BRANCH_CONDITIONAL && arch_instr.branch_taken != arch_instr.branch_prediction))
    { // conditional branches are re-evaluated at decode when the target is computed
      sim_stats.back().total_rob_occupancy_at_branch_mispredict += std::size(ROB);
      sim_stats.back().branch_type_misses[arch_instr.branch_type]++;
      // get_profiler_ptr->bp_btb_stats.mispredictions++;
      // cout << "Main miss " << get_profiler_ptr->bp_btb_stats.mispredictions << endl;
      get_profiler_ptr->stats_branch.miss++;
      arch_instr.branch_miss = true;
      if (!warmup)
      {
        arch_instr.branch_mispredicted = 1;
        fetch_stall = 1;
        instrs_to_read_this_cycle = 0;
      }
    }
    else
    {
      // if correctly predicted taken, then we can't fetch anymore instructions this cycle
      if (arch_instr.branch_taken == 1)
      {
        instrs_to_read_this_cycle = 0;
      }
    }

    if (arch_instr.branch_mispredicted)
    {
      get_profiler_ptr->bp_btb_stats.misses[arch_instr.branch_type]++;
      if (arch_instr.branch_type == BRANCH_DIRECT_JUMP || arch_instr.branch_type == BRANCH_DIRECT_CALL)
        get_profiler_ptr->stats_branch_miss.target_direct++;
      else if ((arch_instr.branch_type == BRANCH_INDIRECT || arch_instr.branch_type == BRANCH_INDIRECT_CALL))
        get_profiler_ptr->stats_branch_miss.target_indirect++;
      else if (arch_instr.branch_type == BRANCH_RETURN)
      {
        get_profiler_ptr->stats_branch_miss.target_return++;
      }
      else if (arch_instr.branch_type == BRANCH_CONDITIONAL && (predicted_branch_target == arch_instr.branch_target))
        get_profiler_ptr->stats_branch_miss.condition_miss++;
    }

    if (arch_instr.is_branch)
    {
      get_profiler_ptr->branch_map[arch_instr.ip]++;
      get_profiler_ptr->stats_branch.total_branches++;
      if (arch_instr.branch_mispredicted)
      {
        // cout << "Miss found " << arch_instr.instr_id << endl;
        get_profiler_ptr->avg_instr_btwn_missbranch[instr_in_btwn]++;
        instr_in_btwn = 0;
      }
    }

    if (arch_instr.branch_type == BRANCH_INDIRECT || arch_instr.branch_type == BRANCH_RETURN)
    {
      bool found = false;
      for (auto &x : get_profiler_ptr->indirect_target_map[arch_instr.ip])
      {
        if (x.first == arch_instr.branch_target)
        {
          found = true;
          x.second++;
        }
      }
      if (!found)
      {
        get_profiler_ptr->indirect_target_map[arch_instr.ip].push_back(std::pair{arch_instr.branch_target, 1});
      }
    }

    if (arch_instr.branch_type == BRANCH_DIRECT_CALL || arch_instr.branch_type == BRANCH_INDIRECT_CALL)
    {
      get_profiler_ptr->total_calls++;
      if (arch_instr.branch_mispredicted)
      {
        get_profiler_ptr->total_call_miss++;
      }
    }

    // if the its a branch and is not always taken
    if (arch_instr.is_branch && !arch_instr.predicted_always_taken)
    {
#ifdef H2P_TAGE_NON_BOUNDARY
      if (arch_instr.bp_prediction_src.is_h2p)
      {
        arch_instr.hard_to_predict_branch = true;
      }
#ifdef H2P_TAGE_STYLE_CTR_BTB
      if (arch_instr.bp_prediction_src.is_h2p || (arch_instr.branch_prediction && (arch_instr.btb_prediction_src.is_h2p || arch_instr.predicted_target == 0)))
      {
        arch_instr.hard_to_predict_branch = true;
      }
#endif

#ifdef H2P_TAGE_STYLE_SEZNEC_BTB
      if (arch_instr.bp_prediction_src.is_h2p || (arch_instr.branch_prediction && (arch_instr.btb_prediction_src.is_h2p || arch_instr.predicted_target == 0)))
      {
        arch_instr.hard_to_predict_branch = true;
      }
#endif

#endif
#ifdef H2P_PRED_TABLE
      if (m_h2p_ptr->is_h2p_branch_table(arch_instr.ip, 1))
      {
        arch_instr.hard_to_predict_branch = true;
      }
#endif
#ifdef H2P_HP_50
      if (impl_get_yout(arch_instr.ip, 1) >= -50 && impl_get_yout(arch_instr.ip, 1) <= 50)
      {
        arch_instr.hard_to_predict_branch = true;
      }
#endif
#ifdef H2P_IDEAL
      if (arch_instr.branch_miss)
      {
        arch_instr.hard_to_predict_branch = true;
      }
#endif
    }

    if (arch_instr.branch_type == BRANCH_CONDITIONAL)
    {
      if (arch_instr.hard_to_predict_branch)
      {
        get_profiler_ptr->h2p_predictor_stats.total_marked_as_h2p++;
      }
      if ((arch_instr.branch_prediction != arch_instr.branch_taken))
      {
        get_profiler_ptr->h2p_predictor_stats.total_conditional_misses++;
        if (arch_instr.hard_to_predict_branch)
        {
          get_profiler_ptr->h2p_predictor_stats.total_h2p_marked_correctly++;
        }
      }
    }

    get_profiler_ptr->stats_branch.btb_predictions++;
    if (arch_instr.branch_taken && arch_instr.predicted_target != arch_instr.branch_target)
    {
      get_profiler_ptr->stats_branch.btb_miss++;
      // if (arch_instr.branch_target > 0 && !warmup)
      //   cout << "BTB miss found! " << arch_instr.ip << " " << arch_instr.predicted_target << endl;
      if (arch_instr.hard_to_predict_branch && arch_instr.branch_type == BRANCH_CONDITIONAL)
      {
        get_profiler_ptr->stats_branch.h2p_btb_misses++;
      }
    }

#ifdef REAL_BRANCH_RECOVERY
    // CHANGE: Logging h2p prefetcher info only for real branch recovery mode
    // if (arch_instr.hard_to_predict_branch)
    //   printf("arch_instr.hard_to_predict_branch: %d  arch_instr.branch_type: %d\n",
    //          arch_instr.hard_to_predict_branch,
    //          arch_instr.branch_type);

    if (arch_instr.hard_to_predict_branch && arch_instr.branch_type == BRANCH_CONDITIONAL)
    {
      // cout << "inside h2p prefetching" << endl;
      alt_path_has_indirect = false;

      get_profiler_ptr->hpca.total_h2p_tried++;
      if (arch_instr.branch_prediction != arch_instr.branch_taken)
      {
        get_profiler_ptr->hpca.total_h2p_miss++;
      }

      uint64_t taken_bb_size = 0;
      uint64_t not_taken_bb_size = 0;
      taken_bb_size = impl_btb_bb_size(arch_instr.ip, 2).first;
      not_taken_bb_size = h2p_not_taken_bb_size[arch_instr.ip].first;
      std::pair<uint64_t, uint64_t> spec_path_info = {0, 0};
      spec_main_addrs = arch_instr.ip;
      impl_speculative_begin(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type, begin_branch_recovery);
      alt_bp_predictor->speculative_begin(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
      if (arch_instr.branch_prediction)
      { // taken so prefetch not-taken path
        // cout << "Not taken " << arch_instr.instr_id << endl;
        spec_path_info = prefetch_alternate_path(arch_instr.ip, arch_instr.ip + 4, not_taken_bb_size, arch_instr.branch_type, arch_instr.instr_id,
                                                 arch_instr.branch_taken != arch_instr.branch_prediction, arch_instr); // prefetch not-taken
      }
      else
      {
        spec_path_info =
            prefetch_alternate_path(arch_instr.ip, impl_btb_prediction(arch_instr.ip, 2).first.first, taken_bb_size, arch_instr.branch_type,
                                    arch_instr.instr_id, arch_instr.branch_taken != arch_instr.branch_prediction, arch_instr); // prefetch not-taken
      }
      impl_speculative_end();
      alt_bp_predictor->speculative_end();
      if (alt_path_has_indirect)
      {
        get_profiler_ptr->alt_path_with_ind_br++;
      }
      last_main_ip = arch_instr.ip;

      // save only for the real h2p
      if (arch_instr.branch_taken != arch_instr.branch_prediction)
      {
        if (spec_path_info.first > 0)
        {
          // cout << "Total branches " << spec_path_info.first << endl;
          get_profiler_ptr->total_saved_bb_size += spec_path_info.first;
          get_profiler_ptr->total_pref_paths++;

          if (spec_path_info.first < NUM_BRANCHES_SAVED)
          {
            get_profiler_ptr->total_path_stalls++;
          }

          if (has_h2p)
          {
            get_profiler_ptr->path_has_h2p++;
            get_profiler_ptr->total_bb_distance_from_h2p += h2p_bb_distance;
            get_profiler_ptr->average_bb_when_h2p += spec_path_info.first;
          }
        }
        else
        {
          get_profiler_ptr->hpca.unable_to_start_pref++;
        }
      }
      else
      {
        get_profiler_ptr->total_saved_bb_size_wh2p += spec_path_info.first;
        get_profiler_ptr->total_pref_paths_wh2p++;
      }
    }
#endif

    if (last_br_type == BRANCH_RETURN)
    {
      h2p_tt_map[last_target] = last_bb_size;
    }
    if (!last_br_taken)
    { // not taken bb_size does not cost a btb entry
      h2p_not_taken_bb_size[last_ip].first = last_bb_size;
      h2p_not_taken_bb_size[last_ip].second = last_br_type;
    }

    impl_update_bb_size(last_ip, 1, last_bb_size, last_br_taken, last_br_type, 0);
    impl_update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type, begin_branch_recovery);
    impl_last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);

    if (arch_instr.branch_type == BRANCH_CONDITIONAL)
    {
      alt_bp_predictor->UpdatePredictor(arch_instr.ip, arch_instr.branch_type, arch_instr.branch_taken, arch_instr.branch_target, arch_instr.bp_miss);
    }
    else
    {
      alt_bp_predictor->TrackOtherInst(arch_instr.ip, arch_instr.branch_type, arch_instr.branch_taken, arch_instr.branch_target);
    }

    last_bb_size = 0;
    last_ip = arch_instr.ip;
    last_br_taken = arch_instr.branch_taken;
    last_br_type = arch_instr.branch_type;
    last_target = arch_instr.branch_target;
    // cout<<"Instruction was branch"<<endl;
  }

  // fast warmup eliminates register dependencies between instructions
  // branch predictor, cache contents, and prefetchers are still warmed up
  if (warmup)
  {
    arch_instr.source_registers.clear();
    arch_instr.destination_registers.clear();
  }

  if (mark_next_instr)
  {
    arch_instr.instr_after_branch_mispred = true;
    mark_next_instr = false;
  }

  if (begin_branch_recovery)
  {
    if (arch_instr.is_branch)
    { //&& arch_instr.branch_taken) { //counting all branches
      if (total_branches_saved > NUM_BRANCHES_SAVED)
      {
        last_recovered_ip = 0;
        uop_cache_windows_recovered = 0;
        begin_branch_recovery = false;
        total_branches_saved = 0;
        ideal_br_in_process = false;
      }
      else
      {
        total_branches_saved++;
      }
    }
  }

  if (begin_branch_recovery)
  {
    arch_instr.track_for_stats = true;
  }

#ifdef IDEAL_BRANCH_RECOVERY
  if (begin_branch_recovery)
  {
    total_critical_instruction++;
    arch_instr.is_critical = true;
  }
#endif

#ifdef L1IPREF
  if (begin_branch_recovery)
  {
    total_critical_instruction++;
    arch_instr.is_critical = true;
    h2p_map[arch_instr.ip] = true;
  }
#endif

  // #ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND
  if (begin_branch_recovery)
  {
    if (last_instr.ip >> LOG2_BLOCK_SIZE != arch_instr.ip >> LOG2_BLOCK_SIZE)
      get_profiler_ptr->number_of_lines_by_ideal++;
    // assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_CONDITIONAL)
    {
      total_critical_instruction++;
      // cout << "Critial marked " << arch_instr.ip << endl;
      arch_instr.is_critical = true;
    }
  }
  // #endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND_IND
  if (begin_branch_recovery)
  {
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_CONDITIONAL || (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL))
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN
  if (begin_branch_recovery)
  {
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_CONDITIONAL || (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL) || last_br_miss_type == BRANCH_RETURN)
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN_DIRECT
  if (begin_branch_recovery)
  {
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_CONDITIONAL || (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL) || last_br_miss_type == BRANCH_RETURN || (last_br_miss_type == BRANCH_DIRECT_CALL || last_br_miss_type == BRANCH_DIRECT_JUMP))
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_DIRECT
  if (begin_branch_recovery)
  {
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_CONDITIONAL || (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL) || (last_br_miss_type == BRANCH_DIRECT_CALL || last_br_miss_type == BRANCH_DIRECT_JUMP))
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_IND
  if (begin_branch_recovery)
  {
    // cout << "last_br_type " << int(last_br_miss_type) << endl;
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_INDIRECT || last_br_miss_type == BRANCH_INDIRECT_CALL)
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_DIRECT
  if (begin_branch_recovery)
  {
    // cout << "last_br_type " << int(last_br_miss_type) << endl;
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_DIRECT_CALL || last_br_miss_type == BRANCH_DIRECT_JUMP)
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

#ifdef IDEAL_BRANCH_RECOVERY_ONLY_RETURN
  if (begin_branch_recovery)
  {
    // cout << "last_br_type " << int(last_br_miss_type) << endl;
    assert(last_br_miss_type != 0);
    if (last_br_miss_type == BRANCH_RETURN)
    {
      total_critical_instruction++;
      arch_instr.is_critical = true;
    }
  }
#endif

  if (begin_branch_recovery)
  {
    arch_instr.after_br_miss = true; // for coverage calculations
  }

  arch_instr.cycle_added_in_ftq = current_cycle;
  arch_instr.br_resolved = false;
  arch_instr.event_cycle = current_cycle;
  arch_instr.ftq_cycle = current_cycle;
  arch_instr.tag_checked = false;
  get_profiler_ptr->speculative_miss.branch_occurance[arch_instr.branch_type]++;

  // only for missed targets
#ifdef WITH_MRC

  // MRC lookup
  bool mrc_hit = false;

  if (arch_instr.is_branch && (arch_instr.branch_miss))
  {
    get_profiler_ptr->mrc_target_check++;

    if (arch_instr.branch_taken)
      from_mrc = m_mrc_ptr->Lookup(arch_instr.branch_target, current_cycle);
    else
      from_mrc = m_mrc_ptr->Lookup(arch_instr.ip + 4, current_cycle);

    // save the length
    if (ips_checked_mrc > 0)
    {
      get_profiler_ptr->mrc_found++;
      get_profiler_ptr->mrc_hit_length += ips_checked_mrc;
    }

    ips_checked_mrc = 0;
    if (from_mrc.size() > 0)
    {
      get_profiler_ptr->mrc_target_hit++;
    }
  }

  if (!from_mrc.empty() && ips_checked_mrc <= NUM_OF_INSTR_PER_ENTRY)
  {
    get_profiler_ptr->mrc_checks++;
    ips_checked_mrc++;
    for (auto &mcr_entry : from_mrc)
    {
      if (mcr_entry == arch_instr.ip)
      {
        mrc_hit = true;
        get_profiler_ptr->mrc_hits++;

        arch_instr.fetched = COMPLETED;
        arch_instr.decoded = COMPLETED;
        arch_instr.event_cycle = current_cycle;
        arch_instr.dib_checked = COMPLETED;
        arch_instr.mrc_hit = true;
        break;
      }
    }
    // cout << "MRC Check IP " << arch_instr.ip << " mrc_hit " << mrc_hit << " ips_checked_mrc " << ips_checked_mrc << " @" << current_cycle << endl;
  }

  // MRC training
  if (arch_instr.is_branch && (arch_instr.branch_miss))
  {
    // if taken then save the target
    if (arch_instr.branch_taken)
    {
      m_mrc_ptr->AddEntry(arch_instr.branch_target);
      last_mispredicted_target = arch_instr.branch_target;
    }
    else
    {
      m_mrc_ptr->AddEntry(arch_instr.ip + 4);
      last_mispredicted_target = arch_instr.ip + 4;
    }
    instr_added_to_mcr = 0;
  }
  else
  {
    if (instr_added_to_mcr <= NUM_OF_INSTR_PER_ENTRY)
    {
      m_mrc_ptr->AddInstruction(arch_instr.ip, last_mispredicted_target, current_cycle);
    }
    instr_added_to_mcr++;
  }
#endif

  if (arch_instr.is_branch && arch_instr.branch_type == BRANCH_CONDITIONAL)
  {
    get_profiler_ptr->conditional_demand_path++;
  }

  IFETCH_BUFFER.push_back(arch_instr);
  num_ifetch++;

  last_ip_in_ftq = arch_instr.ip;
  instr_unique_id++;

  if (!arch_instr.two_byte_instr)
  {
    last_bb_size++;
    instr_unique_id_4b++;
  }
}
std::pair<uint64_t, uint8_t> O3_CPU::prefetch_alternate_path(uint64_t starting_ip, uint64_t target, uint64_t bb_size, uint8_t branch_type, uint64_t by_instr,
                                                             bool branch_miss, ooo_model_instr &issuing_instr)
{
  // cout << __func__ << endl;
  // cout << "Inside prefetch_alternate_path" << endl;
  if (issuing_instr.branch_type == BRANCH_CONDITIONAL)
  {
    alt_bp_predictor->UpdatePredictor(issuing_instr.ip, issuing_instr.branch_type, !issuing_instr.branch_prediction, issuing_instr.branch_target, false);
  }
  else
  {
    alt_bp_predictor->TrackOtherInst(issuing_instr.ip, issuing_instr.branch_type, !issuing_instr.branch_prediction, issuing_instr.branch_target);
  }

  stats_num_br = 0;
  begin_check = false;

  alternate_instr_id = issuing_instr.instr_id_4b + 1; // we start prefetching from next

  bool reached_h2p = false;
  int8_t h2p_level = 0;

  has_h2p = false;
  h2p_bb_distance = 0;

  // bool alt_is_correct = issuing_instr.branch_prediction != issuing_instr.branch_taken;

  // save stats for wrong H2P
  // bool alt_is_correct = issuing_instr.branch_prediction != issuing_instr.branch_taken;

  bool alt_is_correct = true;

  if (target == 0)
  {
    if (issuing_instr.branch_type == BRANCH_INDIRECT_CALL || issuing_instr.branch_type == BRANCH_INDIRECT)
    {
      {
        if (alt_is_correct)
          get_profiler_ptr->alt_path_stop_info.no_target_ind++;
      }
    }
    else if (alt_is_correct)
      get_profiler_ptr->alt_path_stop_info.no_target_other++;
    return std::pair{0, 0};
  }

  // alt_branch.clear();
  begin_check = true;

  last_pref_id = issuing_instr.instr_id_4b;

  stop_previous_alternate_path(issuing_instr);

  demand_ips.clear();
  last_alt_pref_by = by_instr;

  uint8_t ips_checked = 0;
  uint64_t m_ip = target;
  int num_br = 0;
  int num_h2p_br = 0;
  bool alt_started_correctly = false;
  int max_cycle_ip = 0;
  uint64_t max_num_of_br = 0;

  int end_counter = 0;

  // if the alternate path started correctly
  if (issuing_instr.branch_prediction != issuing_instr.branch_taken && target == issuing_instr.branch_target)
  {
    alt_started_correctly = true;
  }

  // now try to get the path and add it
  while (1)
  {
    auto btb_info = impl_btb_prediction(m_ip, 1);
    auto m_target = btb_info.first.first;
    auto always_taken = btb_info.first.second;
    auto br_type = impl_get_branch_type(m_ip);
    auto m_prediction = alt_bp_predictor->GetPrediction(m_ip);
    auto h2p_ctr_val = alt_bp_predictor->is_h2p(m_ip);
    if (always_taken)
    {
      m_prediction = true;
    }

#define CP_H2P_PATH

#ifdef CP_H2P_PATH
    if (br_type == BRANCH_CONDITIONAL)
    {
      end_counter += h2p_ctr_val;
    }
    else if (br_type == BRANCH_RETURN)
    {
      end_counter += 1;
    }
    else if (br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL)
    {
      end_counter += 1;
    }
    else
    {
      end_counter += 1; // here we increment for not branch as well? Maybe add a condition to increment only at branches (so if br_type > 0 and <= 6)
    }
#endif

#ifdef NO_ALT_INDIRECT_PREDICTOR
    // no alternate indirect
    if ((br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL))
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.ind_branch++;
      return std::pair{num_br, m_ip};
    }
#endif

    // save branch information for stats only for misses
    // if ((br_type == BRANCH_CONDITIONAL || br_type == BRANCH_DIRECT_CALL || br_type == BRANCH_DIRECT_JUMP || br_type == BRANCH_INDIRECT
    //      || br_type == BRANCH_INDIRECT_CALL || br_type == BRANCH_OTHER || br_type == BRANCH_RETURN)
    //     && (issuing_instr.branch_taken != issuing_instr.branch_prediction)) {
    //   alt_branch[m_ip ^ alternate_instr_id].br_ip = m_ip;
    //   alt_branch[m_ip ^ alternate_instr_id].prediction = m_prediction;
    //   alt_branch[m_ip ^ alternate_instr_id].checked = false;
    //   alt_branch[m_ip ^ alternate_instr_id].br_type = br_type;
    //   alt_branch[m_ip ^ alternate_instr_id].instr_id = alternate_instr_id;
    //   alt_branch[m_ip ^ alternate_instr_id].by_id = issuing_instr.instr_id_4b;
    //   alt_branch[m_ip ^ alternate_instr_id].valid = true;
    //   alt_branch[m_ip ^ alternate_instr_id].started_correctly = alt_is_correct;
    //   if (m_prediction)
    //     alt_branch[m_ip ^ alternate_instr_id].predicted_branch_target = m_target;
    //   else
    //     alt_branch[m_ip ^ alternate_instr_id].predicted_branch_target = 0;
    // }

    ips_checked++;
    max_cycle_ip++;
    alternate_instr_id++;

    bool is_br = false;
    if (br_type == BRANCH_CONDITIONAL || br_type == BRANCH_DIRECT_CALL || br_type == BRANCH_DIRECT_JUMP || br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL || br_type == BRANCH_OTHER || br_type == BRANCH_RETURN)
    {
      is_br = true;
      max_num_of_br++;
    }

    if (br_type == BRANCH_CONDITIONAL)
    {
      get_profiler_ptr->conditional_alt_path++;
    }

    // cout << "Calling add_to_uop_queue_ip: " << current_cycle << endl;
    add_to_uop_queue_ip(m_ip, by_instr, reached_h2p, branch_miss, false, starting_ip, issuing_instr.branch_taken, is_br, m_prediction, br_type);
    // cout << "Called add_to_uop_queue_ip: " << current_cycle << endl;
    // if predicted branch then update the histories
    if (br_type == BRANCH_CONDITIONAL || br_type == BRANCH_DIRECT_CALL || br_type == BRANCH_DIRECT_JUMP || br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL || br_type == BRANCH_OTHER || br_type == BRANCH_RETURN)
    {
      impl_update_btb(m_ip, m_target, m_prediction, br_type, true);
      if (br_type == BRANCH_CONDITIONAL)
      {
        alt_bp_predictor->UpdatePredictor(m_ip, br_type, m_prediction, m_target, false);
      }
      else
      {
        alt_bp_predictor->TrackOtherInst(m_ip, br_type, m_prediction, m_target);
      }
      ips_checked = 0;
    }

    if (ips_checked >= MAX_IP_CHECK)
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.max_ip++;
      get_profiler_ptr->alt_stop_by_max_ip++;
      return std::pair{num_br, m_ip};
    }

#ifdef STOP_AT_MAX_BR
    if (max_num_of_br >= MAX_BRANCHES)
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.max_br_allowed++;
      return std::pair{num_br, m_ip};
    }
#endif

    // #ifdef STOP_AT_MAX_CYCLE
    //     if (max_cycle_ip >= MAX_CYCLE_IP) {
    //       if (alt_is_correct)
    //         get_profiler_ptr->alt_path_stop_info.max_ip_limit++;
    //       get_profiler_ptr->alt_stop_by_limit_ip++;
    //       return std::pair{num_br, m_ip};
    //     }
    // #endif
    if ((br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL))
    {
      get_profiler_ptr->total_ind_in_alt_path++;
      alt_path_has_indirect = true;
    }

    // if branch is taken but we dont have target dont prefetch beyond this point
    if (br_type > 0 && m_prediction && m_target == 0)
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.btb_miss++;
      get_profiler_ptr->alt_stop_by_btb_miss++;
      return std::pair{num_br, m_ip};
    }

    // set the next ip to target or +4
    if (br_type > 0 && m_prediction && m_target > 0)
    { // predicted taken branch
      m_ip = m_target;
    }
    else
    {
      m_ip += 4;
    }

    // count number of branches
    if (br_type == BRANCH_DIRECT_CALL || br_type == BRANCH_DIRECT_JUMP || br_type == BRANCH_INDIRECT || br_type == BRANCH_INDIRECT_CALL || br_type == BRANCH_OTHER || br_type == BRANCH_RETURN)
    {
      num_br++;
    }

#ifdef CP_H2P_PATH
    if (end_counter >= H2P_T)
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.stop_at_sat_counter++;
      get_profiler_ptr->alt_stop_by_sat_counter++;
      return std::pair{num_br, m_ip};
    }
#else
    if (num_br >= NUM_BRANCHES_SAVED)
    {
      if (alt_is_correct)
        get_profiler_ptr->alt_path_stop_info.max_br++;
      return std::pair{num_br, m_ip};
    }
#endif
  }
}

void O3_CPU::decode_prefetched_instruction()
{
  // if this is set the only pref till L1I and do not add it in the micro op cache
#ifdef PREF_ONLY_TILL_L1I
  return;
#endif

#ifdef L1IPREF
  while (!static_cast<CACHE *>(L1I_bus.lower_level)->uop_pref_addrs.empty())
  {
    if (h2p_map[static_cast<CACHE *>(L1I_bus.lower_level)->uop_pref_addrs.front()])
    {
      m_microop_cache_ptr->Insert(static_cast<CACHE *>(L1I_bus.lower_level)->uop_pref_addrs.front(), current_cycle, true, false, true, false, false, true); // h2p path, these are instructions marked as critical for recovery after branch mispredictions hence is_alternate_path = true
    }
    static_cast<CACHE *>(L1I_bus.lower_level)->uop_pref_addrs.pop_front();
  }
#endif

  int available_decode_bandwidth = 0;
#ifdef DECODE_SHARED
  if ((fetch_stall == 1 && allow_pref_to_use_decoders_wp) || (fetch_stall == 0 && allow_pref_to_use_decoders_cp))
  {
    available_decode_bandwidth = DECODE_WIDTH;
  }
  // cout << __func__ << " available_decode_bandwidth " << int(available_decode_bandwidth) << " @" << current_cycle << endl;
#else
  available_decode_bandwidth = DECODE_WIDTH;
#endif

  uint64_t start_address = PRF_DECODE_BUFFER.front().ip;

  while (available_decode_bandwidth > 0 && !PRF_DECODE_BUFFER.empty())
  {

    get_profiler_ptr->m_decode_stats.prefetch_decode++;

    // try to find the if this prefetched ip was branch and if it was taken to place them correctly in the micro op cache
    bool is_br = false;

    if (PRF_DECODE_BUFFER.front().br_type == BRANCH_CONDITIONAL || PRF_DECODE_BUFFER.front().br_type == BRANCH_DIRECT_CALL || PRF_DECODE_BUFFER.front().br_type == BRANCH_DIRECT_JUMP || PRF_DECODE_BUFFER.front().br_type == BRANCH_INDIRECT || PRF_DECODE_BUFFER.front().br_type == BRANCH_INDIRECT_CALL || PRF_DECODE_BUFFER.front().br_type == BRANCH_OTHER || PRF_DECODE_BUFFER.front().br_type == BRANCH_RETURN)
    {
      is_br = true;
    }

    // ADDED: Get alternate path flag from decode entry
    bool is_alternate = PRF_DECODE_BUFFER.front().is_alternate_path;
    // once branch related info is found out add them to the micro op cache
    m_microop_cache_ptr->Insert(PRF_DECODE_BUFFER.front().ip, current_cycle, true, PRF_DECODE_BUFFER.front().taken, true,
                                PRF_DECODE_BUFFER.front().was_from_branch_miss, is_br, is_alternate);
    last_pref_decoded = PRF_DECODE_BUFFER.front().ip;
    PRF_DECODE_BUFFER.pop_front();
    get_profiler_ptr->front_end_energy.pref_decoded++;
    available_decode_bandwidth--;
  }

  while (!static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.empty() && (std::size(PRF_DECODE_BUFFER) < 512))
  {
    prefetch_decode_entry pd_entry;
    pd_entry.ip = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().ip;
    pd_entry.was_from_branch_miss = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().was_from_branch_miss;
    pd_entry.by_instr = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().by_instr;
    pd_entry.br_type = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().br_type;
    pd_entry.taken = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().taken;
    // ADDED: Copy alternate path flag
    pd_entry.is_alternate_path = static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.front().is_alternate_path;
    PRF_DECODE_BUFFER.push_back(pd_entry);
    static_cast<CACHE *>(L1I_bus.lower_level)->cache_pref_decode_buffer.pop_front();
  }
}

void O3_CPU::add_to_uop_queue_ip(uint64_t ip_to_pref, uint64_t by_instr, bool reached_h2p, bool branch_miss, bool nested_pref, uint64_t by_ip, bool was_taken,
                                 bool is_br, bool br_taken, uint8_t pref_br_type)
{

#ifdef NO_PREFETCH_ALL_HIT
  ideal_ips_to_prefetch[ip_to_pref] = true;
#endif

#ifdef UOP_PREFETCH_FILL
  m_microop_cache_ptr->Insert(ip_to_pref, current_cycle, false, false, false, false, false, true); // as this is called from prefetch_alternate_path() only
#endif

  bool taken_br = was_taken;

#ifdef PREFETCH_PATHS
  // cout << "Prefetching IP " << std::hex << ip_to_pref << std::dec << " bb_size " << m_bb_size << " @" << current_cycle << endl;
  // if the PQ is full cant take more requests
  // if (!m_microop_cache_ptr->Lookup(ip_to_pref, current_cycle)) {
  if (true)
  {
    // assert(by_instr > 0);

    ip_pref_entry ip_to_add;
    ip_to_add.ip = ip_to_pref;
    ip_to_add.by_isntr = by_instr;
    ip_to_add.by_ip = by_ip;
    ip_to_add.nested_pref = nested_pref;
    ip_to_add.by_br_miss = branch_miss;
    ip_to_add.btb_check_done = false;
    ip_to_add.bp_check_done = false;
    ip_to_add.ready_to_prefetch = false;
    ip_to_add.was_taken = taken_br;
    ip_to_add.is_branch = is_br;
    ip_to_add.is_taken = br_taken;
    ip_to_add.br_type = pref_br_type;

    // ADDED THIS: Mark as alternate path
    // as this is called from prefetch_alternate_path() only
    ip_to_add.is_alternate_path = true; // KEY LINE!
    // cout << "Marked alternate path for IP ";
    taken_br = false; // mark only the branch
    ips_to_prefetch.push_back(ip_to_add);
    last_pref_ip = ip_to_pref;
  }
#endif
}

void O3_CPU::stop_previous_alternate_path(ooo_model_instr &instr)
{
  // clear older waiting prefs
  ips_to_prefetch.clear();
}

void O3_CPU::operate_on_uop_mshr_queue()
{
  while (!static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.empty() && static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().pref_done && static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().branch_resolved)
  {
    // if (static_cast<CACHE*>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().by_instr == 15836144) cout << "Removed from MSHR " << current_cycle << endl;
    static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.pop_front();
  }

  // auto to_read = L1I_BANDWIDTH; //this will prefetch 2 lines
  auto to_read = 1; // prefetching only 1 line, to maintain the contention in L1I

  while (to_read > 0 && !static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.empty())
  {
    for (auto &mshr_entry : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
    {
      if (!mshr_entry.added_to_pq)
      {
        if (static_cast<CACHE *>(L1I_bus.lower_level)->prefetch_line_pkt(mshr_entry.pq_pkt))
        {
          mshr_entry.added_to_pq = true;
          get_profiler_ptr->num_of_lines_pref[mshr_entry.by_instr]++;
          if (mshr_entry.pq_pkt.by_br_miss)
          {
            get_profiler_ptr->lines_pref_stats.line_pref_by_correct_alt++;
          }
          else
          {
            get_profiler_ptr->lines_pref_stats.line_pref_by_wrong_alt++;
          }
        }
      }
    }
    to_read--;
  }

  // check for deadlock
  if (!std::empty(static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR) && (static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().event_cycle + UOP_MSHR_DEADLOCK_CYCLE) <= current_cycle)
  {
    cout << "Deadlock due to UOP_CACHE_MSHR front() " << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().by_instr << endl;
    // if (UOP_CACHE_MSHR.front().by_instr == 66156599) {
    cout << "line is " << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().cache_line_addr << " ip "
         << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().ip << std::dec << " pref_done? "
         << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().pref_done << " br? "
         << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().branch_resolved << " by_instr "
         << static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR.front().by_instr << " @" << current_cycle << endl;
    // }
    throw champsim::deadlock{cpu};
  }
}

void O3_CPU::check_dib()
{
  // cout << __func__ << endl;

  // check how many instructions are hit so

  uint64_t last_ip = 0;
  bool hit = true;
  uint8_t dib_ports = 2;

  while (dib_ports > 0)
  {

    // each port will try to do the dib check for one window
    auto dib_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [](const ooo_model_instr &x)
                                  { return !x.dib_checked; });
    auto dib_no_match_ip = [find_ip = dib_begin->ip](const ooo_model_instr &x)
    {
      return ((find_ip >> lg2(SIZE_WINDOWS)) != (x.ip >> lg2(SIZE_WINDOWS)));
    };

    auto dib_end = std::find_if(dib_begin, std::end(IFETCH_BUFFER), dib_no_match_ip);
    for (auto it = dib_begin; it != dib_end; ++it)
    {
      do_check_dib(*it);
      get_profiler_ptr->detailed_energy.dib_check++;
      if (it->uop_cache_hit)
      {
        get_profiler_ptr->detailed_energy.dib_hits++;
      }
      last_ip = it->ip;
    }
    dib_ports--;
  }

  // cout << "Mode is " << fetch_mode << endl;
}

void O3_CPU::do_check_dib(ooo_model_instr &instr)
{
  instr.dib_checked = COMPLETED;
  get_profiler_ptr->uop_cache_read++;
  bool hit = false;

#if defined(IDEAL_UOP_CACHE)
  hit = true;
#elif defined(UOP_PREF_L1I_HIT)
  bool hit_from_l1i = false;
  bool hit_from_uop_cache = false;
  if (static_cast<CACHE *>(L1I_bus.lower_level)->is_hit(instr.ip).first)
  {
    hit = true;
    hit_from_l1i = true;
  }
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
  {
    hit = true;
    hit_from_uop_cache = true;
  }

  // if the hit is transformed from L1I call the L1I prefether
  if (hit_from_l1i && !hit_from_uop_cache)
  {
    static_cast<CACHE *>(L1I_bus.lower_level)->check_tag(instr.ip, instr.instr_id, hit);
  }
#elif defined(BRANCH_RECOVERY_HITS)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(BASELINE)
  hit = m_microop_cache_ptr->Lookup(instr.ip, current_cycle);
#elif defined(L1IPREF)
  hit = m_microop_cache_ptr->Lookup(instr.ip, current_cycle);
#elif defined(BRANCH_UOP_PREF)
  hit = m_microop_cache_ptr->Lookup(instr.ip, current_cycle);
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_COND)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_IND)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_COND_IND)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN_DIRECT)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_DIRECT)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_DIRECT)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY_ONLY_RETURN)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(IDEAL_BRANCH_RECOVERY)
  if (instr.is_critical)
    hit = true;
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#elif defined(REAL_BRANCH_RECOVERY)
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
#if defined(NO_PREFETCH_ALL_HIT)
  if (m_microop_cache_ptr->Lookup(instr.ip, current_cycle))
    hit = true;
  if (ideal_ips_to_prefetch[instr.ip])
  {
    hit = true;
  }
  ideal_ips_to_prefetch[instr.ip] = false;
#endif
#endif

#ifdef NO_UOP_CACHE
  hit = false;
#endif

  if (track_till_first_miss)
  {
    if (hit)
    {
      get_profiler_ptr->instr_till_first_uop_miss++;
    }
    else
    {
      track_till_first_miss = false;
    }
  }

  if (instr.track_for_stats)
  {
    get_profiler_ptr->total_instr_in_window++;
    if (!hit)
      get_profiler_ptr->window_instr_miss++;
  }

  if (hit)
  {
    get_profiler_ptr->uop_cache_hit++;
    instr.fetched = COMPLETED;
    instr.decoded = COMPLETED;
    instr.event_cycle = current_cycle;
    instr.dib_checked = COMPLETED;
    instr.uop_cache_hit = true;
    get_profiler_ptr->front_end_energy.fetched_from_uop_cache++;

    if (instr.mrc_hit)
    {
      get_profiler_ptr->mrc_in_uop_cache++;
    }
  }

  if (hit)
  {
    fetch_mode = STREAM;
  }
  else if (fetch_mode == STREAM)
  {
    instr.uop_cache_miss_panelty = true;
    fetch_mode = BUILD;
  }

  instr.from_prefetch = m_microop_cache_ptr->isPrefetched(instr.ip);

  if (hit && instr.from_prefetch)
  {
    get_profiler_ptr->uop_pref_stats.hits_from_pref++;
  }

  bool recently_prefetched = false;
  bool in_mshr = false;
  bool pref_done = false;

#if defined(REAL_BRANCH_RECOVERY)
  for (auto &mshr : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
  {
    if (mshr.ip >> lg2(SIZE_WINDOWS) == instr.ip >> lg2(SIZE_WINDOWS))
    {
      in_mshr = true;
      if (mshr.pref_done)
      {
        pref_done = true;
      }
    }
  }
  for (auto &recent_entry : recently_prefetched_ips)
  {
    if (instr.ip == recent_entry)
    {
      recently_prefetched = true;
    }
  }
#endif

  if (instr.branch_miss)
  {
    get_profiler_ptr->pipeline_resteer++;
    track_till_first_miss = true;
  }

  // cout << "IP " << instr.ip << " hit " << hit << endl;

  m_microop_cache_ptr->UpdateStats(instr.ip, hit, in_mshr, pref_done, recently_prefetched, current_cycle, instr.after_br_miss);
}

// #define PARALLEL_DIB_CHECK

void O3_CPU::fetch_instruction()
{

  // do_tag_check();
  // Fetch a single cache line
#ifdef PARALLEL_DIB_CHECK
  auto fetch_ready = [](const ooo_model_instr &x)
  {
    return !x.fetched;
  };
#else
  auto fetch_ready = [](const ooo_model_instr &x)
  {
    return x.dib_checked == COMPLETED && !x.fetched;
  };
#endif

  auto to_read = L1I_BANDWIDTH;

  auto l1i_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), fetch_ready);
  while (to_read > 0 && l1i_req_begin != std::end(IFETCH_BUFFER))
  {
    // Find the chunk of instructions in the block
    auto no_match_ip = [find_ip = l1i_req_begin->ip](const ooo_model_instr &x)
    {
      return (find_ip >> LOG2_BLOCK_SIZE) != (x.ip >> LOG2_BLOCK_SIZE);
    };
    auto l1i_req_end = std::find_if(l1i_req_begin, std::end(IFETCH_BUFFER), no_match_ip);

    for (auto it = l1i_req_begin; it != l1i_req_end; ++it)
    {
      if (it->uop_cache_miss_panelty)
      {
        it->uop_cache_miss_panelty = false;
        get_profiler_ptr->cpu_stalls.switch_stalls++;
        if constexpr (champsim::uop_debug_print)
        {
          std::cout << "[SWITCH_PANELTY] instr_id: " << it->instr_id << " ip: " << std::hex << it->ip << std::dec << " @" << current_cycle << std::endl;
        }
        return;
      }
    }

    // Issue to L1I
    auto success = do_fetch_instruction(l1i_req_begin, l1i_req_end);
    if (success)
    {
      right_path_size += l1i_req_end - l1i_req_begin;
      right_path_accesses++;
      for (auto it = l1i_req_begin; it != l1i_req_end; ++it)
      {
        it->fetched = INFLIGHT;
        it->l1i_inflight_time = current_cycle;
        get_profiler_ptr->front_end_energy.fetched_from_l1i++;
        if (it->instr_id == 63158260)
        {
          cout << "Fetched in progress! " << std::hex << (it->ip >> LOG2_BLOCK_SIZE) << " inflight " << int(it->fetched) << std::dec << " @" << current_cycle
               << endl;
        }
      }
      break;
    }
    --to_read;
    l1i_req_begin = std::find_if(l1i_req_end, std::end(IFETCH_BUFFER), fetch_ready);
  }
}

void O3_CPU::do_tag_check()
{
  assert(false);
  auto tag_ready = [](const ooo_model_instr &x)
  {
    return !x.tag_checked;
  };

  if (IFETCH_BUFFER.empty())
  {
    return;
  }

  auto tag_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), tag_ready);
  auto no_match_ip = [find_ip = tag_req_begin->ip](const ooo_model_instr &x)
  {
    return (find_ip >> LOG2_BLOCK_SIZE) != (x.ip >> LOG2_BLOCK_SIZE);
  };

  auto tag_req_end = std::find_if(tag_req_begin, std::end(IFETCH_BUFFER), no_match_ip);
  if (tag_req_begin == tag_req_end)
    return;

  bool all_ip_hit_uop_cache = true;
  for (auto it = tag_req_begin; it != tag_req_end; ++it)
  {
    it->tag_checked = true;
    if (!m_microop_cache_ptr->Ishit(it->ip))
    {
      all_ip_hit_uop_cache = false;
    }
  }

  static_cast<CACHE *>(L1I_bus.lower_level)->check_tag(tag_req_begin->ip, tag_req_begin->instr_id, all_ip_hit_uop_cache);
}

bool O3_CPU::do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end)
{
  PACKET fetch_packet;
  fetch_packet.v_address = begin->ip;
  fetch_packet.instr_id = begin->instr_id;
  fetch_packet.ip = begin->ip;
  fetch_packet.instr_depend_on_me = {begin, end};

  if constexpr (champsim::uop_debug_print)
  {
    std::cout << "[FETCH] instr_id: " << begin->instr_id << " ip: " << std::hex << begin->ip << std::dec << " @" << current_cycle << std::endl;
  }

  if constexpr (champsim::debug_print_after_warmup)
  {
    if (!warmup)
    {
      std::cout << "[IFETCH] " << __func__ << " instr_id: " << begin->instr_id << std::hex;
      std::cout << " ip: " << begin->ip << std::dec << " dependents: " << std::size(fetch_packet.instr_depend_on_me);
      std::cout << " event_cycle: " << begin->event_cycle << std::endl;
    }
  }
  return L1I_bus.issue_read(fetch_packet);
}

void O3_CPU::promote_to_decode()
{
  bool started_with_uop_hit = false;
  unsigned available_fetch_bandwidth = FETCH_WIDTH;

  if (IFETCH_BUFFER.front().uop_cache_hit)
  {
    available_fetch_bandwidth = UOP_CACHE_THROUGHPUT;
    started_with_uop_hit = true;
  }

  while (available_fetch_bandwidth > 0 && !IFETCH_BUFFER.empty() && std::size(DECODE_BUFFER) < DECODE_BUFFER_SIZE && IFETCH_BUFFER.front().fetched == COMPLETED)
  {
    // in one cycle only fetch from either uop cache or from decoders
    if (!IFETCH_BUFFER.front().uop_cache_hit && started_with_uop_hit)
    {
      return;
    }

    if (IFETCH_BUFFER.front().uop_cache_hit && !started_with_uop_hit)
    {
      return;
    }

    IFETCH_BUFFER.front().event_cycle = current_cycle + ((warmup || IFETCH_BUFFER.front().decoded) ? 0 : DECODE_LATENCY);
    DECODE_BUFFER.push_back(std::move(IFETCH_BUFFER.front()));
    if (!IFETCH_BUFFER.front().uop_cache_hit)
    {
      num_decode++;
    }
    get_profiler_ptr->hpca.total_time_in_ftq += current_cycle - IFETCH_BUFFER.front().ftq_cycle;
    IFETCH_BUFFER.pop_front();

    available_fetch_bandwidth--;
  }

  // check for deadlock
  if (!std::empty(IFETCH_BUFFER) && (IFETCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
  {
    print_deadlock();
    assert(false); // throw champsim::deadlock{cpu}; //the throw gives a segfault dont know why
  }
}

void O3_CPU::decode_instruction()
{
  demand_decode_this_cycle.clear();
  std::size_t available_decode_bandwidth = DECODE_WIDTH;
  bool needs_decoders = false;

  // Send decoded instructions to dispatch
  while (available_decode_bandwidth > 0 && !std::empty(DECODE_BUFFER) && DECODE_BUFFER.front().event_cycle <= current_cycle && std::size(DISPATCH_BUFFER) < DISPATCH_BUFFER_SIZE)
  {
    ooo_model_instr &db_entry = DECODE_BUFFER.front();

    // decode is doing a uop cache miss so it needs the decoders
    if (!db_entry.uop_cache_hit)
    {
      get_profiler_ptr->m_decode_stats.demand_decode++;
      needs_decoders = true;
      demand_decode_this_cycle.push_back(db_entry.ip);
      // cout << "Miss found CP " << current_cycle << endl;
    }

    do_dib_update(db_entry, false);

    // Resume fetch
    if (db_entry.branch_mispredicted)
    {
      // These branches detect the misprediction at decode
      if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL) || (db_entry.branch_type == BRANCH_CONDITIONAL && db_entry.branch_taken == db_entry.branch_prediction))
      {
        // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
        db_entry.branch_mispredicted = 0;
        // pay misprediction penalty
        fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
      }
    }

    if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL) || (db_entry.branch_type == BRANCH_CONDITIONAL && db_entry.branch_taken == db_entry.branch_prediction))
    {
      // if (db_entry.instr_id == 89747618) {
      // cout << "Resolved decode " << db_entry.instr_id << " @" << current_cycle << endl;
      // }
      this->update_mshr(db_entry);
      last_br_miss = db_entry.branch_mispredicted;
    }

    // Add to dispatch
    db_entry.event_cycle = current_cycle + (warmup ? 0 : DISPATCH_LATENCY);
    num_dispatch++;
    DISPATCH_BUFFER.push_back(std::move(db_entry));
    DECODE_BUFFER.pop_front();

    available_decode_bandwidth--;
  }

  if (!needs_decoders)
  {
    allow_pref_to_use_decoders_cp = true;
    // cout << "Allow alternate now CP " << current_cycle << endl;
  }

  // check for deadlock
  if (!std::empty(DECODE_BUFFER) && (DECODE_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

// here we try to dispatch 6 as in real implementation we will have a buffer and thus the
// dispatch will be able to issue 6 instruction so no need to switching here
void O3_CPU::promote_to_dispatch_from_decoders()
{
  std::size_t available_dispatch_bandwidth = DISPATCH_WIDTH;
  get_profiler_ptr->total_dispatch_cycle++;
  // dispatch DISPATCH_WIDTH instructions into the ROB
  while (available_dispatch_bandwidth > 0 && !std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().event_cycle < current_cycle && std::size(ROB) != ROB_SIZE && ((std::size_t)std::count_if(std::begin(LQ), std::end(LQ), std::not_fn(is_valid<decltype(LQ)::value_type>{})) >= std::size(DISPATCH_BUFFER.front().source_memory)) && ((std::size(DISPATCH_BUFFER.front().destination_memory) + std::size(SQ)) <= SQ_SIZE))
  {
    ROB.push_back(std::move(DISPATCH_BUFFER.front()));
    num_rob++;
    DISPATCH_BUFFER.pop_front();
    do_memory_scheduling(ROB.back());

    available_dispatch_bandwidth--;
  }

  // check for deadlock
  if (!std::empty(DISPATCH_BUFFER) && (DISPATCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};

  get_profiler_ptr->total_dispatch_width += DISPATCH_WIDTH;
  get_profiler_ptr->avg_dispatched += (DISPATCH_WIDTH - available_dispatch_bandwidth);

  if (available_dispatch_bandwidth != 0)
  { // full dispatch bandwidth is not used
    if (UOP_BUFFER.empty() && DECODE_BUFFER.empty() && DISPATCH_BUFFER.empty() && IFETCH_BUFFER.empty())
    {
      get_profiler_ptr->cpu_stalls.empty_dispatch += (available_dispatch_bandwidth);
    }
    if (DISPATCH_BUFFER.empty() && UOP_BUFFER.empty())
    {
      get_profiler_ptr->cpu_stalls.front_end_empty += (available_dispatch_bandwidth);
      get_profiler_ptr->front_end_stall++;
      if (branch_miss_front_end_stall)
      {
        get_profiler_ptr->branch_miss_front_end_stall++;
      }
      // cout << "Not dispatched due to empty frontend " << (available_dispatch_bandwidth) << " @" << current_cycle << endl;
    }
    else if (std::size(ROB) == ROB_SIZE || std::size(LQ) == LQ_SIZE || std::size(SQ) == SQ_SIZE)
    {
      get_profiler_ptr->cpu_stalls.back_end_full += (available_dispatch_bandwidth);
      // cout << "Not dispatched due to full backend " << (available_dispatch_bandwidth) << " @" << current_cycle << endl;
    }
  }

  // if (available_dispatch_bandwidth != 0) { // full dispatch bandwidth is not used
  //   if (UOP_BUFFER.empty() && DECODE_BUFFER.empty() && DISPATCH_BUFFER.empty() && IFETCH_BUFFER.empty()) {
  //     get_profiler_ptr->cpu_stalls.empty_dispatch += (available_dispatch_bandwidth);
  //   }
  //   if (DISPATCH_BUFFER.empty() && UOP_BUFFER.empty()) {
  //     get_profiler_ptr->cpu_stalls.front_end_empty += (available_dispatch_bandwidth);
  //     get_profiler_ptr->front_end_stall++;
  //     if (branch_miss_front_end_stall) {
  //       get_profiler_ptr->branch_miss_front_end_stall++;
  //     }
  //   } else if (std::size(ROB) == ROB_SIZE || std::size(LQ) == LQ_SIZE || std::size(SQ) == SQ_SIZE) {
  //     get_profiler_ptr->cpu_stalls.back_end_full += (available_dispatch_bandwidth);
  //   }
  // }

  // // check for deadlock
  // if (!std::empty(DISPATCH_BUFFER) && (DISPATCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle) {
  //   cout << "DEADLOCK!!! DISPATCH_BUFFER instr_id: " << DISPATCH_BUFFER.front().instr_id << endl;
  //   throw champsim::deadlock{cpu};
  // }
  // if (!std::empty(UOP_BUFFER) && (UOP_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle) {
  //   cout << "DEADLOCK!!! UOP_BUFFER instr_id: " << UOP_BUFFER.front().instr_id << endl;
  //   throw champsim::deadlock{cpu};
  // }
}

bool O3_CPU::dispatch_instruction(ooo_model_instr &instr)
{
  if (instr.event_cycle < current_cycle && std::size(ROB) != ROB_SIZE && ((std::size_t)std::count_if(std::begin(LQ), std::end(LQ), std::not_fn(is_valid<decltype(LQ)::value_type>{})) >= std::size(instr.source_memory)) && ((std::size(instr.destination_memory) + std::size(SQ)) <= SQ_SIZE))
  {
    // cout << "Disptached " << instr.instr_id << " @" << current_cycle << endl;
    instr.dispatched_cycle = current_cycle;
    get_profiler_ptr->prediction_to_dispatch += (current_cycle - instr.cycle_added_in_ftq);
    ROB.push_back(std::move(instr));
    assert(std::size(ROB) <= ROB_SIZE);
    do_memory_scheduling(ROB.back());
    last_dispatched_id = instr.instr_id;
    return true;
  }

  if (std::size(ROB) == ROB_SIZE)
    get_profiler_ptr->cpu_stalls.rob_full++;
  if (std::size(LQ) == LQ_SIZE)
    get_profiler_ptr->cpu_stalls.lq_full++;
  if (std::size(SQ) == SQ_SIZE)
    get_profiler_ptr->cpu_stalls.sq_full++;
  return false;
}

void O3_CPU::stream_instruction()
{
  // the size is kept same as the decode buffer size
  auto fetch_bandwidth = std::min<std::size_t>(UOP_CACHE_THROUGHPUT, DECODE_BUFFER_SIZE - std::size(UOP_BUFFER));
  if (std::size(UOP_BUFFER) >= UOP_BUFFER_SIZE)
  {
    if constexpr (champsim::uop_debug_print)
    {
      cout << "UOP_BUFFER FULL!! " << current_cycle << endl;
    }
    get_profiler_ptr->cpu_stalls.uop_queue_full++;
  }
  auto [uop_window_begin, uop_window_end] =
      champsim::get_span_p(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), (fetch_bandwidth),
                           [cycle = current_cycle](const auto &x)
                           { return x.uop_cache_hit && x.dib_checked == COMPLETED; });

  std::for_each(uop_window_begin, uop_window_end, [&, this](auto &db_entry)
                {
    assert(db_entry.uop_cache_hit);

    if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL)
        || (db_entry.branch_type == BRANCH_CONDITIONAL && db_entry.branch_taken == db_entry.branch_prediction)) {
      this->update_mshr(db_entry);
      last_br_miss = db_entry.branch_mispredicted;
    }

    // Resume fetch
    if (db_entry.branch_mispredicted) {
      // These branches detect the misprediction at decode
      if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL)
          || (db_entry.branch_type == BRANCH_CONDITIONAL && db_entry.branch_taken == db_entry.branch_prediction)) {
        // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute

        if (db_entry.hard_to_predict_branch) {
          // pref from the spec side
        }

        get_profiler_ptr->stats_branch.miss_decode++;
        db_entry.branch_mispredicted = 0;
        // last_br_miss_type = db_entry.branch_type;
        get_profiler_ptr->speculative_miss.avg_resolve_time[db_entry.branch_type] += current_cycle - db_entry.ftq_cycle;
        last_br_miss_instr = db_entry;

        // pay misprediction penalty
        this->fetch_resume_cycle = this->current_cycle + BRANCH_MISPREDICT_PENALTY;
        // cout << "Mispredicted branch " << db_entry.instr_id << " resume at " << this->fetch_resume_cycle << " @" << current_cycle << endl;
      }
    }

    // Add to dispatch
    db_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : this->DISPATCH_LATENCY); });

  std::move(uop_window_begin, uop_window_end, std::back_inserter(UOP_BUFFER));
  IFETCH_BUFFER.erase(uop_window_begin, uop_window_end);

  assert(std::size(UOP_BUFFER) <= UOP_BUFFER_SIZE);

  if (!std::empty(IFETCH_BUFFER) && (IFETCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
  {
    cout << "DEADLOCK!!! IFETCH_BUFFER " << endl;
    throw champsim::deadlock{cpu};
  }
}

void O3_CPU::do_dib_update(ooo_model_instr &instr, bool is_prefetch)
{
  // cout << "Added " << instr.ip << endl;
  m_microop_cache_ptr->Insert(instr.ip, current_cycle, instr.is_critical, instr.branch_type == BRANCH_DIRECT_JUMP, false, false, instr.is_branch, false); // ADDED NOT alternate path (demand)
}

void O3_CPU::schedule_instruction()
{
  auto search_bw = SCHEDULER_SIZE;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw > 0; ++rob_it)
  {
    if (rob_it->scheduled == 0)
      do_scheduling(*rob_it);

    if (rob_it->executed == 0)
      --search_bw;
  }
}

void O3_CPU::do_scheduling(ooo_model_instr &instr)
{
  // Mark register dependencies
  for (auto src_reg : instr.source_registers)
  {
    if (!std::empty(reg_producers[src_reg]))
    {
      ooo_model_instr &prior = reg_producers[src_reg].back();
      if (prior.registers_instrs_depend_on_me.empty() || prior.registers_instrs_depend_on_me.back().get().instr_id != instr.instr_id)
      {
        prior.registers_instrs_depend_on_me.push_back(instr);
        instr.num_reg_dependent++;
      }
    }
  }

  for (auto dreg : instr.destination_registers)
  {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto ins = std::lower_bound(begin, end, instr, [](const ooo_model_instr &lhs, const ooo_model_instr &rhs)
                                { return lhs.instr_id < rhs.instr_id; });
    reg_producers[dreg].insert(ins, std::ref(instr));
  }

  instr.scheduled = COMPLETED;
  instr.event_cycle = current_cycle + (warmup ? 0 : SCHEDULING_LATENCY);

  get_profiler_ptr->detailed_energy.rf_reads += instr.source_registers.size();
}

void O3_CPU::execute_instruction()
{
  auto exec_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && exec_bw > 0; ++rob_it)
  {
    if (rob_it->scheduled == COMPLETED && rob_it->executed == 0 && rob_it->num_reg_dependent == 0 && rob_it->event_cycle <= current_cycle)
    {
      do_execution(*rob_it);
      --exec_bw;
      if constexpr (champsim::uop_debug_print)
      {
        std::cout << "[EXECUTION] instr_id: " << rob_it->instr_id << " ip: " << std::hex << rob_it->ip << std::dec << " @" << current_cycle << std::endl;
      }
    }
  }
}

void O3_CPU::do_execution(ooo_model_instr &rob_entry)
{
  rob_entry.executed = INFLIGHT;
  rob_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark LQ entries as ready to translate
  for (auto &lq_entry : LQ)
    if (lq_entry.has_value() && lq_entry->instr_id == rob_entry.instr_id)
      lq_entry->event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark SQ entries as ready to translate
  for (auto &sq_entry : SQ)
    if (sq_entry.instr_id == rob_entry.instr_id)
      sq_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // if constexpr (champsim::debug_print_after_warmup) {
  //   if (!warmup)
  //     std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " event_cycle: " << rob_entry.event_cycle << std::endl;
  // }
}

void O3_CPU::do_memory_scheduling(ooo_model_instr &instr)
{
  // load
  for (auto &smem : instr.source_memory)
  {
    auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), is_valid<decltype(LQ)::value_type>{});
    assert(q_entry != std::end(LQ));
    q_entry->emplace(LSQ_ENTRY{instr.instr_id,
                               smem,
                               instr.ip,
                               std::numeric_limits<uint64_t>::max(),
                               {instr.asid[0], instr.asid[1]},
                               false,
                               std::numeric_limits<uint64_t>::max(),
                               {}}); // add it to the load queue

    // Check for forwarding
    auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem](const auto &lhs, const auto &rhs)
                                  { return lhs.virtual_address != smem || (rhs.virtual_address == smem && lhs.instr_id < rhs.instr_id); });
    if (sq_it != std::end(SQ) && sq_it->virtual_address == smem)
    {
      if (sq_it->fetch_issued)
      { // Store already executed
        q_entry->reset();
        ++instr.completed_mem_ops;

        if constexpr (champsim::debug_print)
          std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " forwards from " << sq_it->instr_id << std::endl;
      }
      else
      {
        assert(sq_it->instr_id < instr.instr_id);   // The found SQ entry is a prior store
        sq_it->lq_depend_on_me.push_back(*q_entry); // Forward the load when the store finishes
        (*q_entry)->producer_id = sq_it->instr_id;  // The load waits on the store to finish

        if constexpr (champsim::debug_print)
          std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " waits on " << sq_it->instr_id << std::endl;
      }
    }
  }

  get_profiler_ptr->detailed_energy.lq_writes += instr.source_memory.size();

  // store
  for (auto &dmem : instr.destination_memory)
    SQ.push_back({instr.instr_id,
                  dmem,
                  instr.ip,
                  std::numeric_limits<uint64_t>::max(),
                  {instr.asid[0], instr.asid[1]},
                  false,
                  std::numeric_limits<uint64_t>::max(),
                  {}}); // add it to the store queue
  get_profiler_ptr->detailed_energy.sq_writes += instr.destination_memory.size();
}

void O3_CPU::operate_lsq()
{
  auto store_bw = SQ_WIDTH;

  const auto complete_id = std::empty(ROB) ? std::numeric_limits<uint64_t>::max() : ROB.front().instr_id;
  auto do_complete = [cycle = current_cycle, complete_id, this](const auto &x)
  {
    return x.instr_id < complete_id && x.event_cycle <= cycle && this->do_complete_store(x);
  };

  auto unfetched_begin = std::partition_point(std::begin(SQ), std::end(SQ), [](const auto &x)
                                              { return x.fetch_issued; });
  auto [fetch_begin, fetch_end] = champsim::get_span_p(unfetched_begin, std::end(SQ), store_bw,
                                                       [cycle = current_cycle](const auto &x)
                                                       { return !x.fetch_issued && x.event_cycle <= cycle; });
  store_bw -= std::distance(fetch_begin, fetch_end);
  std::for_each(fetch_begin, fetch_end, [cycle = current_cycle, this](auto &sq_entry)
                {
    this->do_finish_store(sq_entry);
    sq_entry.fetch_issued = true;
    sq_entry.event_cycle = cycle; });

  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(SQ), std::cend(SQ), store_bw, do_complete);
  store_bw -= std::distance(complete_begin, complete_end);
  SQ.erase(complete_begin, complete_end);

  auto load_bw = LQ_WIDTH;

  for (auto &lq_entry : LQ)
  {
    if (load_bw > 0 && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && !lq_entry->fetch_issued && lq_entry->event_cycle < current_cycle)
    {
      auto success = execute_load(*lq_entry);
      if (success)
      {
        --load_bw;
        lq_entry->fetch_issued = true;
      }
    }
  }
}

void O3_CPU::do_finish_store(const LSQ_ENTRY &sq_entry)
{
  sq_entry.finish(std::begin(ROB), std::end(ROB));

  // Release dependent loads
  for (std::optional<LSQ_ENTRY> &dependent : sq_entry.lq_depend_on_me)
  {
    assert(dependent.has_value()); // LQ entry is still allocated
    assert(dependent->producer_id == sq_entry.instr_id);

    dependent->finish(std::begin(ROB), std::end(ROB));
    dependent.reset();
  }
}

bool O3_CPU::do_complete_store(const LSQ_ENTRY &sq_entry)
{
  PACKET data_packet;
  data_packet.v_address = sq_entry.virtual_address;
  data_packet.instr_id = sq_entry.instr_id;
  data_packet.ip = sq_entry.ip;

  // if constexpr (champsim::debug_print_after_warmup) {
  //   if (!warmup)
  //     std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::endl;
  // }
  get_profiler_ptr->detailed_energy.lq_searches++;
  return L1D_bus.issue_write(data_packet);
}

bool O3_CPU::execute_load(const LSQ_ENTRY &lq_entry)
{
  PACKET data_packet;
  data_packet.v_address = lq_entry.virtual_address;
  data_packet.instr_id = lq_entry.instr_id;
  data_packet.ip = lq_entry.ip;

  // if constexpr (champsim::debug_print_after_warmup) {
  //   if (!warmup)
  //     std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::endl;
  // }

  get_profiler_ptr->detailed_energy.sq_searches++;
  return L1D_bus.issue_read(data_packet);
}

void O3_CPU::do_complete_execution(ooo_model_instr &instr)
{
  for (auto dreg : instr.destination_registers)
  {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto elem = std::find_if(begin, end, [id = instr.instr_id](ooo_model_instr &x)
                             { return x.instr_id == id; });
    assert(elem != end);
    reg_producers[dreg].erase(elem);
  }

  if constexpr (champsim::uop_debug_print)
  {
    std::cout << "[EXECUTION_DONE] instr_id: " << instr.instr_id << " ip: " << std::hex << instr.ip << std::dec << " @" << current_cycle << std::endl;
  }

  // if (instr.instr_id == 24708)
  //   assert(false);
  instr.executed = COMPLETED;
  get_profiler_ptr->detailed_energy.rob_writes += instr.destination_registers.size();

  if (instr.is_branch)
  {
    // cout << "Resolved execute " << instr.instr_id << " @" << current_cycle << endl;
    this->update_mshr(instr);
    last_br_miss = instr.branch_mispredicted;
  }
  for (ooo_model_instr &dependent : instr.registers_instrs_depend_on_me)
  {
    dependent.num_reg_dependent--;
    assert(dependent.num_reg_dependent >= 0);

    if (dependent.num_reg_dependent == 0)
      dependent.scheduled = COMPLETED;
  }

  if (instr.instr_after_branch_mispred)
  {
    get_profiler_ptr->avg_branch_recovery_time.first += (current_cycle - instr.cycle_added_in_ftq);
    get_profiler_ptr->avg_branch_recovery_time.second++;
    instr.instr_after_branch_mispred = false;
    // cout << "It took for instr " << std::hex << instr.ip << std::dec << " delta " << (current_cycle - instr.cycle_added_in_ftq) << " @" << current_cycle <<
    // endl;
  }

  if (instr.branch_mispredicted)
  {
    last_br_miss_type = instr.branch_type;
    get_profiler_ptr->bp_btb_stats.misses_at_exec[instr.branch_type]++;
    get_profiler_ptr->speculative_miss.avg_resolve_time[instr.branch_type] += current_cycle - instr.ftq_cycle;
    last_br_miss_instr = instr;

    if (instr.hard_to_predict_branch)
    {
      // pref from the spec side
    }

    get_profiler_ptr->stats_branch.miss_execute++;
    fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;

    // only at execute
    // ideal_br_in_process = true;

    // only conditional
    //  if (instr.branch_type == BRANCH_CONDITIONAL) {
    //    ideal_br_in_process = true;
    //  }
  }
}

void O3_CPU::update_mshr(ooo_model_instr &instr)
{
  for (auto &ips : ips_to_prefetch)
  {
    // ips_to_prefetch.pop_front(); //this will remove the pref after the br is resolved
    if (ips.by_isntr == instr.instr_id && !ips.br_already_resolved)
    {
      ips.br_already_resolved = true; // this will just mark those entries that the br is resolved
      // if (instr.instr_id == 50103053) cout << "Marking entries " << ips.ip << " to prefetch as the branch is already resolved! " << ips.br_already_resolved
      // << " @" << current_cycle << endl;
    }
  }

  // if (instr.instr_id == 1465118) cout << "BR resolved " << current_cycle << endl;
  //  update the UOP CACHE mshr that the branch is resolved
  if (instr.hard_to_predict_branch)
  { // h2p and not mispredicted
    if (!instr.branch_mispredicted)
    {
      for (auto &mshr_entry : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
      {
        if (mshr_entry.by_instr == instr.instr_id)
        {                              // invalidate all the lines as it will not be used anymore
          mshr_entry.is_valid = false; // invalidate the entry
          mshr_entry.h2p_hit = false;
          mshr_entry.branch_resolved = true;
          // if (mshr_entry.pref_done) { // markt the entries in micro op cache as prefetched wrong
          //   m_microop_cache_ptr->markWrongPrefetch(mshr_entry.ips_in_line, mshr_entry.by_instr);
          //   // m_microop_cache_ptr->invalidateEntries(mshr_entry.ips_in_line, mshr_entry.by_instr);
          // }
        }
      }
    }
    else
    {
      for (auto &mshr_entry : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
      {
        if (mshr_entry.by_instr == instr.instr_id)
        {
          mshr_entry.branch_resolved = true;
          mshr_entry.h2p_hit = true;
        }
      }
    }
  }
}

void O3_CPU::complete_inflight_instruction()
{
  // update ROB entries with completed executions
  auto complete_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw > 0; ++rob_it)
  {
    if ((rob_it->executed == INFLIGHT) && (rob_it->event_cycle <= current_cycle) && rob_it->completed_mem_ops == rob_it->num_mem_ops())
    {
      do_complete_execution(*rob_it);
      --complete_bw;
    }
  }
}

void O3_CPU::handle_memory_return()
{
  for (auto l1i_bw = FETCH_WIDTH, to_read = L1I_BANDWIDTH; l1i_bw > 0 && to_read > 0 && !L1I_bus.PROCESSED.empty(); --to_read)
  {
    PACKET &l1i_entry = L1I_bus.PROCESSED.front();

    while (l1i_bw > 0 && !l1i_entry.instr_depend_on_me.empty())
    {
      ooo_model_instr &fetched = l1i_entry.instr_depend_on_me.front();
      // some entries in instr_depend_on_me gets the fetched bit reset in the ideal_br_config which is weird
      //  so to have a quick fix we check the FTQ and update the fetched flag accordingly
      auto ftq_entry =
          std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [line_id = fetched.instr_id](ooo_model_instr &x)
                       { return x.instr_id == line_id; });

      if (ftq_entry != end(IFETCH_BUFFER))
      {
        fetched.fetched = ftq_entry->fetched;
      }

      if ((fetched.ip >> LOG2_BLOCK_SIZE) == (l1i_entry.v_address >> LOG2_BLOCK_SIZE) && fetched.fetched != 0)
      {
        fetched.fetched = COMPLETED;
        ftq_entry->fetched = COMPLETED;

        --l1i_bw;
        if constexpr (champsim::uop_debug_print)
        {
          std::cout << "[IFETCH] " << __func__ << " instr_id: " << fetched.instr_id << " ip " << std::hex << fetched.ip << std::dec << " fetch completed @"
                    << current_cycle << std::endl;
        }
      }

      l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
    }

    for (auto &mshr : static_cast<CACHE *>(L1I_bus.lower_level)->UOP_CACHE_MSHR)
    {
      if (mshr.cache_line_addr == (l1i_entry.v_address >> LOG2_BLOCK_SIZE) && !mshr.pref_done)
      {
        mshr.pref_done = true;
        for (auto &ips : mshr.ips_in_line)
        {
          for (auto &ip : get_profiler_ptr->alt_path[mshr.by_instr].ips)
          {
            if (ips.ip == ip.first)
            {
              ip.second.pref_receive_cycle = current_cycle;
            }
          }

          recently_prefetched_ips.push_back(ips.ip);
          if (recently_prefetched_ips.size() >= 1024)
          {
            recently_prefetched_ips.pop_front();
          }

          if (std::size(PRF_DECODE_BUFFER) < 512)
          {
            prefetch_decode_entry pd_entry;
            pd_entry.ip = ips.ip;
            pd_entry.was_from_branch_miss = mshr.pq_pkt.by_br_miss;
            pd_entry.by_instr = mshr.by_instr;
            pd_entry.taken = false;
            for (auto &entry : mshr.predicted_taken_branches)
            {
              if (ips.ip == entry)
              {
                pd_entry.taken = true;
                break;
              }
            }

            PRF_DECODE_BUFFER.push_back(pd_entry);
          }
        }
      }
    }

    // remove this entry if we have serviced all of its instructions
    if (l1i_entry.instr_depend_on_me.empty())
      L1I_bus.PROCESSED.pop_front();
  }

  auto l1d_it = std::begin(L1D_bus.PROCESSED);
  for (auto l1d_bw = L1D_BANDWIDTH; l1d_bw > 0 && l1d_it != std::end(L1D_bus.PROCESSED); --l1d_bw, ++l1d_it)
  {
    for (auto &lq_entry : LQ)
    {
      if (lq_entry.has_value() && lq_entry->fetch_issued && lq_entry->virtual_address >> LOG2_BLOCK_SIZE == l1d_it->v_address >> LOG2_BLOCK_SIZE)
      {
        lq_entry->finish(std::begin(ROB), std::end(ROB));
        lq_entry.reset();
      }
    }
  }
  L1D_bus.PROCESSED.erase(std::begin(L1D_bus.PROCESSED), l1d_it);
}

void O3_CPU::retire_rob()
{
  auto [retire_begin, retire_end] = champsim::get_span_p(std::cbegin(ROB), std::cend(ROB), RETIRE_WIDTH, [](const auto &x)
                                                         { return x.executed == COMPLETED; });
  if constexpr (champsim::uop_debug_print)
  {
    std::for_each(retire_begin, retire_end,
                  [&](const auto &x)
                  { std::cout << "[RETIRE] instr_id: " << x.instr_id << " is retired @" << current_cycle << std::endl; });
  }

  std::for_each(retire_begin, retire_end, [&](const auto &x)
                {
    get_profiler_ptr->m_total_retired++;
    get_profiler_ptr->detailed_energy.rf_writes += x.destination_registers.size();

    // cout << "Before critic " << get_profiler_ptr->critical_uop_checkss << " hit " << get_profiler_ptr->critical_uop_hits << " ip " << x.ip << " critial "
    //      << x.is_critical << endl;
    if (x.is_critical) {
      checks++;

      // cout << "Critial found " << x.ip << " hit " << x.uop_cache_hit << endl;
      get_profiler_ptr->hpca.critical_checks++;
      if (x.uop_cache_hit) {
        hits++;
        get_profiler_ptr->hpca.critical_hits++;
      }
    }
    // cout << "Checks " << get_profiler_ptr->hpca.critical_checks << " hits " << get_profiler_ptr->hpca.critical_hits << endl;
    // cout << "After critic " << get_profiler_ptr->critical_uop_checkss << " hit " << get_profiler_ptr->critical_uop_hits << endl;

    if (x.hard_to_predict_branch) {
      int num_branches = 0;
      for (auto& rob : ROB) {
        if (rob.instr_id_4b > x.instr_id_4b) { // check only younger instructions that come in the pipeline after the h2p branch
          if (rob.alt_miss) {
            get_profiler_ptr->miss_distance_from_h2p += num_branches;
            get_profiler_ptr->total_counts_from_h2p++;
            if (num_branches <= 32) {
              get_profiler_ptr->less_than_8[num_branches]++;
            }
            break;
          }
          if (rob.is_branch) {
            num_branches++;
          }
        }
      }
    }

    if (x.is_branch) {

      if (x.branch_miss) {
        get_profiler_ptr->total_miss_distance += x.instr_id - last_br_miss_instr_id;
        last_br_miss_instr_id = x.instr_id;
      }

      // update the H2P stats
      if (x.hard_to_predict_branch) {
        if (x.branch_miss) {
          get_profiler_ptr->stats_branch.critical_miss++;
          // if (!warmup)
          //   cout << "x.instr_id " << x.instr_id << " h2p predicted correctly " << endl;
        }

        get_profiler_ptr->total_h2p_distance += x.instr_id - last_h2p_instr_id;
        get_profiler_ptr->stats_branch.marked_critical++;
        get_profiler_ptr->h2p_map[x.ip]++;
        last_h2p_instr_id = x.instr_id;
      }
    }

    m_h2p_ptr->UpdateBranchHistory(x);

    // if (x.instr_id == 78180) {
    //   assert(false);
    // }

    if (x.is_critical) {
      get_profiler_ptr->avg_instr_saved++;
    }
    if (x.instr_id < last_retired_instr) {
      cout << "Instructions ordering is violated!!" << endl;
      assert(false);
    } });
  last_retired_instr = retire_end->instr_id;

  num_retired += std::distance(retire_begin, retire_end);

  // cout << retire_end->instr_id << endl;
  ROB.erase(retire_begin, retire_end);

  // Check for deadlock
  if (!std::empty(ROB) && (ROB.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void CacheBus::return_data(const PACKET &packet) { PROCESSED.push_back(packet); }

void O3_CPU::print_deadlock()
{
  std::cout << "DEADLOCK! CPU " << cpu << " cycle " << current_cycle << std::endl;

  if (!std::empty(IFETCH_BUFFER))
  {
    std::cout << "IFETCH_BUFFER head";
    std::cout << " instr_id: " << IFETCH_BUFFER.front().instr_id;
    std::cout << " cache_line: " << std::hex << (IFETCH_BUFFER.front().ip >> LOG2_BLOCK_SIZE) << std::dec;
    std::cout << " fetched: " << +IFETCH_BUFFER.front().fetched;
    std::cout << " uop_hit: " << +IFETCH_BUFFER.front().uop_cache_hit;
    std::cout << " scheduled: " << +IFETCH_BUFFER.front().scheduled;
    std::cout << " executed: " << +IFETCH_BUFFER.front().executed;
    std::cout << " num_reg_dependent: " << +IFETCH_BUFFER.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << IFETCH_BUFFER.front().num_mem_ops() - IFETCH_BUFFER.front().completed_mem_ops;
    std::cout << " event: " << IFETCH_BUFFER.front().event_cycle;
    std::cout << std::endl;
  }
  else
  {
    std::cout << "IFETCH_BUFFER empty" << std::endl;
  }

  if (!std::empty(ROB))
  {
    std::cout << "ROB head";
    std::cout << " instr_id: " << ROB.front().instr_id;
    std::cout << " fetched: " << +ROB.front().fetched;
    std::cout << " scheduled: " << +ROB.front().scheduled;
    std::cout << " executed: " << +ROB.front().executed;
    std::cout << " num_reg_dependent: " << +ROB.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << ROB.front().num_mem_ops() - ROB.front().completed_mem_ops;
    std::cout << " event: " << ROB.front().event_cycle;
    std::cout << std::endl;
  }
  else
  {
    std::cout << "ROB empty" << std::endl;
  }

  // print LQ entry
  std::cout << "Load Queue Entry" << std::endl;
  for (auto lq_it = std::begin(LQ); lq_it != std::end(LQ); ++lq_it)
  {
    if (lq_it->has_value())
    {
      std::cout << "[LQ] entry: " << std::distance(std::begin(LQ), lq_it) << " instr_id: " << (*lq_it)->instr_id << " address: " << std::hex
                << (*lq_it)->virtual_address << std::dec << " fetched_issued: " << std::boolalpha << (*lq_it)->fetch_issued << std::noboolalpha
                << " event_cycle: " << (*lq_it)->event_cycle;
      if ((*lq_it)->producer_id != std::numeric_limits<uint64_t>::max())
        std::cout << " waits on " << (*lq_it)->producer_id;
      std::cout << std::endl;
    }
  }

  // print SQ entry
  std::cout << std::endl
            << "Store Queue Entry" << std::endl;
  for (auto sq_it = std::begin(SQ); sq_it != std::end(SQ); ++sq_it)
  {
    std::cout << "[SQ] entry: " << std::distance(std::begin(SQ), sq_it) << " instr_id: " << sq_it->instr_id << " address: " << std::hex
              << sq_it->virtual_address << std::dec << " fetched: " << std::boolalpha << sq_it->fetch_issued << std::noboolalpha
              << " event_cycle: " << sq_it->event_cycle << " LQ waiting: ";
    for (std::optional<LSQ_ENTRY> &lq_entry : sq_it->lq_depend_on_me)
      std::cout << lq_entry->instr_id << " ";
    std::cout << std::endl;
  }
}

void LSQ_ENTRY::finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const
{
  auto rob_entry = std::partition_point(begin, end, [id = this->instr_id](auto x)
                                        { return x.instr_id < id; });
  assert(rob_entry != end);
  assert(rob_entry->instr_id == this->instr_id);

  ++rob_entry->completed_mem_ops;
  assert(rob_entry->completed_mem_ops <= rob_entry->num_mem_ops());
}

bool CacheBus::issue_read(PACKET data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.cpu = cpu;
  data_packet.type = LOAD;
  data_packet.to_return = {this};
  return lower_level->add_rq(data_packet);
}

bool CacheBus::issue_write(PACKET data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.cpu = cpu;
  data_packet.type = WRITE;

  return lower_level->add_wq(data_packet);
}