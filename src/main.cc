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
#include <array>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
// #include "micro_op_cache.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "phase_info.h"
#include "profiler.h"
#include "ptw.h"
#include "stats_printer.h"
#include "util.h"
#include "vmem.h"

void init_structures();

#include "core_inst.inc"

const char* exe_name = "test";
const char* file_name = "~/champsim_default_file_name.stats";

int champsim_main(std::vector<std::reference_wrapper<O3_CPU>>& cpus, std::vector<std::reference_wrapper<champsim::operable>>& operables,
                  std::vector<champsim::phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names);

void signal_handler(int signal)
{
  std::cout << "Caught signal: " << signal << std::endl;
  abort();
}

template <typename CPU, typename C, typename D>
std::vector<champsim::phase_stats> zip_phase_stats(const std::vector<champsim::phase_info>& phases, const std::vector<CPU>& cpus,
                                                   const std::vector<C>& cache_list, const D& dram)
{
  std::vector<champsim::phase_stats> retval;

  for (std::size_t i = 0; i < std::size(phases); ++i) {
    if (!phases.at(i).is_warmup) {
      champsim::phase_stats stats;

      stats.name = phases.at(i).name;
      stats.trace_names = phases.at(i).trace_names;

      std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [i](const O3_CPU& cpu) { return cpu.sim_stats.at(i); });
      std::transform(std::begin(cache_list), std::end(cache_list), std::back_inserter(stats.sim_cache_stats),
                     [i](const CACHE& cache) { return cache.sim_stats.at(i); });
      std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
                     [i](const DRAM_CHANNEL& chan) { return chan.sim_stats.at(i); });
      std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [i](const O3_CPU& cpu) { return cpu.roi_stats.at(i); });
      std::transform(std::begin(cache_list), std::end(cache_list), std::back_inserter(stats.roi_cache_stats),
                     [i](const CACHE& cache) { return cache.roi_stats.at(i); });
      std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
                     [i](const DRAM_CHANNEL& chan) { return chan.roi_stats.at(i); });

      retval.push_back(stats);
    }
  }

  return retval;
}

int main(int argc, char** argv)
{
    srand (time(NULL));

  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  cout<<"Signal handler initialized"<<endl;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);
  // initialize knobs
  uint8_t knob_cloudsuite = 0;
  uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;
  bool knob_json_out = false;
  std::ofstream json_file;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
                                         {"json", optional_argument, 0, 'j'},
                                         {"file_name", required_argument, 0, 'f'},
                                         {"exe_name", required_argument, 0, 'e'},
                                         {"traces", no_argument, &traces_encountered, 1},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hc", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      for (O3_CPU& cpu : ooo_cpu)
        cpu.show_heartbeat = false;
      break;
    case 'c':
      knob_cloudsuite = 1;
      break;
    case 'f':
      file_name = optarg;
      break;
    case 'e':
      exe_name = optarg;
      break;
    case 'j':
      knob_json_out = true;
      if (optarg)
        json_file.open(optarg);
    case 0:
      break;
    default:
      abort();
    }
  }

  get_profiler_ptr->set_stats_file_name(string(file_name), string(exe_name));

  std::vector<std::string> trace_names{std::next(argv, optind), std::next(argv, argc)};

  std::vector<champsim::phase_info> phases{{champsim::phase_info{"Warmup", true, warmup_instructions, trace_names},
                                            champsim::phase_info{"Simulation", false, simulation_instructions, trace_names}}};

  std::cout << std::endl;
  std::cout << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl;
  std::cout << std::endl;
  std::cout << "Warmup Instructions: " << phases[0].length << std::endl;
  std::cout << "Simulation Instructions: " << phases[1].length << std::endl;
  std::cout << "Number of CPUs: " << std::size(ooo_cpu) << std::endl;
  std::cout << "Page size: " << PAGE_SIZE << std::endl;
  std::cout << std::endl;

  init_structures();

  champsim_main(ooo_cpu, operables, phases, knob_cloudsuite, trace_names);

  std::cout << std::endl;
  std::cout << "ChampSim completed all CPUs" << std::endl;
  std::cout << std::endl;

  auto phase_stats = zip_phase_stats(phases, ooo_cpu, caches, DRAM);

  champsim::plain_printer default_print{std::cout};
  default_print.print(phase_stats);
  get_profiler_ptr->print_json_stats_file();
  for (CACHE& cache : caches)
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : caches)
    cache.impl_replacement_final_stats();

  // int i = 0;
  // for (O3_CPU& cpu : ooo_cpu) {
  //   std::cout << std::endl << "CPU " << i << " Instr per access: " << (((double)cpu.right_path_size) / cpu.right_path_accesses) << std::endl;
  //   std::cout << std::endl
  //             << "CPU" << i << "_DIB TOTAL: " << get_profiler_ptr->uop_cache_read << " HITS: " << get_profiler_ptr->uop_cache_hit << " MISSES: " << get_profiler_ptr->uop_cache_read - get_profiler_ptr->uop_cache_hit
  //             << " HITRATE: " << ((get_profiler_ptr->uop_cache_hit * 100.0) / get_profiler_ptr->uop_cache_read) << "% num_instr " << get_profiler_ptr->uop_cache_read << std::endl;
  //   std::cout << std::endl
  //             << "CPU " << i << " instructions at stage IF: " << cpu.num_ifetch - cpu.num_decode << " DE: " << cpu.num_decode - cpu.num_dispatch
  //             << " DI: " << cpu.num_dispatch - cpu.num_rob << " ROB: " << cpu.num_rob - (cpu.num_retired - cpu.begin_sim_instr)
  //             << " RET: " << cpu.num_retired - cpu.begin_sim_instr << std::endl;
  //   std::cout << std::endl
  //             << "CPU " << i
  //             << " percent wrong_path instructions at stage IF: " << ((cpu.num_ifetch - cpu.num_decode) * 100.0) / (cpu.num_retired - cpu.begin_sim_instr)
  //             << " DE: " << ((cpu.num_decode - cpu.num_dispatch) * 100.0) / (cpu.num_retired - cpu.begin_sim_instr)
  //             << " DI: " << ((cpu.num_dispatch - cpu.num_rob) * 100.0) / (cpu.num_retired - cpu.begin_sim_instr)
  //             << " ROB: " << ((cpu.num_rob - (cpu.num_retired - cpu.begin_sim_instr)) * 100.0) / (cpu.num_retired - cpu.begin_sim_instr) << std::endl;
  //   std::cout << std::endl << "CPU " << i << " percent back-end wp known: " << ((cpu.num_rob_wp_info * 100.0) / cpu.num_rob_wp) << std::endl;

  //   std::cout << std::endl;
  //   std::cout << "Address generation events: " << cpu.addr_gen_events << std::endl;
  //   std::cout << "Register file reads: " << cpu.src_regs << std::endl;
  //   std::cout << "Register file writes: " << cpu.dst_regs << std::endl;
  //   std::cout << "ROB writes: " << cpu.rob_writes << std::endl;
  //   std::cout << "LQ writes: " << cpu.lq_writes << std::endl;
  //   std::cout << "LQ searches: " << cpu.lq_searches << std::endl;
  //   std::cout << "SQ writes: " << cpu.sq_writes << std::endl;
  //   std::cout << "SQ searches: " << cpu.sq_searches << std::endl;
  //   i++;
  // }
 
  if (knob_json_out) {
    if (json_file.is_open()) {
      champsim::json_printer printer{json_file};
      printer.print(phase_stats);
    } else {
      champsim::json_printer printer{std::cout};
      printer.print(phase_stats);
    }
  }

  return 0;
}
