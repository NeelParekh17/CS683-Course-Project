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

#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <bitset>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

// #include "alt_mbp.h"
// using namespace alt_mbp;

#include "alt_mpp.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "h2p.h"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "profiler.h"
#include "target_table.h"
#include "util.h"

#ifdef ALT_SIZE_8KB
#include "alt_tage_8KB.h"
#endif

#ifdef ALT_SIZE_64KB
#include "alt_tage_64KB.h"
#endif

enum STATUS { INFLIGHT = 1, COMPLETED = 2 };

enum FETCH_MODE { STREAM = 1, BUILD = 2 };

enum PATH_MODE { WRONG = 1, RIGHT = 2 };

class CacheBus : public MemoryRequestProducer
{
  uint32_t cpu;

public:
  std::deque<PACKET> PROCESSED;
  CacheBus(uint32_t cpu_idx, MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), cpu(cpu_idx) {}
  bool issue_read(PACKET packet);
  bool is_rq_free();
  bool issue_write(PACKET packet);
  void return_data(const PACKET& packet) override final;
};

struct cpu_stats {
  std::string name;
  uint64_t begin_instrs = 0, begin_cycles = 0;
  uint64_t end_instrs = 0, end_cycles = 0;
  uint64_t total_rob_occupancy_at_branch_mispredict = 0;

  std::array<long long, 8> total_branch_types = {};
  std::array<long long, 8> branch_type_misses = {};

  uint64_t instrs() const { return end_instrs - begin_instrs; }
  uint64_t cycles() const { return end_cycles - begin_cycles; }
};

struct LSQ_ENTRY {
  uint64_t instr_id = 0;
  uint64_t virtual_address = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
  bool fetch_issued = false;

  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
  std::vector<std::reference_wrapper<std::optional<LSQ_ENTRY>>> lq_depend_on_me;

  void finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const;
};

// cpu
class O3_CPU : public champsim::operable
{
public:
  uint64_t num_branch = 0;
  uint64_t num_branch_this_cycle = 0;

  uint32_t cpu = 0;

  // cycle
  uint64_t begin_phase_cycle = 0;
  uint64_t begin_phase_instr = 0;
  uint64_t finish_phase_cycle = 0;
  uint64_t finish_phase_instr = 0;
  uint64_t last_heartbeat_cycle = 0;
  uint64_t last_heartbeat_instr = 0;
  uint64_t next_print_instruction = STAT_PRINTING_PERIOD;

  // instruction
  uint64_t instr_unique_id = 0;
  uint64_t instr_unique_id_4b = 0;
  uint64_t instrs_to_read_this_cycle = 0;
  uint64_t instrs_to_fetch_this_cycle = 0;
  uint64_t num_retired = 0;

  uint64_t last_ip_in_ftq = 0;

  uint64_t last_ip_prefetched = 0;
  uint8_t last_br_type = 0;
  uint64_t last_target = 0;

  uint64_t last_retired_instr = 0;

  uint64_t instr_in_btwn = 0;

  bool show_heartbeat = true;

  using stats_type = cpu_stats;

  std::vector<stats_type> roi_stats{}, sim_stats{};

  // instruction buffer
  struct dib_shift {
    std::size_t shamt;
    auto operator()(uint64_t val) const { return val >> shamt; }
  };
  using dib_type = champsim::lru_table<uint64_t, dib_shift, dib_shift>;
  dib_type DIB;

  struct timing_entry_m {
    uint64_t uop_checked_event = 0;
    bool uop_hit = false;
  };
  enum BTB_Priority { DEMAND_PRIORITY, ALTERNATE_PRIORITY, NO_PRIORITY };

  struct prefetch_decode_entry {
    uint64_t ip = 0;
    bool was_from_branch_miss = false;
    uint64_t by_instr = 0;
    uint8_t br_type = 0;
    bool taken = false;
    bool is_alternate_path = false;  // ADDED THIS FIELD
  };

  struct path_entry_m {
    bool finished = false;
    uint64_t instr_id = 0;
    unsigned num_taken_br = 0;
    uint64_t ip;
    std::deque<std::pair<uint64_t, timing_entry_m>> ip_info;
  };

  struct alternate_path_info {
    ooo_model_instr by_instr;
    uint64_t target = 0;
    uint64_t bb_size = 0;
    bool is_valid = false;
    uint64_t start_cycle = 0;
    uint64_t event_cycle = 0;
    uint64_t num_of_taken_br = 0;
  };

  struct demand_ip_info {
    uint64_t ip = 0;
    bool dib_contention_checked = false;
    bool btb_contention_checked = false;
  };
  std::deque<alternate_path_info> ALT_PATH_TABLE;

  struct tmp_return_entry {
    uint64_t counter = 0;
    uint64_t target = 0;
    uint64_t bb_size = 0;
  };

  std::map<uint64_t, tmp_return_entry> tmp_return_map;

  std::deque<path_entry_m> ongoing_path_info;
  std::deque<uint64_t> recent_nested_prefetches;

  struct dib_entry_t {
    bool valid = false;
    unsigned lru = 999999;
  };

  const std::size_t dib_set, dib_way, dib_window;
  TargetTable<1024, 4> h2p_tt;

  std::map<uint64_t, uint8_t> pref_stop_map;
  std::map<uint64_t, uint64_t> h2p_tt_map;

  // instruction buffer
  using dib_t = std::vector<dib_entry_t>;
  dib_t DIB_profile{32 * 8};

  H2P* m_h2p_ptr;

#ifdef ALT_SIZE_8KB
  TAGE_PREDICTOR_8KB* alt_bp_predictor;
#endif

#ifdef ALT_SIZE_64KB
  TAGE_PREDICTOR_64KB* alt_bp_predictor;
#endif

  uint64_t last_pref_ip = 0;

  // reorder buffer, load/store queue, register file
  std::deque<ooo_model_instr> IFETCH_BUFFER;
  std::deque<ooo_model_instr> DISPATCH_BUFFER;
  std::deque<ooo_model_instr> DECODE_BUFFER;
  std::deque<ooo_model_instr> ROB;
  std::deque<ooo_model_instr> UOP_BUFFER;
  std::deque<ooo_model_instr> DECODED_INSTR_BUFFER;

  uint64_t last_instr_to_update_uop_cache = 0;

  std::deque<PACKET> UOP_CACHE_PQ;

  std::deque<uint64_t> instr_issued_pref;
  std::deque<demand_ip_info> demand_ips;
  std::deque<uint64_t> alternate_ips;
  uint64_t last_alternate_ip = 0;
  uint64_t last_demand_ip = 0;

  ooo_model_instr last_branch_instr;

  uint64_t last_main_ip = 0;

  std::vector<std::optional<LSQ_ENTRY>> LQ;
  std::deque<LSQ_ENTRY> SQ;

  std::array<std::vector<std::reference_wrapper<ooo_model_instr>>, std::numeric_limits<uint8_t>::max() + 1> reg_producers;

  // Constants
  const std::size_t IFETCH_BUFFER_SIZE, DISPATCH_BUFFER_SIZE, DECODE_BUFFER_SIZE, ROB_SIZE, SQ_SIZE, LQ_SIZE;
  const long int FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH;
  const long int LQ_WIDTH, SQ_WIDTH;
  const long int RETIRE_WIDTH;
  const unsigned BRANCH_MISPREDICT_PENALTY, DISPATCH_LATENCY, DECODE_LATENCY, SCHEDULING_LATENCY, EXEC_LATENCY;
  const long int L1I_BANDWIDTH, L1D_BANDWIDTH;

  ooo_model_instr mispred_instruction;

  uint64_t checks = 0;
  uint64_t hits = 0;

  BTB_Priority priority_this_cycle = NO_PRIORITY;
  bool alt_got_priority = false;
  bool conflict_banks[BTB_BANKS];
  bool demand_banks[BTB_BANKS];
  bool alternate_banks[BTB_BANKS];

  // energy stats
  uint64_t num_ifetch = 0;
  uint64_t num_decode = 0;
  uint64_t num_dispatch = 0;
  uint64_t num_rob = 0;
  uint64_t num_rob_wp = 0;
  uint64_t num_rob_wp_info = 0;
  uint64_t dib_accesses = 0;
  uint64_t dib_hits = 0;
  uint64_t num_dib_checked = 0;
  uint64_t begin_sim_cycle = 0;
  uint64_t begin_sim_instr = 0;
  uint64_t last_sim_cycle = 0;
  uint64_t last_sim_instr = 0;
  uint64_t finish_sim_cycle = 0;
  uint64_t finish_sim_instr = 0;
  uint64_t addr_gen_events = 0;
  uint64_t src_regs = 0;
  uint64_t dst_regs = 0;
  uint64_t rob_writes = 0;
  uint64_t lq_writes = 0;
  uint64_t lq_searches = 0;
  uint64_t sq_writes = 0;
  uint64_t sq_searches = 0;

  uint64_t right_path_accesses = 0;
  uint64_t right_path_size = 0;
  uint64_t last_l1i_addr = 0;

  // branch
  uint8_t fetch_stall = 0;
  uint64_t fetch_resume_cycle = 0;

  uint64_t last_br_miss_instr_id = 0;
  uint64_t last_h2p_instr_id = 0;
  uint64_t last_pref_id = 0;

  uint64_t last_alt_pref_by = 0;

  uint64_t next_ip = 0;
  uint64_t last_bb_size = 0;
  uint64_t last_global_hist = 0;

  uint64_t last_br_to_resolve = 0;
  uint64_t last_br_miss = 0;

  ooo_model_instr last_instr;

  deque<uint64_t> acc_buffer_prefetch;
  deque<uint64_t> acc_buffer;

  bool last_instr_was_two_byte = false;

  bool bp_miss = false;
  bool begin_branch_recovery = false;
  bool begin_critial_path = false;
  bool start_num_br_correctly_pred = false;
  bool after_conditional_miss = false;
  bool critical_branch_taken = false;
  uint64_t critical_branch_ip = 0;
  bool ideal_br_in_process = false;
  uint8_t last_br_miss_type = 0;
  ooo_model_instr last_br_miss_instr;
  bool begin_check = false;

  int stats_num_br = 0;

  uint64_t alternate_instr_id = 0;

  bool start_stats = false;
  int stats_bin = 0;
  int miss_bins = 0;

  uint64_t last_ftq_head_id = 0;

  uint64_t total_critical_instruction = 0;
  uint64_t total_branches_saved = 0;
  uint64_t total_br_on_critial_path = 0;
  uint64_t uop_cache_windows_recovered = 0;
  uint64_t last_recovered_ip = 0;
  bool branch_miss_front_end_stall = false;
  bool mark_next_instr = false;
  uint64_t num_windows = 0;
  bool begin_num_windows = false;
  uint64_t curr_num_windows = 0;
  uint64_t last_ip = 0;
  bool last_br_taken = false;

  bool branch_recovery_in_process = false;
  uint64_t recovery_branch_ip = 0;
  uint64_t recovery_branch_instr_id = 0;

  uint64_t last_pref_decoded = 0;
  uint64_t last_decode_ip = 0;

  uint64_t disptatched_this_cycle = 0;
  uint64_t last_dispatched_id = 0;

  bool allow_pref_to_use_decoders_wp = false;
  bool allow_pref_to_use_decoders_cp = false;

  std::deque<uint64_t> demand_decode_this_cycle;

  uint64_t ips_checked_mrc = 0;

  uint64_t instr_till_first_uop_miss = 0;
  bool track_till_first_miss = false;

  struct ip_pref_entry {
    uint64_t ip = 0;
    uint64_t by_isntr = 0;
    uint64_t by_ip = 0;
    bool nested_pref = false;
    bool br_already_resolved = false;
    uint64_t alt_check_addr = 0;
    uint64_t main_check_addr = 0;
    bool by_br_miss = false;
    bool btb_check_done = false;
    bool bp_check_done = false;
    bool ready_to_prefetch = false;
    bool was_taken = false;
    bool in_mshr = false;
    bool dib_check_done = false;
    bool dib_hit = false;
    bool btb_checked = false;
    bool is_branch = false;
    bool is_taken = false;
    uint8_t br_type = 0;
    uint64_t btb_check_cycle = 0;
    uint64_t last_cycle_to_try_btb = 0;
    int probability = 0;
    bool is_head = false;

    // ADDED THIS:
    bool is_alternate_path = false;  // Track if this is alternate path prefetch
  };
  bool dib_port_used = false;
  std::deque<uint64_t> recently_prefetched_ips;

  std::deque<ip_pref_entry> ips_to_prefetch;
  std::vector<std::pair<uint64_t, uint64_t>> spec_ips_to_prefetch;
  std::map<uint64_t, bool> ideal_ips_to_prefetch;
  // std::deque<uint64_t> ideal_ips_to_prefetch;

  std::map<uint64_t, std::deque<uint64_t>> return_map;
  std::map<uint64_t, uint64_t> indirect_map;

  FETCH_MODE fetch_mode = STREAM;
  PATH_MODE path_mode = RIGHT;
  std::deque<prefetch_decode_entry> PRF_DECODE_BUFFER;

  bool keep_adding_to_uop = false;
  uint64_t spec_main_addrs = 0;

  uint64_t last_mispredicted_target = 0;
  uint64_t last_mispredicted_target_at_mrc_lookup = 0;
  bool mrc_target_hit = false;
  
  uint64_t instr_added_to_mcr = 0;
  std::deque<uint64_t> from_mrc;

  bool has_h2p = false;
  uint64_t h2p_bb_distance = 0;
  bool alt_path_has_indirect = false;

  bool start_test = false;
  uint64_t window_test = 0;
  uint64_t last_pc_test = 0;

  struct alt_br_info {
    uint64_t br_ip = 0;
    bool prediction = false;
    bool checked = false;
    int br_type = 0;
    uint64_t instr_id = 0;
    uint64_t by_id = 0;
    uint64_t predicted_branch_target = 0;
    bool valid = false;
    bool started_correctly = false;
  };

  std::map<uint64_t, alt_br_info> alt_branch;
  // std::deque<alt_br_info> alt_branch;

  uint64_t last_critical_branch = 0;

  const long IN_QUEUE_SIZE = 2 * FETCH_WIDTH;
  std::deque<ooo_model_instr> input_queue;

  std::map<uint64_t, std::pair<uint64_t, uint8_t>> h2p_not_taken_bb_size;
  std::map<uint64_t, std::pair<uint64_t, uint8_t>> h2p_taken_bb_size;

  std::map<uint64_t, bool> h2p_map;

  struct pref_br_info {
    bool is_branch = false;
    uint8_t branch_type = 0;
    bool br_taken = false;
  };

  std::map<uint64_t, pref_br_info> br_info_prefetch_path;

  CacheBus L1I_bus, L1D_bus;

  void initialize() override final;
  void operate() override final;
  void begin_phase() override final;
  void end_phase(unsigned cpu) override final;

  void initialize_instruction();
  void check_dib();
  void translate_fetch();
  void fetch_instruction();
  void do_tag_check();
  void promote_to_decode();
  void decode_instruction();
  bool dispatch_instruction(ooo_model_instr& instr);
  void schedule_instruction();
  void execute_instruction();
  void schedule_memory_instruction();
  void operate_lsq();
  void complete_inflight_instruction();
  void handle_memory_return();
  void retire_rob();
  void update_dib_from_prefetch();
  void stream_instruction();
  void promote_to_dispatch_from_uop_cache();
  void promote_to_dispatch_from_decoders();
  BTB_Priority determinePriority(uint64_t alt_start_ip, uint64_t demand_start_ip);

  bool add_uop_cache_pq(uint64_t ip);
  void operate_uop_cache_pq();
  void decode_prefetched_instruction();
  void increment_alternate_path();
  void check_contention();
  void check_banking_collision();

  std::pair<uint64_t, uint8_t> prefetch_alternate_path(uint64_t starting_ip, uint64_t target, uint64_t bb_size, uint8_t branch_type, uint64_t by_instr,
                                                       bool branch_miss, ooo_model_instr& issuing_instr);
  std::pair<uint64_t, uint8_t> prefetch_nested_alternate_path(uint64_t starting_ip, uint64_t target, uint64_t bb_size, uint8_t branch_type, uint64_t by_instr,
                                                              bool branch_miss);

  void check_squash();
  void do_squash(uint64_t instr_id, uint64_t ip);

  void mark_at_uop_cache_hit(ooo_model_instr& instr);

  bool is_h2p_branch_hp(uint64_t ip);
  bool is_h2p_branch_table(uint64_t ip);
  void add_to_uop_queue_ip(uint64_t ip_to_pref, uint64_t by_instr, bool reached_h2p, bool branch_miss, bool nested_pref, uint64_t by_ip, bool was_taken,
                           bool is_br, bool br_taken, uint8_t pref_br_type);
  void add_to_uop_queue(uint64_t target, uint64_t bb_size, uint64_t by_instr, bool reached_h2p, bool branch_miss, bool nested_pref, uint64_t by_ip,
                        bool by_taken);
  void init_wrong_path_instruction();
  void init_random_instruction(ooo_model_instr& instr);
  void do_init_instruction(ooo_model_instr& instr);
  void do_check_dib(ooo_model_instr& instr);
  bool do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end);
  void do_dib_update(ooo_model_instr& instr, bool is_prefetch);
  void update_dib_from_prefetch(uint64_t pref_line);
  void prefetch_to_uop_cache_ideal(uint64_t starting_ip, uint64_t num_windows, uint64_t by_ip);
  void operate_on_uop_mshr_queue();
  void operate_on_alternate_path();
  void add_to_uop_cache_mshr();
  std::vector<uint64_t> bb_to_cachelines(std::vector<uint64_t> bb);
  void update_mshr(ooo_model_instr& instr);
  void stop_previous_alternate_path(ooo_model_instr& instr);
  void save_h2p_info(ooo_model_instr& instr, std::vector<std::pair<uint64_t, uint64_t>> bb, uint64_t by_ip, bool br_taken, bool taken_path);
  void do_scheduling(ooo_model_instr& instr);
  void do_execution(ooo_model_instr& rob_it);
  void do_memory_scheduling(ooo_model_instr& instr);
  void do_complete_execution(ooo_model_instr& instr);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void do_finish_store(const LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);

  uint64_t roi_instr() const { return roi_stats.back().instrs(); }
  uint64_t roi_cycle() const { return roi_stats.back().cycles(); }
  uint64_t sim_instr() const { return num_retired - begin_phase_instr; }
  uint64_t sim_cycle() const { return current_cycle - sim_stats.back().begin_cycles; }

  void print_deadlock() override final;

#include "ooo_cpu_modules.inc"

  const std::bitset<NUM_BRANCH_MODULES> bpred_type;
  const std::bitset<NUM_BTB_MODULES> btb_type;

  O3_CPU(uint32_t index, double freq_scale, dib_type&& dib, std::size_t dib_set, std::size_t dib_way, std::size_t dib_window_size,
         std::size_t ifetch_buffer_size, std::size_t decode_buffer_size, std::size_t dispatch_buffer_size, std::size_t rob_size, std::size_t lq_size,
         std::size_t sq_size, unsigned fetch_width, unsigned decode_width, unsigned dispatch_width, unsigned schedule_width, unsigned execute_width,
         long int lq_width, long int sq_width, unsigned retire_width, unsigned mispredict_penalty, unsigned decode_latency, unsigned dispatch_latency,
         unsigned schedule_latency, unsigned execute_latency, MemoryRequestConsumer* l1i, long int l1i_bw, MemoryRequestConsumer* l1d, long int l1d_bw,
         std::bitset<NUM_BRANCH_MODULES> bpred, std::bitset<NUM_BTB_MODULES> btb)
      : champsim::operable(freq_scale), cpu(index), DIB{std::move(dib)}, dib_set(dib_set), dib_way(dib_way), dib_window(dib_window_size), LQ(lq_size),
        IFETCH_BUFFER_SIZE(ifetch_buffer_size), DISPATCH_BUFFER_SIZE(dispatch_buffer_size), DECODE_BUFFER_SIZE(decode_buffer_size), ROB_SIZE(rob_size),
        LQ_SIZE(lq_size), SQ_SIZE(sq_size), FETCH_WIDTH(fetch_width), DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width),
        SCHEDULER_SIZE(schedule_width), EXEC_WIDTH(execute_width), LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width),
        BRANCH_MISPREDICT_PENALTY(mispredict_penalty), DISPATCH_LATENCY(dispatch_latency), DECODE_LATENCY(decode_latency), SCHEDULING_LATENCY(schedule_latency),
        EXEC_LATENCY(execute_latency), L1I_BANDWIDTH(l1i_bw), L1D_BANDWIDTH(l1d_bw), L1I_bus(cpu, l1i), L1D_bus(cpu, l1d), bpred_type(bpred), btb_type(btb)
  {
    m_h2p_ptr = new H2P();

#ifdef ALT_SIZE_8KB
    alt_bp_predictor = new TAGE_PREDICTOR_8KB();
#endif
#ifdef ALT_SIZE_64KB
    alt_bp_predictor = new TAGE_PREDICTOR_64KB();
#endif

    h2p_tt.initialize();
    get_profiler_ptr->cpu = cpu;
    get_profiler_ptr->freq_scale = freq_scale;
    get_profiler_ptr->ifetch_buffer_size = ifetch_buffer_size;
    get_profiler_ptr->decode_buffer_size = decode_buffer_size;
    get_profiler_ptr->dispatch_buffer_size = dispatch_buffer_size;
    get_profiler_ptr->rob_size = rob_size;
    get_profiler_ptr->lq_size = lq_size;
    get_profiler_ptr->sq_size = sq_size;
    get_profiler_ptr->fetch_width = fetch_width;
    get_profiler_ptr->decode_width = decode_width;
    get_profiler_ptr->dispatch_width = dispatch_width;
    get_profiler_ptr->schedule_width = schedule_width;
    get_profiler_ptr->execute_width = execute_width;
    get_profiler_ptr->lq_width = lq_width;
    get_profiler_ptr->sq_width = sq_width;
    get_profiler_ptr->retire_width = retire_width;
    get_profiler_ptr->mispredict_penalty = mispredict_penalty;
    get_profiler_ptr->decode_latency = decode_latency;
    get_profiler_ptr->dispatch_latency = dispatch_latency;
    get_profiler_ptr->schedule_latency = schedule_latency;
    get_profiler_ptr->execute_latency = execute_latency;
  }
};

#endif