#ifndef PROFILER_H
#define PROFILER_H

#include <fstream>
#include <string.h>
#include <string>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "defines.h"
#include "instruction.h"
#include "json.h"
#include "memory_class.h"

using namespace std;
using json = nlohmann::json;

// STATS
#define STATS_TABLE_INDEX_BITS 16
#define STATS_TABLE_ENTRIES (1 << STATS_TABLE_INDEX_BITS)
#define STATS_TABLE_MASK (STATS_TABLE_ENTRIES - 1)

typedef struct __stats_entry {
  uint64_t accesses;
  uint64_t misses;
  uint64_t hits;
  uint64_t late;
  uint64_t early; // early
} stats_entry;

typedef struct __decode_stats {
  uint64_t demand_decode;
  uint64_t prefetch_decode;
} decode_stats;

typedef struct __eviction_stats {
  uint64_t by_demand;
  uint64_t by_pref_not_used;
  uint64_t by_pref_used;
} eviction_stats;

typedef struct __lines_prefetched {
  uint64_t line_pref_by_correct_alt = 0;
  uint64_t number_of_wrong_alt_paths = 0;
  uint64_t number_of_correct_alt_paths = 0;
  uint64_t line_pref_by_wrong_alt = 0;
} lines_prefetched;

constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{
    {std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP}, std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT}, std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
     std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL}, std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL}, std::pair{"BRANCH_RETURN", BRANCH_RETURN}}};

typedef struct __uop_stats_entry {
  uint64_t ip_prefetched;
  uint64_t ip_prefetched_timely;
  uint64_t ip_prefetched_late;
  uint64_t ip_prefetched_early;
  uint64_t window_prefetched;
  uint64_t misses;
  uint64_t hits;
  uint64_t late;
  uint64_t wrong;
  uint64_t ip_correctly_predicted;
  uint64_t early_pref;
  uint64_t timely;
  uint64_t reuse_on_correct_br;
  uint64_t hits_from_pref;
  uint64_t after_br_dib_check;
  uint64_t after_br_dib_hit;
} uop_stats_entry;

// window granularity
typedef struct __uop_cache_prefetch_stats {
  uint64_t total_prefetched;
  uint64_t late_prefetches;
  uint64_t wrong_prefetches;
  uint64_t earl_prefetches;
  uint64_t timely_prefetches;
} uop_cache_prefetch_stats;

typedef struct __alt_path_stop {
  uint64_t no_target_other = 0;
  uint64_t no_target_ind = 0;
  uint64_t ind_branch = 0;
  uint64_t max_ip = 0;
  uint64_t btb_miss = 0;
  uint64_t max_br = 0;
  uint64_t stop_at_sat_counter = 0;
  uint64_t max_br_allowed = 0;
  uint64_t max_ip_limit;
} alt_path_stop;

typedef struct __correct_alt_path_stop {
  uint64_t bp_wrong = 0;
  uint64_t btb_wrong = 0;
  uint64_t was_ind = 0;
} correct_alt_path_stop;

typedef struct __h2p_predictor_stats {
  uint64_t total_conditional_misses = 0;
  uint64_t total_marked_as_h2p = 0;
  uint64_t total_h2p_marked_correctly = 0;
} h2p_predictor;

typedef struct __event_entry {
  bool uop_hit = false;
  uint64_t uop_check_cycle = 0;
  uint64_t pref_receive_cycle = 0;
  uint64_t pref_issued_cycle = 0;
} event_entry;

typedef struct __path_info {
  std::deque<std::pair<uint64_t, event_entry>> ips;
  bool is_valid = false;
  uint64_t total_taken_br = 0;
} path_info;

typedef struct __conflict_info {
  uint64_t total_btb_conflict_check = 0;
  uint64_t conflict_btb = 0;
  uint64_t total_window_conflict_check = 0;
  uint64_t conflict_window = 0;
  uint64_t btb_check_cycle = 0;
  uint64_t dib_check_cycle = 0;
} conflict_info;

typedef struct __alt_path_stats {
  uint64_t max_br;
  uint64_t no_bb;
  uint64_t no_target;
  uint64_t total_prefs;
  uint64_t no_bb_indirect;
  uint64_t no_bb_return;
  uint64_t no_btb_indirect;
  uint64_t no_btb_return;

} alt_path_stats;

typedef struct __hpca_23_stats {
  uint64_t ftq_head_stalled;
  uint64_t total_pref_on_wrong_h2p_path;
  uint64_t wrong_uop_pref_used;
  uint64_t total_wrong_pref;
  uint64_t br_miss_used;
  uint64_t br_miss_not_used;
  uint64_t late_prefs;
  uint64_t total_h2p_tried;
  uint64_t unable_to_start_pref;
  uint64_t total_h2p_miss;
  uint64_t critical_checks;
  uint64_t critical_hits;
  uint64_t total_time_in_ftq = 0;
} hpca_stats;

typedef struct __bp_stats {
  uint64_t total_preditions;
  uint64_t total_taken;
  uint64_t total_not_taken;
  uint64_t mispredictions;
  uint64_t btb_miss;
  uint64_t btb_prediction;
  uint64_t bp_miss;
  uint64_t bp_prediction;

  std::array<long long, 8> misses = {};
  std::array<long long, 8> main_path_misses = {};

  std::array<long long, 8> misses_at_exec = {};
  std::array<long long, 8> avg_resolve_time = {};
  std::array<long long, 8> branch_occurance = {};

  uint64_t total_alt_predictions = 0;
  uint64_t alt_misses = 0;

  uint64_t total_cond_pred = 0;
  uint64_t total_cond_miss = 0;

  uint64_t alt_miss_type_bim = 0;
  uint64_t alt_miss_type_tage = 0;
  uint64_t alt_miss_type_others = 0;

  uint64_t level_one_predictions = 0;
  uint64_t level_one_misses = 0;

  uint64_t alt_btb_predictions = 0;
  uint64_t alt_btb_predicted_correctly = 0;
  uint64_t alt_btb_same_as_main_path = 0;

  uint64_t alt_path_btb_miss = 0;
  uint64_t alt_path_bp_miss = 0;

} bp_stats;

typedef struct __miss_type {
  uint64_t target_direct;
  uint64_t target_indirect;
  uint64_t target_return;
  uint64_t condition_miss;
} branch_miss;

typedef struct __uop_pref {
  uint64_t total_lines_prefetched;
  uint64_t total_ips_prefetched;
  uint64_t total_pref_ips;
  uint64_t pref_hit;
} uop_pref_stats;

typedef struct __energy_stats_detailed {

  uint64_t dib_check = 0;
  uint64_t dib_hits = 0;

  uint64_t addr_gen_events = 0;
  uint64_t addr_gen_events_alt = 0;
  uint64_t rf_reads = 0;
  uint64_t rf_writes = 0;
  uint64_t rob_writes = 0;
  uint64_t lq_writes = 0;
  uint64_t lq_searches = 0;
  uint64_t sq_writes = 0;
  uint64_t sq_searches = 0;

  uint64_t l1i_load_hit = 0;
  uint64_t l1i_total_miss = 0;
  uint64_t l1i_rfo_hit = 0;
  uint64_t l1i_pref_hit = 0;

  uint64_t l1d_load_hit = 0;
  uint64_t l1d_total_miss = 0;
  uint64_t l1d_rfo_hit = 0;
  uint64_t l1d_pref_hit = 0;

  uint64_t l2c_load_hit = 0;
  uint64_t l2c_total_miss = 0;
  uint64_t l2c_rfo_hit = 0;
  uint64_t l2c_pref_hit = 0;

  uint64_t llc_load_hit = 0;
  uint64_t llc_total_miss = 0;
  uint64_t llc_rfo_hit = 0;
  uint64_t llc_pref_hit = 0;

} energy_stats_detailed;

typedef struct __energy_stats {
  uint64_t fetched_from_l1i;
  uint64_t fetched_from_uop_cache;
  uint64_t decoded;
  uint64_t pref_decoded;
} energy_stats;

typedef struct __branch_Stats {
  uint64_t total_branches;
  uint64_t marked_critical;
  uint64_t miss;
  uint64_t miss_decode;
  uint64_t miss_execute;
  uint64_t critical_miss;
  uint64_t direct_jump;
  uint64_t indirect;
  uint64_t conditional;
  uint64_t direct_call;
  uint64_t indirect_call;
  uint64_t branch_return;
  uint64_t others;
  uint64_t btb_miss;
  uint64_t bp_miss;
  uint64_t btb_predictions;
  uint64_t h2p_btb_misses;
} branch_stats;

typedef struct __uop_hit_stalls {
  uint64_t uop_queue_full;
  uint64_t decode_full;
  uint64_t rob_full;
  uint64_t dispatch_full;
  uint64_t uop_hit_followed_by_l1i_miss;
  uint64_t avg_cycle_in_ftq;
  uint64_t build_path_stall;
} uop_hit_stalls;

typedef struct __map_data {
  uint64_t reads;
  uint64_t hits;
} map_data;

typedef struct __stalls {
  uint64_t ftq_full;
  uint64_t decode_full;
  uint64_t dispatch_full;
  uint64_t switch_stalls;
  uint64_t pq_full_i;
  uint64_t mshr_full_i;
  uint64_t rq_full_i;
  uint64_t vapq_full_i;
  uint64_t uq_full_i;
  uint64_t puq_full_i;
  uint64_t wq_full_i;
  uint64_t predecoder_full;
  uint64_t uop_queue_full;
  uint64_t lq_full;
  uint64_t sq_full;
  uint64_t rob_full;
  uint64_t empty_dispatch;
  uint64_t front_end_empty;
  uint64_t back_end_full;
} stalls;

class Profiler
{
public:
  Profiler() {}

  void add_to_stats(string a1, string a2, uint64_t val)
  {

    a2.erase(std::remove_if(a2.begin(), a2.end(), ::isspace), a2.end());
    js[a1][a2] = val;
  }

  void set_stats_file_name(string filename, string exec_name)
  {
    full_file_name = filename;
    cout << "Generating Output file in: " << full_file_name << endl;
    exe_name = exec_name;
  }

  void open_stat_file(string filename) { stats.open(filename); }

  void print_json_stats_file()
  {
    this->open_stat_file(full_file_name);
    this->print_system_config();
    this->print_stats();
    stats << setw(4) << js << endl;
    stats.close();
  }

  static bool compare(const std::pair<long long int, long long int>& a, const std::pair<long long int, long long int>& b) { return a.second > b.second; }

  void print_system_config()
  {
    js["core_config"]["exec"] = exe_name;
    js["core_config"]["cpu"] = cpu;
    js["core_config"]["freq_scale"] = freq_scale;
    js["core_config"]["ifetch_buffer_size"] = ifetch_buffer_size;
    js["core_config"]["decode_buffer_size"] = decode_buffer_size;
    js["core_config"]["dispatch_buffer_size"] = dispatch_buffer_size;
    js["core_config"]["rob_size"] = rob_size;
    js["core_config"]["lq_size"] = lq_size;
    js["core_config"]["sq_size"] = sq_size;
    js["core_config"]["fetch_width"] = fetch_width;
    js["core_config"]["decode_width"] = decode_width;
    js["core_config"]["dispatch_width"] = dispatch_width;
    js["core_config"]["schedule_width"] = schedule_width;
    js["core_config"]["execute_width"] = execute_width;
    js["core_config"]["lq_width"] = lq_width;
    js["core_config"]["sq_width"] = sq_width;
    js["core_config"]["retire_width"] = retire_width;
    js["core_config"]["mispredict_penalty"] = mispredict_penalty;
    js["core_config"]["decode_latency"] = decode_latency;
    js["core_config"]["dispatch_latency"] = dispatch_latency;
    js["core_config"]["schedule_latency"] = schedule_latency;
    js["core_config"]["execute_latency"] = execute_latency;

    js["core_config"]["RQ_SIZE"] = m_RQ_SIZE;
    js["core_config"]["PQ_SIZE"] = m_PQ_SIZE;
    js["core_config"]["WQ_SIZE"] = m_WQ_SIZE;
    js["core_config"]["PTWQ_SIZE"] = m_PTWQ_SIZE;

    js["L1D"]["sets"] = l1cd_sets;
    js["L1D"]["ways"] = l1cd_ways;

    js["L1I"]["sets"] = l1ci_sets;
    js["L1I"]["ways"] = l1ci_ways;
    js["L1I"]["MSHR"] = l1ci_mshr;

    std::stringstream runtime_config;
    js["configs"]["runtime_config"] = CONFIGURATION;
  }

  void print_stats()
  {
    js["cpu_stats"]["total_retired_instructions"] = m_total_retired;
    js["cpu_stats"]["ipc"] = ipc;
    js["cpu_stats"]["loads"] = loads;
    js["cpu_stats"]["stores"] = stores;
    js["cpu_stats"]["page_faults"] = total_page_faults;

    double hit, miss = 0.0;

    hit = (double(uop_cache_hit) / double(uop_cache_read) * 100.00);
    double hit_old = (double(uop_cache_hit_old) / double(uop_cache_read) * 100.00);
    miss = 100.00 - hit;

    auto hit_critical = (double(critical_hits) / double(critical_reads) * 100.00);

    for (const auto& pair : branch_map) {
      uint64_t value = pair.second;
      if (value == 1) {
        ++count1;
      } else if (value >= 2 && value <= 1000) {
        ++count1k;
      } else if (value >= 1001 && value <= 10000) {
        ++count10k;
      } else if (value >= 10001 && value <= 100000) {
        ++count100k;
      } else {
        ++count100kPlus;
      }
    }

    std::vector<std::pair<long long int, long long int>> vec;

    for (const auto& pair : branch_map) {
      vec.push_back(pair);
    }
    std::sort(vec.begin(), vec.end(), compare);
    int count = 0;
    // for (const auto& pair : vec) {
    //   if (count >= 5) {
    //     break;
    //   }
    //   std::cout << std::hex << pair.first << std::dec << ": " << pair.second << std::endl;
    //   count++;
    // }

    std::vector<std::pair<long long int, long long int>> h2p_vec;
    for (const auto& pair : h2p_map) {
      h2p_vec.push_back(pair);
    }
    std::sort(h2p_vec.begin(), h2p_vec.end(), compare);
    count = 0;
    for (const auto& pair : h2p_vec) {
      if (count >= 5) {
        break;
      }
      std::cout << "H2P: " << count << " " << std::hex << pair.first << std::dec << ": " << pair.second << std::endl;
      string ss = "h2p_" + to_string(count + 1);
      js["branch_stats"][ss] = (double(pair.second) / double(stats_branch.miss) * 100.00);
      count++;
    }

    std::vector<std::pair<long long int, long long int>> call_vec;
    for (const auto& pair : call_map) {
      call_vec.push_back(pair);
    }
    std::sort(call_vec.begin(), call_vec.end(), compare);
    count = 0;
    // for (const auto& pair : call_vec) {
    //   if (count >= 5) {
    //     break;
    //   }
    //   std::cout << "call_" << count << ": " << std::hex << pair.first << std::dec << ": " << pair.second << std::endl;
    //   string ss_count = "call_" + to_string(count + 1);
    //   string ss_ip = "call_" + to_string(count + 1) + "_ip";
    //   string ss_freq = "call_" + to_string(count + 1) + "_freq";
    //   js["branch_stats"][ss_ip] = (double(pair.first));
    //   js["branch_stats"][ss_freq] = (double(pair.second));
    //   ss_count = ss_count + "_per";
    //   js["branch_stats"][ss_count] = (double(pair.second) / double(m_total_retired) * 100.00);
    //   count++;
    // }

    // for (const auto& pair : avg_instr_btwn_missbranch)
    //   std::cerr << "num_instr: " << pair.first << " occurrence: " << pair.second << endl;

    uint64_t total_branches_map = count1 + count1k + count10k + count100k + count100kPlus;

    js["branch_stats"]["num_h2p_branches"] = h2p_map.size();
    js["branch_stats"]["branch_occur_once"] = (double(count1) / double(total_branches_map) * 100.00);
    js["branch_stats"]["branch_occur_1k"] = (double(count1k) / double(total_branches_map) * 100.00);
    js["branch_stats"]["branch_occur_10k"] = (double(count10k) / double(total_branches_map) * 100.00);
    js["branch_stats"]["branch_occur_100k"] = (double(count100k) / double(total_branches_map) * 100.00);
    js["branch_stats"]["branch_occur_100k_plus"] = (double(count100kPlus) / double(total_branches_map) * 100.00);

    js["branch_stats"]["total_branch_mispredictions"] = stats_branch.miss;
    js["branch_stats"]["critical_branches"] = stats_branch.marked_critical;
    js["branch_stats"]["critical_branches_per"] = (double(stats_branch.marked_critical) / double(stats_branch.miss) * 100.00);
    js["branch_stats"]["critical_branches_per_total"] = (double(stats_branch.marked_critical) / double(stats_branch.total_branches) * 100.00);

    js["branch_stats"]["h2p_mpki"] = ((double(stats_branch.critical_miss) / double(m_total_retired)) * double(1000));
    js["branch_stats"]["h2p_branches_that_miss_per_total_miss"] = (double(stats_branch.critical_miss) / double(stats_branch.miss) * 100.00);
    js["branch_stats"]["h2p_branches_that_miss_per_total_bp_miss"] = (double(stats_branch.critical_miss) / double(stats_branch.bp_miss) * 100.00);
    js["branch_stats"]["num_diff_h2p_ip"] = h2p_map.size();
    js["branch_stats"]["h2p_branches_that_miss"] = stats_branch.critical_miss;
    js["branch_stats"]["h2p_branches_that_miss_per_h2p"] = (double(stats_branch.critical_miss) / double(stats_branch.marked_critical) * 100.00);
    js["branch_stats"]["miss_resolved_at_decode"] = stats_branch.miss_decode;
    js["branch_stats"]["miss_resolved_at_execute"] = stats_branch.miss_execute;
    js["branch_stats"]["miss_resolved_at_decode_per"] =
        (double(stats_branch.miss_decode) / double(stats_branch.miss_decode + stats_branch.miss_execute) * 100.00);
    js["branch_stats"]["miss_resolved_at_execute_per"] =
        (double(stats_branch.miss_execute) / double(stats_branch.miss_decode + stats_branch.miss_execute) * 100.00);
    js["branch_stats"]["btb_miss"] = stats_branch.btb_miss;
    js["branch_stats"]["btb_miss_per"] = (double(stats_branch.btb_miss) / double(stats_branch.btb_predictions) * 100.00);
    js["branch_stats"]["h2p_btb_miss_per"] = (double(stats_branch.h2p_btb_misses) / double(hpca.total_h2p_tried) * 100.00);
    js["branch_stats"]["bp_miss"] = stats_branch.bp_miss;

    uint64_t total_branch_type = stats_branch.direct_jump + stats_branch.indirect + stats_branch.conditional + stats_branch.direct_call
                                 + stats_branch.indirect_call + stats_branch.branch_return + stats_branch.others;

    js["branch_type"]["direct_jump"] = stats_branch.direct_jump;
    js["branch_type"]["indirect"] = stats_branch.indirect;
    js["branch_type"]["conditional"] = stats_branch.conditional;
    js["branch_type"]["direct_call"] = stats_branch.direct_call;
    js["branch_type"]["indirect_call"] = stats_branch.indirect_call;
    js["branch_type"]["branch_return"] = stats_branch.branch_return;
    js["branch_type"]["others"] = stats_branch.others;
    js["branch_type"]["total_branches_types"] = total_branch_type;

    js["branch_type"]["branches_resolve_decode"] = stats_branch.direct_jump + stats_branch.indirect + stats_branch.direct_call + stats_branch.indirect_call
                                                   + stats_branch.branch_return + stats_branch.others;
    js["branch_type"]["branches_resolve_execute"] = stats_branch.conditional;
    js["branch_type"]["branches_resolve_decode_per"] = double((stats_branch.direct_jump + stats_branch.indirect + stats_branch.direct_call
                                                               + stats_branch.indirect_call + stats_branch.branch_return + stats_branch.others)
                                                              / double(total_branch_type) * 100.00);
    js["branch_type"]["branches_resolve_execute_per"] = double(stats_branch.conditional / double(total_branch_type) * 100.00);
    js["branch_type"]["direct_jump_per"] = double(stats_branch.direct_jump / double(total_branch_type) * 100.00);
    js["branch_type"]["indirect_per"] = double(stats_branch.indirect / double(total_branch_type) * 100.00);
    js["branch_type"]["conditional_per"] = double(stats_branch.conditional / double(total_branch_type) * 100.00);
    js["branch_type"]["direct_call_per"] = double(stats_branch.direct_call / double(total_branch_type) * 100.00);
    js["branch_type"]["indirect_call_per"] = double(stats_branch.indirect_call / double(total_branch_type) * 100.00);
    js["branch_type"]["branch_return_per"] = double(stats_branch.branch_return / double(total_branch_type) * 100.00);
    js["branch_type"]["others_per"] = double(stats_branch.others / double(total_branch_type) * 100.00);
    js["branch_type"]["call_miss"] = double(total_call_miss / double(total_calls) * 100.00);
    // for (auto& x : indirect_target_map) {
    //   cout << "Indirect/Return-IP: " << std::hex << x.first << std::dec << " t: " << x.second.size() << endl;
    //   // for (auto& y : x.second) {
    //   //   cout << "Target: " << y.first << " total_occurance: " << y.second << endl;
    //   // }
    // }

    // std::cout << "Total critical instr: " << avg_instr_saved << std::endl;
    // std::cout << "H2P Indirect Diff: " << h2p_indirect_diff_target << " " << (double(h2p_indirect_diff_target) / double(avg_instr_saved) * 100.00) << "%" <<
    // std::endl; std::cout << "H2P Return Diff: " << h2p_return_diff_target << " " << (double(h2p_return_diff_target) / double(avg_instr_saved) * 100.00) <<
    // "%" << std::endl; js["branch_stats"]["h2p_indirect_diff"] = double(h2p_indirect_diff_target) / double(avg_instr_saved) * 100.00;
    // js["branch_stats"]["h2p_return_diff"] = double(h2p_return_diff_target) / double(avg_instr_saved) * 100.00;

    // std::cout << "H2P tag not found: " << h2p_tag_not_found << " " << (double(h2p_tag_not_found) / double(stats_branch.marked_critical) * 100.00) << "%" <<
    // std::endl; std::cout << "percentage_of_h2p: " << (double(stats_branch.marked_critical) / double(stats_branch.miss) * 100.00) << std::endl;
    js["branch_stats"]["total_branches"] = stats_branch.total_branches;
    js["branch_stats"]["h2p_miss"] = double(h2p_tag_not_found) / double(stats_branch.marked_critical) * 100.00;

    js["uop_cache"]["critical_way_hit_once"] = (double(victim_critical_ways_used) / double(victim_critical_ways) * 100.00);
    // js["uop_cache"]["total_instruction_saved"] = hit_critical;
    js["uop_cache"]["average_instruction_saved"] = (double(avg_instr_saved) / double(UOP_CACHE_RECOVERY_SIZE * 4 * critically_marked_branches)) * 100.00;

    js["uop_cache"]["total_instr_saved"] = double(avg_instr_saved);
    js["uop_cache"]["total_instr_saved_per_miss"] = (double(avg_instr_saved) / double(stats_branch.miss) * 100.00);

    hit = (double(uop_cache_hit) / double(uop_cache_read) * 100.00);

    js["uop_cache"]["hit_critical_per"] = hit_critical;
    js["uop_cache"]["uop_cache_read"] = uop_cache_read;
    js["uop_cache"]["uop_cache_hit"] = uop_cache_hit;
    js["uop_cache"]["from_fetch"] = from_fetch;
    js["uop_cache"]["hit_per"] = hit;
    cout << "uop_cache_read: " << uop_cache_read << endl;
    cout << "uop_cache_hit: " << uop_cache_hit << endl;
    cout << "UOP_CACHE_HIT: " << (double(uop_cache_hit) / double(uop_cache_read) * 100.00) << endl;
    js["uop_cache"]["hit_per_old"] = hit_old;
    js["uop_cache"]["miss_per"] = miss;
    js["uop_cache"]["mpki"] = (double(uop_cache_read - uop_cache_hit) / double(m_total_retired) * 1000.00);
    js["uop_cache"]["conflict_misses"] = uop_conflict_misses;
    js["uop_cache"]["conflict_misses_per"] = (double(uop_conflict_misses) / double(uop_cache_read - uop_cache_hit) * 100.00);
    js["uop_cache"]["conflict_misses_prefetched"] = uop_conflict_misses_prefetched;
    js["uop_cache"]["conflict_misses_per_total_miss"] = (double(uop_conflict_misses_prefetched) / double(uop_cache_read - uop_cache_hit) * 100.00);
    js["uop_cache"]["conflict_misses_per_total_conflict_miss"] = (double(uop_conflict_misses_prefetched) / double(uop_conflict_misses) * 100.00);

    hit = (double(l1i_hit) / double(l1i_access) * 100.00);
    miss = 100.00 - hit;

    js["l1i_cache"]["l1i_access"] = l1i_access;
    js["l1i_cache"]["l1i_hit"] = l1i_hit;
    js["l1i_cache"]["l1i_miss"] = l1i_miss;
    js["l1i_cache"]["hit_per"] = hit;
    js["l1i_cache"]["miss_per"] = miss;
    js["l1i_cache"]["mpki"] = (double(l1i_miss) / double(m_total_retired) * 1000.00);

    cout << "L1I Hit Rate: " << miss << endl;

    js["cpu_stalls"]["ftq_full"] = cpu_stalls.ftq_full;
    js["cpu_stalls"]["decode_full"] = cpu_stalls.decode_full;
    js["cpu_stalls"]["dispatch_full"] = cpu_stalls.dispatch_full;
    js["cpu_stalls"]["uop_buffer"] = cpu_stalls.uop_queue_full;
    js["cpu_stalls"]["switch_stalls"] = cpu_stalls.switch_stalls;

    js["cpu_stalls"]["rob_full"] = cpu_stalls.rob_full;
    js["cpu_stalls"]["lq_full"] = cpu_stalls.lq_full;
    js["cpu_stalls"]["sq_full"] = cpu_stalls.sq_full;

    js["cpu_stats"]["target_direct"] = double(stats_branch_miss.target_direct) / double(m_total_retired) * 1000.00;
    js["cpu_stats"]["target_indirect"] = double(stats_branch_miss.target_indirect) / double(m_total_retired) * 1000.00;
    js["cpu_stats"]["target_return"] = double(stats_branch_miss.target_return) / double(m_total_retired) * 1000.00;
    js["cpu_stats"]["condition_miss"] = double(stats_branch_miss.condition_miss) / double(m_total_retired) * 1000.00;
    js["cpu_stats"]["type_mpki_total"] =
        double(stats_branch_miss.condition_miss + stats_branch_miss.target_return + stats_branch_miss.target_indirect + stats_branch_miss.target_direct)
        / double(m_total_retired) * 1000.00;

    js["cpu_dispatch_stalls"]["all_front_end_queue_empty_per_cycle"] = (double(cpu_stalls.empty_dispatch) / double(total_dispatch_cycle));
    js["cpu_dispatch_stalls"]["no_dispatch_due_to_empty_frontend_per_cycle"] = (double(cpu_stalls.front_end_empty) / double(total_dispatch_cycle));
    js["cpu_dispatch_stalls"]["no_dispatch_due_to_backend_full_per_cycle"] = (double(cpu_stalls.back_end_full) / double(total_dispatch_cycle));
    js["cpu_dispatch_stalls"]["total_of_back_and_front"] = cpu_stalls.back_end_full + cpu_stalls.front_end_empty;
    js["cpu_dispatch_stalls"]["check_total_dispatch_per_cycle"] =
        (double(cpu_stalls.back_end_full + cpu_stalls.front_end_empty + avg_dispatched) / double(total_dispatch_cycle));
    js["cpu_dispatch_stalls"]["no_dispatch_due_to_empty_frontend_per"] =
        (double(cpu_stalls.front_end_empty) / double(cpu_stalls.front_end_empty + cpu_stalls.back_end_full) * 100.00);
    js["cpu_dispatch_stalls"]["no_dispatch_due_to_backend_full_per"] =
        (double(cpu_stalls.back_end_full) / double(cpu_stalls.front_end_empty + cpu_stalls.back_end_full) * 100.00);
    js["cpu_dispatch_stalls"]["avg_dispatched_per_cycle"] = double(avg_dispatched) / double(total_dispatch_cycle);
    js["cpu_dispatch_stalls"]["total_dispath_width"] = double(total_dispatch_width) / double(total_dispatch_cycle);
    js["cpu_dispatch_stalls"]["front_end_stall_due_to_branch_miss_per"] = (double(branch_miss_front_end_stall) / double(front_end_stall) * 100.00);
    js["cpu_dispatch_stalls"]["branch_to_dispatch"] = double(prediction_to_dispatch) / double(m_total_retired);

    js["cpu_stalls"]["rob_full_per_cycle"] = (double(cpu_stalls.rob_full) / double(total_cycles) * 100.00);
    js["cpu_stalls"]["ftq_full_per_cycle"] = (double(cpu_stalls.ftq_full) / double(total_cycles) * 100.00);
    js["cpu_stalls"]["decode_full_per_cycle"] = (double(cpu_stalls.decode_full) / double(total_cycles) * 100.00);
    js["cpu_stalls"]["dispatch_full_per_cycle"] = (double(cpu_stalls.dispatch_full) / double(total_cycles) * 100.00);
    js["cpu_stalls"]["uop_buffer_full_per_cycle"] = (double(cpu_stalls.uop_queue_full) / double(total_cycles) * 100.00);
    js["cpu_stalls"]["switch_stalls_per_cycle"] = (double(cpu_stalls.switch_stalls) / double(total_cycles) * 100.00);

    // UOP hit stalls

    js["uop_hit_stalls"]["uop_hit_followed_by_l1i_miss"] = uop_cache_hit_stalls.uop_hit_followed_by_l1i_miss;
    js["uop_hit_stalls"]["rob_full"] = uop_cache_hit_stalls.rob_full;
    js["uop_hit_stalls"]["dispatch_full"] = uop_cache_hit_stalls.dispatch_full;
    js["uop_hit_stalls"]["uop_queue_full"] = uop_cache_hit_stalls.uop_queue_full;
    js["uop_hit_stalls"]["decode_full"] = uop_cache_hit_stalls.decode_full;
    js["uop_hit_stalls"]["build_path_stall"] = uop_cache_hit_stalls.build_path_stall;

    js["uop_hit_stalls"]["uop_hit_followed_by_l1i_miss_per"] = (double(uop_cache_hit_stalls.uop_hit_followed_by_l1i_miss) / double(uop_cache_hit) * 100.00);
    js["uop_hit_stalls"]["rob_full_per"] = (double(uop_cache_hit_stalls.rob_full) / double(uop_cache_hit) * 100.00);
    js["uop_hit_stalls"]["dispatch_full_per"] = (double(uop_cache_hit_stalls.dispatch_full) / double(uop_cache_hit) * 100.00);
    js["uop_hit_stalls"]["uop_queue_full_per"] = (double(uop_cache_hit_stalls.uop_queue_full) / double(uop_cache_hit) * 100.00);
    js["uop_hit_stalls"]["decode_full_per"] = (double(uop_cache_hit_stalls.decode_full) / double(uop_cache_hit) * 100.00);
    js["uop_hit_stalls"]["build_path_stall_per"] = (double(uop_cache_hit_stalls.build_path_stall) / double(uop_cache_hit) * 100.00);

    js["uop_hit_stalls"]["avg_cycle_in_ftq"] = double(uop_cache_hit_stalls.avg_cycle_in_ftq) / double(uop_cache_hit);

    js["cpu_stats"]["branch_mpki"] = double(stats_branch.miss) / double(m_total_retired) * 1000.00;
    js["cpu_stats"]["default_branch_mpki"] = double(branch_mpki);
    js["cpu_stats"]["avg_branch_recovery_time"] = double(avg_branch_recovery_time.first) / double(avg_branch_recovery_time.second);

    std::vector<double> main_mpkis;
    std::transform(std::begin(bp_btb_stats.misses), std::end(bp_btb_stats.misses), std::back_inserter(main_mpkis),
                   [instrs = m_total_retired](auto x) { return 1000.0 * std::ceil(x) / std::ceil(instrs); });

    for (auto [str, idx] : types) {
      js["branch_mpki"][str] = main_mpkis[idx];
    }

    std::vector<double> main_mpkis_at_exec;
    std::transform(std::begin(bp_btb_stats.misses_at_exec), std::end(bp_btb_stats.misses_at_exec), std::back_inserter(main_mpkis_at_exec),
                   [instrs = m_total_retired](auto x) { return 1000.0 * std::ceil(x) / std::ceil(instrs); });

    for (auto [str, idx] : types) {
      js["branch_mpki_at_exec"][str] = main_mpkis_at_exec[idx];
    }

    js["main_bp_btb"]["total_predictions"] = bp_btb_stats.total_preditions;
    js["main_bp_btb"]["total_miss"] = bp_btb_stats.mispredictions;
    js["main_bp_btb"]["accuracy"] = ((double(bp_btb_stats.total_preditions - bp_btb_stats.mispredictions) / double(bp_btb_stats.total_preditions)) * 100.00);
    js["main_bp_btb"]["mpki"] = ((double(bp_btb_stats.mispredictions) / double(m_total_retired)) * 1000.00);

    std::vector<std::pair<long long int, long long int>> ftq_call_vec;
    for (const auto& pair : ftq_stall_map) {
      ftq_call_vec.push_back(pair);
    }
    std::sort(ftq_call_vec.begin(), ftq_call_vec.end(), compare);
    count = 0;
    // for (const auto& pair : ftq_call_vec) {
    //   if (count >= 5) {
    //     break;
    //   }
    //   std::cout << "longest_stall_in_FTQ_head: " << count << ": " << std::hex
    //     << pair.first << std::dec << ": occurance: "
    //     << pair.second << std::endl;
    //   count++;
    // }

    // cout << "Instruction not fetched " << instr_blocked_ftq[5] << " " << double(double(instr_blocked_ftq[5]) / double(instr_checked)) * 100.0 << endl;
    // cout << "Decode full " << instr_blocked_ftq[6] << " " << double(double(instr_blocked_ftq[6]) / double(instr_checked)) * 100.0 << endl;
    // cout << "Average distance from last miss " << double(total_distance_from_last_miss) / double(instr_checked) << endl;
    // cout << "Stall_per_cycle " << double(double(total_cycle_ftq_stall) / double(total_cycles)) * 100.0 << "%" << endl;

    js["ftq_stalls"]["l1i_miss"] = double(double(instr_blocked_ftq[5]) / double(instr_checked)) * 100.0;
    js["ftq_stalls"]["decode_full"] = double(double(instr_blocked_ftq[6]) / double(instr_checked)) * 100.0;
    js["ftq_stalls"]["ftq_head_stall_100_cycle"] = double(double(instr_checked) / double(m_total_retired)) * 100.0;
    js["ftq_stalls"]["per_cycle_stalled"] = double(double(total_cycle_ftq_stall) / double(total_cycles)) * 100.0;

    // cout << "Stalled due to returns " << total_path_stalls_due_to_return << " " << total_path_stalls << " "
    //   << double(double(total_path_stalls_due_to_return) / double(total_path_stalls)) * 100.0 << endl;
    js["path_stalls"]["per_due_to_returns"] = double(double(total_path_stalls_due_to_return) / double(total_path_stalls)) * 100.0;
    js["path_stalls"]["per_due_to_conditional"] = double(double(total_path_stalls_due_to_conditional) / double(total_path_stalls)) * 100.0;
    js["path_stalls"]["per_due_to_indirect"] = double(double(total_path_stalls_due_to_indirect) / double(total_path_stalls)) * 100.0;
    js["path_stalls"]["path_not_finished"] = double(double(total_path_stalls) / double(total_pref_paths)) * 100.0;
    js["path_stalls"]["paths_have_h2p_in_them"] = path_has_h2p;
    js["path_stalls"]["paths_have_h2p_in_them_per"] = double(double(path_has_h2p) / double(total_pref_paths)) * 100.0;
    js["path_stalls"]["avg_h2p_distance"] = double(double(total_bb_distance_from_h2p) / double(path_has_h2p));
    js["path_stalls"]["avg_bb_when_h2p"] = double(double(average_bb_when_h2p) / double(path_has_h2p));
    js["path_stalls"]["average_num_of_bb"] = double(double(total_saved_bb_size) / double(total_pref_paths));
    js["path_stalls"]["average_num_of_bb_wh2p"] = double(double(total_saved_bb_size_wh2p) / double(total_pref_paths_wh2p));

    js["uop_pref_stats"]["h2p_coverage"] = double(stats_branch.critical_miss) / double(stats_branch.bp_miss) * 100.00;
    js["uop_pref_stats"]["h2p_accuracy"] = double(stats_branch.critical_miss) / double(stats_branch.marked_critical) * 100.00;
    js["uop_pref_stats"]["h2p_over_all_branches"] = double(stats_branch.marked_critical) / double(stats_branch.total_branches) * 100.00;

    cout << "-------------------H2P stats----------------------------" << endl;
    cout << "total_num_br: " << total_saved_bb_size << endl;
    cout << "total_marked_as_h2p: " << stats_branch.marked_critical << endl;
    cout << "total_h2p_miss: " << stats_branch.critical_miss << endl;
    cout << "h2p_coverage: " << double(stats_branch.critical_miss) / double(stats_branch.bp_miss) * 100.00 << endl;
    cout << "h2p_accuracy: " << double(stats_branch.critical_miss) / double(stats_branch.marked_critical) * 100.00 << endl;

    for (auto i = -4; i <= 3; i++) {
      if (tage_alt.find(i) == tage_alt.end()) {
        tage_alt[i].misses = 0;
        tage_alt[i].predictions = 0;
      }
      if (tage_hit.find(i) == tage_hit.end()) {
        tage_hit[i].misses = 0;
        tage_hit[i].predictions = 0;
      }
      if (tage_hit_u.find(i) == tage_hit_u.end()) {
        tage_hit_u[i].misses = 0;
        tage_hit_u[i].predictions = 0;
      }
    }

    for (auto i = 0; i <= 16; i++) {
      if (tage_loop.find(i) == tage_loop.end()) {
        tage_loop[i].misses = 0;
        tage_loop[i].predictions = 0;
      }
    }

    for (auto i = 0; i <= 4; i++) {
      if (tage_bimodal.find(i) == tage_bimodal.end()) {
        tage_bimodal[i].misses = 0;
        tage_bimodal[i].predictions = 0;
      }
    }

    for (auto i = 0; i <= 4; i++) {
      if (tage_bimodal_1in8.find(i) == tage_bimodal_1in8.end()) {
        tage_bimodal_1in8[i].misses = 0;
        tage_bimodal_1in8[i].predictions = 0;
      }
    }

    for (auto i = -256; i <= 256; i++) {
      if (tage_sat.find(i) == tage_sat.end()) {
        tage_sat[i].misses = 0;
        tage_sat[i].predictions = 0;
      }
    }

    // std::map<int, miss_histo_entry> tage_alt;
    // std::map<int, miss_histo_entry> tage_hit;
    // std::map<int, miss_histo_entry> tage_bimodal;
    // std::map<int, miss_histo_entry> tage_loop;
    // std::map<int, miss_histo_entry> tage_sat;

    uint64_t total_alt_prediction = 0;
    uint64_t total_hit_predictions = 0;
    uint64_t not_sat_pred = 0;
    uint64_t total_bimodal_predictions = 0;
    uint64_t total_u_preds = 0;
    uint64_t not_sat_bimodal_pred = 0;

    uint64_t total_alt_misses = 0;
    uint64_t total_hit_misses = 0;
    uint64_t not_sat_miss = 0;
    uint64_t total_bimodal_misses = 0;
    uint64_t total_u_misses = 0;
    uint64_t not_sat_bimoal_miss = 0;

    for (auto& entry : tage_hit_u) {
      total_u_preds += entry.second.predictions;
      total_u_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_alt"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0) {
        // js["miss_histogram_MPKI_tage_alt"][yout_string] = 0;
        js["miss_histogram_per_tage_hit_u"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_alt"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_hit_u"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
        // cout << "U bit " << yout_string << " " << double(double(entry.second.misses) / double(entry.second.predictions)) * 100 << endl;
      }
    }

    for (auto& entry : tage_alt) {
      total_alt_prediction += entry.second.predictions;
      total_alt_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_alt"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0) {
        // js["miss_histogram_MPKI_tage_alt"][yout_string] = 0;
        js["miss_histogram_per_tage_alt"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_alt"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_alt"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    js["h2p_accuracy"]["alt_accuracy"] = double(double(total_alt_misses) / double(total_alt_prediction)) * 100;

    for (auto& entry : tage_hit) {
      if (entry.first > -4 && entry.first < 3) {
        not_sat_pred += entry.second.predictions;
        not_sat_miss += entry.second.misses;
      }
      total_hit_predictions += entry.second.predictions;
      total_hit_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_hit"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0) {
        // js["miss_histogram_MPKI_tage_hit"][yout_string] = 0;
        js["miss_histogram_per_tage_hit"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_hit"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_hit"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    js["h2p_accuracy"]["hit_accuracy"] = double(double(not_sat_miss) / double(not_sat_pred)) * 100;

    for (auto& entry : tage_bimodal) {
      if (entry.first == 1 || entry.first == 2) {
        not_sat_bimodal_pred += entry.second.predictions;
        not_sat_bimoal_miss += entry.second.misses;
      }

      total_bimodal_predictions += entry.second.predictions;
      total_bimodal_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_bimodal"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0) {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = 0;
        js["miss_histogram_per_tage_bimodal"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_bimodal"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    uint64_t total_bimodal_predictions_1in8 = 0;
    uint64_t total_bimodal_misses_1in8 = 0;
    for (auto& entry : tage_bimodal_1in8) {
      total_bimodal_predictions_1in8 += entry.second.predictions;
      total_bimodal_misses_1in8 += entry.second.misses;
      string yout_string = to_string(entry.first);
      if (entry.second.misses == 0) {
        js["miss_histogram_per_tage_bimodal_1in8"][yout_string] = 0;
      } else {
        js["miss_histogram_per_tage_bimodal_1in8"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    uint64_t total_sat_misses = 0;
    uint64_t total_sat_prediction = 0;
    for (auto& entry : tage_sat) {
      total_sat_prediction += entry.second.predictions;
      total_sat_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_bimodal"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0 || entry.second.predictions == 0) {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = 0;
        js["miss_histogram_per_tage_sat"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_sat"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    uint64_t total_loop_misses = 0;
    uint64_t total_loop_prediction = 0;
    for (auto& entry : tage_loop) {
      total_loop_prediction += entry.second.predictions;
      total_loop_misses += entry.second.misses;
      string yout_string = to_string(entry.first);
      // js["miss_histogram_tage_bimodal"][yout_string] = entry.second.misses;
      if (entry.second.misses == 0) {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = 0;
        js["miss_histogram_per_tage_loop"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_bimodal"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_histogram_per_tage_loop"][yout_string] = double(double(entry.second.misses) / double(entry.second.predictions)) * 100;
      }
    }

    cout << "hit_misses: " << not_sat_miss << endl;
    cout << "hit_predictions: " << not_sat_pred << endl;
    cout << "hit_accuracy: " << double(double(not_sat_miss) / double(not_sat_pred)) * 100 << endl;
    cout << "bimodal_accuracy: " << double(double(not_sat_bimoal_miss) / double(not_sat_bimodal_pred)) * 100 << endl;

    js["h2p_accuracy"]["hit_accuracy"] = double(double(not_sat_miss) / double(not_sat_pred)) * 100;
    js["h2p_accuracy"]["bimodal_accuracy"] = double(double(not_sat_bimoal_miss) / double(not_sat_bimodal_pred)) * 100;
    js["h2p_accuracy"]["average_accuracy"] =
        double(double(not_sat_bimoal_miss + not_sat_miss + total_alt_misses) / double(not_sat_bimodal_pred + total_alt_prediction + not_sat_pred)) * 100;

    total_tage_prediction = total_alt_prediction + total_hit_predictions + total_bimodal_predictions + total_loop_prediction + total_sat_prediction;

    uint64_t bim_0in8_predictions = total_bimodal_predictions - total_bimodal_predictions_1in8;

    js["tage_predictor_info"]["total_tage_vs_total_branches"] = (double(total_tage_prediction) / double(bp_btb_stats.total_preditions)) * 100;
    js["tage_predictor_info"]["alt_bank_per"] = (double(total_alt_prediction) / double(total_tage_prediction)) * 100;
    js["tage_predictor_info"]["hit_bank_per"] = (double(total_hit_predictions) / double(total_tage_prediction)) * 100;
    js["tage_predictor_info"]["bimodal_0in8_per"] = (double(bim_0in8_predictions) / double(total_tage_prediction)) * 100;
    js["tage_predictor_info"]["bimodal_1in8_per"] = (double(total_bimodal_predictions_1in8) / double(total_tage_prediction)) * 100;
    js["tage_predictor_info"]["loop_per"] = (double(total_loop_prediction) / double(total_tage_prediction)) * 100;
    js["tage_predictor_info"]["stat_per"] = (double(total_sat_prediction) / double(total_tage_prediction)) * 100;

    js["tage_predictor_info"]["updated_by_SC"] = (double(changed_by_sc) / double(total_tage_prediction)) * 100;

    js["tage_predictor_info"]["alt_bank_miss_per"] = (double(total_alt_misses) / double(total_alt_prediction)) * 100;
    js["tage_predictor_info"]["hit_bank_miss_per"] = (double(total_hit_misses) / double(total_hit_predictions)) * 100;
    js["tage_predictor_info"]["bimodal_miss_per"] = (double(total_bimodal_misses) / double(total_bimodal_predictions)) * 100;
    js["tage_predictor_info"]["bimodal_1in8_miss_per"] = (double(total_bimodal_misses_1in8) / double(total_bimodal_predictions_1in8)) * 100;
    js["tage_predictor_info"]["sat_miss_per"] = (double(total_sat_misses) / double(total_sat_prediction)) * 100;
    js["tage_predictor_info"]["loop_miss_per"] = (double(total_loop_misses) / double(total_loop_prediction)) * 100;

    js["tage_predictor_info"]["alt_bank_miss_per_total_miss"] = (double(total_alt_misses) / double(stats_branch.bp_miss)) * 100;
    js["tage_predictor_info"]["hit_bank_miss_per_total_miss"] = (double(total_hit_misses) / double(stats_branch.bp_miss)) * 100;
    js["tage_predictor_info"]["bimodal_miss_per_total_miss"] = (double(total_bimodal_misses) / double(stats_branch.bp_miss)) * 100;

    js["tage_predictor_info"]["coverage_missing_due_to_bim"] = (double(coverage_miss_bim) / double(coverage_miss)) * 100;
    js["tage_predictor_info"]["coverage_missing_due_to_hit"] = (double(coverage_miss_hit) / double(coverage_miss)) * 100;
    js["tage_predictor_info"]["coverage_missing_due_to_alt"] = (double(coverage_miss_alt) / double(coverage_miss)) * 100;
    js["tage_predictor_info"]["coverage_missing_due_to_others"] = (double(coverage_miss_other) / double(coverage_miss)) * 100;

    auto total_decodes = front_end_energy.decoded + front_end_energy.pref_decoded;
    ;
    js["front_end_energy_stats"]["normal_decodes"] = front_end_energy.decoded;
    js["front_end_energy_stats"]["pref_decodes"] = front_end_energy.pref_decoded;
    js["front_end_energy_stats"]["fetched_from_l1i"] = front_end_energy.fetched_from_l1i;
    js["front_end_energy_stats"]["fetched_from_uop_cache"] = front_end_energy.fetched_from_uop_cache;
    js["front_end_energy_stats"]["total_decodes"] = total_decodes;
    js["front_end_energy_stats"]["pref_decodes_per"] = (double(front_end_energy.pref_decoded) / double(total_decodes)) * 100;
    js["front_end_energy_stats"]["normal_decodes_per"] = (double(front_end_energy.decoded) / double(total_decodes)) * 100;

    js["hpca_stats"]["pref_timely"] = (double(uop_pref_stats.timely) / double(uop_pref_stats.window_prefetched)) * 100.00;
    js["hpca_stats"]["pref_late"] = (double(uop_pref_stats.late) / double(uop_pref_stats.window_prefetched)) * 100.00;
    js["hpca_stats"]["pref_early"] = (double(uop_pref_stats.early_pref) / double(uop_pref_stats.window_prefetched)) * 100.00;

    js["hpca_stats"]["hits_from_pref"] = double(double(uop_pref_stats.hits_from_pref) / double(uop_cache_hit)) * 100.0;
    js["hpca_stats"]["ftq_head_stalled"] = hpca.ftq_head_stalled;

    js["hpca_stats"]["total_pref_on_wrong_h2p_path"] = hpca.total_pref_on_wrong_h2p_path;
    js["hpca_stats"]["pref_used_on_wrong_path"] = hpca.wrong_uop_pref_used;
    js["hpca_stats"]["per_of_use"] = (double(hpca.wrong_uop_pref_used) / double(hpca.total_pref_on_wrong_h2p_path)) * 100.00;
    js["hpca_stats"]["total_path_failedto_start"] = (double(hpca.unable_to_start_pref) / double(hpca.total_h2p_tried)) * 100.00;
    js["hpca_stats"]["h2p_that_cond_miss"] = (double(hpca.total_h2p_miss) / double(hpca.total_h2p_tried)) * 100.00;

    cout << "H2P_THAT_MISS: " << (double(hpca.total_h2p_miss) / double(hpca.total_h2p_tried)) * 100.0 << endl;

    // alt_path_with_ind_br

    js["hpca_stats"]["avg_indirect_per_alt_path"] = (double(total_ind_in_alt_path) / double(hpca.total_h2p_tried));
    // js["hpca_stats"]["alt_path_with_indirect_br"] = double(alt_path_with_ind_br);
    js["hpca_stats"]["per_alt_path_with_indirect_br"] = (double(alt_path_with_ind_br) / double(hpca.total_h2p_tried)) * 100.00;

    cout << "total alt path: " << double(hpca.total_h2p_tried) << endl;
    cout << "indirect branches on alt path: " << double(total_ind_in_alt_path) << endl;
    cout << "Average indirect branches on alt path: " << (double(total_ind_in_alt_path) / double(hpca.total_h2p_tried)) << endl;
    cout << "per of alt path with indrect br: " << (double(alt_path_with_ind_br) / double(hpca.total_h2p_tried)) * 100.00 << endl;

    js["hpca_stats"]["h2p_per_kilo_instr"] = (double(stats_branch.marked_critical) / double(m_total_retired)) * 1000.00;
    js["hpca_stats"]["correctly_predicted_ips"] = double(double(uop_pref_stats.ip_correctly_predicted) / double(uop_pref_stats.ip_prefetched)) * 100.0;

    js["hpca_stats"]["average_distance_between_two_br_misses"] = double(total_miss_distance) / double(bp_btb_stats.mispredictions);
    js["hpca_stats"]["average_distance_between_two_h2p_misses"] = double(total_h2p_distance) / double(stats_branch.marked_critical);

    js["hpca_stats"]["tracking_instr_after_miss"] = total_instr_in_window;
    js["hpca_stats"]["tracking_instr_uop_miss"] = window_instr_miss;
    js["hpca_stats"]["tracking_instr_uop_miss_per"] = (double(window_instr_miss) / double(total_instr_in_window)) * 100.00;
    // js["hpca_stats"]["tracking_instr_stall_ftq"] = window_instr_stall;
    // js["hpca_stats"]["tracking_instr_stall_ftq_per"] = (double(window_instr_stall) / double(total_instr_in_window)) * 100.00;

    js["isca_stats"]["total_alt_predictions"] = alt_bp_btb_stats.total_alt_predictions;
    js["isca_stats"]["alt_misses"] = alt_bp_btb_stats.alt_misses;
    js["isca_stats"]["alt_path_miss_rate"] = (double(alt_bp_btb_stats.alt_misses) / double(alt_bp_btb_stats.total_alt_predictions)) * 100.00;
    js["isca_stats"]["alt_misses_by_bp"] = (double(alt_bp_btb_stats.alt_path_bp_miss) / double(alt_bp_btb_stats.alt_misses)) * 100.00;
    js["isca_stats"]["alt_misses_by_btb"] = (double(alt_bp_btb_stats.alt_path_btb_miss) / double(alt_bp_btb_stats.alt_misses)) * 100.00;
    js["isca_stats"]["alt_path_accuracy"] = double(double(alt_bp_btb_stats.alt_misses) / double(alt_bp_btb_stats.total_alt_predictions)) * 100.00;

    std::vector<double> first_br_miss_mpki;
    std::transform(std::begin(alt_bp_btb_stats.misses), std::end(alt_bp_btb_stats.misses), std::back_inserter(first_br_miss_mpki),
                   [instrs = alt_bp_btb_stats.total_alt_predictions](auto x) { return 100.0 * std::ceil(x) / std::ceil(instrs); });

    cout << "Total alt predictions done: " << alt_bp_btb_stats.total_alt_predictions << endl;
    cout << "total alt misses " << alt_bp_btb_stats.alt_misses << endl;
    cout << "Accuracy on alt path  " << double(double(alt_bp_btb_stats.alt_misses) / double(alt_bp_btb_stats.total_alt_predictions)) * 100.00 << endl;
    for (auto [str, idx] : types) {
      js["isca_stats_first_br_miss_rate"][str] = first_br_miss_mpki[idx];
      cout << "isca_stats_first_br_miss_rate_" << str << ": " << first_br_miss_mpki[idx] << endl;
    }

    std::vector<double> alt_bp_on_main_path;
    std::transform(std::begin(alt_bp_btb_stats.main_path_misses), std::end(alt_bp_btb_stats.main_path_misses), std::back_inserter(alt_bp_on_main_path),
                   [instrs = m_total_retired](auto x) { return 1000.0 * std::ceil(x) / std::ceil(instrs); });

    for (auto [str, idx] : types) {
      js["isca_stats_alt_bp_on_main_path"][str] = alt_bp_on_main_path[idx];
      cout << "isca_stats_alt_bp_on_main_path_" << str << ": " << alt_bp_on_main_path[idx] << endl;
    }

    uint64_t total_alt_miss = alt_bp_btb_stats.alt_miss_type_bim + alt_bp_btb_stats.alt_miss_type_tage + alt_bp_btb_stats.alt_miss_type_others;

    cout << "============================ HPCA STATS ================================" << endl;

    cout << "FTQ_stalled: " << hpca.ftq_head_stalled << endl;
    cout << "FTQ_stalled_mpki: " << (double(hpca.ftq_head_stalled) / double(m_total_retired)) * 1000.00 << endl;
    cout << "total_pref_on_wrong_h2p_path: " << hpca.total_pref_on_wrong_h2p_path << endl;
    cout << "pref_used_on_wrong_path: " << hpca.wrong_uop_pref_used << endl;
    cout << "wrong_pref_use: " << (double(hpca.wrong_uop_pref_used) / double(hpca.total_pref_on_wrong_h2p_path)) * 100.00 << endl;
    cout << "h2p_per_kilo_instr: " << (double(stats_branch.marked_critical) / double(m_total_retired)) * 1000.00 << endl;
    cout << "miss_br_pref_used " << (hpca.br_miss_used) << endl;
    cout << "window_pre " << double(uop_pref_stats.window_prefetched) << endl;

    cout << "================================ Prefetching stats ========================" << endl;

    cout << "total_window_pref: " << double(uop_pref_stats.window_prefetched) << endl;
    cout << "pref_timely: " << (double(uop_pref_stats.timely) / double(uop_pref_stats.window_prefetched)) * 100.00 << endl;
    cout << "pref_late: " << (double(uop_pref_stats.late) / double(uop_pref_stats.window_prefetched)) * 100.00 << endl;
    cout << "pref_early: " << (double(uop_pref_stats.early_pref) / double(uop_pref_stats.window_prefetched)) * 100.00 << endl;

    cout << "Prefetching Accuracy: " << (double(uop_pref_stats.timely) / double(uop_pref_stats.window_prefetched)) * 100.00 << endl;
    cout << "Prefetching Coverage: " << double(double(uop_pref_stats.after_br_dib_hit) / double(uop_pref_stats.after_br_dib_check)) * 100.0 << endl;

    js["isca_stats"]["pref_accuracy"] = (double(uop_pref_stats.timely) / double(uop_pref_stats.window_prefetched)) * 100.00;
    js["isca_stats"]["Pref_coverage"] = double(double(uop_pref_stats.after_br_dib_hit) / double(uop_pref_stats.after_br_dib_check)) * 100.0;

    uint64_t total_prefs = uop_pref_stats.ip_correctly_predicted + uop_pref_stats.ip_prefetched_late + uop_pref_stats.ip_prefetched_early;

    cout << "Timely_prefetch_ip: " << uop_pref_stats.ip_correctly_predicted << endl;
    cout << "Late_prefetch_ip: " << uop_pref_stats.ip_prefetched_late << endl;
    cout << "Early_prefetch_ip: " << uop_pref_stats.ip_prefetched_early << endl;

    cout << "Per_Timely_prefetch_ip: " << (double(uop_pref_stats.ip_correctly_predicted) / double(total_prefs)) * 100.00 << endl;
    cout << "Per_Late_prefetch_ip: " << (double(uop_pref_stats.ip_prefetched_late) / double(total_prefs)) * 100.00 << endl;
    cout << "Per_Early_prefetch_ip: " << (double(uop_pref_stats.ip_prefetched_early) / double(total_prefs)) * 100.00 << endl;

    js["isca_stats"]["Per_Timely_prefetch_ip"] = (double(uop_pref_stats.ip_correctly_predicted) / double(total_prefs)) * 100.00;
    js["isca_stats"]["Per_Late_prefetch_ip"] = (double(uop_pref_stats.ip_prefetched_late) / double(total_prefs)) * 100.00;
    js["isca_stats"]["Per_Early_prefetch_ip"] = (double(uop_pref_stats.ip_prefetched_early) / double(total_prefs)) * 100.00;

    cout << "hits_from_pref: " << double(double(uop_pref_stats.timely) / double(uop_cache_hit)) * 100.0 << endl;

    cout << "accuracy_on_alt_path_preditions " << alt_bp_btb_stats.total_alt_predictions << endl;
    cout << "Misses on alternate path " << alt_bp_btb_stats.alt_misses << endl;
    cout << "Miss rate on alternate path " << (double(alt_bp_btb_stats.alt_misses) / double(alt_bp_btb_stats.total_alt_predictions)) * 100.00 << "%" << endl;

    cout << "Total prefetch reach a branch " << alt_br_correct.size() << endl;
    int sum = 0;

    for (const auto& entry : alt_br_correct) {
      sum += entry.second;
    }

    double average = static_cast<double>(sum) / alt_br_correct.size();
    cout << "Average number of correctly predicted branches " << average << endl;

    js["isca_stats"]["avg_branch_predicted_correctly"] = average;
    js["isca_stats"]["avg_num_br"] = double(total_saved_bb_size) / double(total_pref_paths);
    js["isca_stats"]["cond_miss_rate_of_alt_on_main_path"] = double(alt_bp_btb_stats.total_cond_miss) / double(alt_bp_btb_stats.total_cond_pred) * 100.00;

    cout << "Total branches predicted: " << total_saved_bb_size << endl;
    cout << "Total H2P generated: " << total_pref_paths << endl;
    cout << "Average_num_of_br: " << double(total_saved_bb_size) / double(total_pref_paths) << endl;

    js["isca_stats"]["average_miss_distance_after_h2p"] = double(miss_distance_from_h2p) / double(total_counts_from_h2p);
    // js["isca_stats"]["average_miss_distance_less_than_8_after_h2p"] = double(less_than_8) / double(total_counts_from_h2p) * 100.00;

    for (auto& entry : less_than_8) {
      string yout_string = to_string(entry.first);
      js["average_miss_distance"][yout_string] = double(double(entry.second) / double(total_counts_from_h2p)) * 100.00;
      cout << "less_than_ " << entry.first << " second " << entry.second << ": " << double(double(entry.second) / double(total_counts_from_h2p)) * 100.00
           << endl;
    }

    cout << "Miss distance after H2P " << double(miss_distance_from_h2p) / double(total_counts_from_h2p) << endl;
    // cout << "average_miss_distance_less_than_8_after_h2p " << double(less_than_8) / double(total_counts_from_h2p) * 100.00 << endl;

    for (auto& entry : miss_coverage_hit) {
      string yout_string = to_string(entry.first);
      if (entry.second == 0) {
        // js["miss_histogram_MPKI_tage_loop"][yout_string] = 0;
        js["miss_coverage_hit"][yout_string] = 0;
      } else {
        // js["miss_histogram_MPKI_tage_loop"][yout_string] = double(double(entry.second.misses) / double(m_total_retired)) * 1000;
        js["miss_coverage_hit"][yout_string] = double(double(entry.second) / double(coverage_miss_hit)) * 100;
        // cout << "miss_coverage_hit " << yout_string << ": " << double(double(entry.second) / double(coverage_miss_hit)) * 100 << endl;
      }
    }

    uint64_t total_hits = 0;
    uint64_t total_late = 0;
    uint64_t total_early = 0;
    uint64_t total_misses = 0;
    uint64_t total_accesses = 0;

    for (uint32_t i = 0; i < STATS_TABLE_ENTRIES; i++) {
      total_hits += stats_table[i].hits;
      total_late += stats_table[i].late;
      total_early += stats_table[i].early;
      total_misses += stats_table[i].misses;
      total_accesses += stats_table[i].accesses;
    }

    cout << "PREF_STATS_TABLE_MISSES: " << ((double)total_misses / (double)total_accesses) * 100.00 << endl;
    cout << "PREF_STATS_TABLE_MISSES_COVERAGE: " << ((double)total_hits / (double)(total_hits + total_misses)) * 100.00 << endl;
    cout << "PREF_STATS_TABLE_MISSES_ACCURACY: " << ((double)total_hits / (double)(total_hits + total_late + total_early)) * 100.00 << endl;
    cout << "PREF_STATS_TABLE_MISSES_LATE: " << ((double)total_late / (double)(total_hits + total_late + total_early)) * 100.00 << endl;
    cout << "PREF_STATS_TABLE_MISSES_EARLY: " << ((double)total_early / (double)(total_hits + total_late + total_early)) * 100.00 << endl;

    js["isca_stats"]["PREF_STATS_TABLE_MISSES"] = ((double)total_misses / (double)total_accesses) * 100.00;
    js["isca_stats"]["PREF_STATS_TABLE_MISSES_COVERAGE"] = ((double)total_hits / (double)(total_hits + total_misses)) * 100.00;
    js["isca_stats"]["PREF_STATS_TABLE_MISSES_ACCURACY"] = ((double)total_hits / (double)(total_hits + total_late + total_early)) * 100.00;
    js["isca_stats"]["PREF_STATS_TABLE_MISSES_LATE"] = ((double)total_late / (double)(total_hits + total_late + total_early)) * 100.00;
    js["isca_stats"]["PREF_STATS_TABLE_MISSES_EARLY"] = ((double)total_early / (double)(total_hits + total_late + total_early)) * 100.00;

    cout << "PERCENTAGE_OF_BTB_CONFLICTS: " << ((double)conflict_stats.conflict_btb / (double)conflict_stats.total_btb_conflict_check) * 100.00 << endl;
    cout << "PERCENTAGE_OF_UOP_CACHE_CONFLICTS: " << ((double)conflict_stats.conflict_window / (double)conflict_stats.total_window_conflict_check) * 100.00
         << endl;

    js["isca_stats"]["PERCENTAGE_OF_BTB_CONFLICTS"] = ((double)conflict_stats.conflict_btb / (double)conflict_stats.total_btb_conflict_check) * 100.00;
    js["isca_stats"]["PERCENTAGE_OF_UOP_CACHE_CONFLICTS"] =
        ((double)conflict_stats.conflict_window / (double)conflict_stats.total_window_conflict_check) * 100.00;

    auto tota_h2p_path = alt_path_stop_info.btb_miss + alt_path_stop_info.ind_branch + alt_path_stop_info.max_br + alt_path_stop_info.max_ip
                         + alt_path_stop_info.no_target_other + alt_path_stop_info.no_target_ind + alt_path_stop_info.stop_at_sat_counter
                         + alt_path_stop_info.max_ip_limit;

    cout << "alt_path_stop_btb_miss " << ((double)alt_path_stop_info.btb_miss / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_ind_branch " << ((double)alt_path_stop_info.ind_branch / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_max_br " << ((double)alt_path_stop_info.max_br / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_max_ip " << ((double)alt_path_stop_info.max_ip / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_no_target_other " << ((double)alt_path_stop_info.no_target_other / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_no_target_ind " << ((double)alt_path_stop_info.no_target_ind / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_stop_at_sat_counter " << ((double)alt_path_stop_info.stop_at_sat_counter / (double)tota_h2p_path) * 100.00 << endl;
    cout << "alt_path_stop_stop_at_ip_limit_reached " << ((double)alt_path_stop_info.max_ip_limit / (double)tota_h2p_path) * 100.00 << endl;

    cout << "tota_h2p_path " << tota_h2p_path << endl;

    js["alt_path_stop"]["btb_miss"] = ((double)alt_path_stop_info.btb_miss / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["ind_branch"] = ((double)alt_path_stop_info.ind_branch / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["max_br"] = ((double)alt_path_stop_info.max_br / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["max_ip"] = ((double)alt_path_stop_info.max_ip / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["no_target_others"] = ((double)alt_path_stop_info.no_target_other / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["no_target_ind"] = ((double)alt_path_stop_info.no_target_ind / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["stop_at_sat_counter"] = ((double)alt_path_stop_info.stop_at_sat_counter / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["max_br_allowed"] = ((double)alt_path_stop_info.max_br_allowed / (double)tota_h2p_path) * 100.00;
    js["alt_path_stop"]["ip_limit_reached"] = ((double)alt_path_stop_info.max_ip_limit / (double)tota_h2p_path) * 100.00;

    auto correct_alt_total = correct_alt_path_stop_info.bp_wrong + correct_alt_path_stop_info.btb_wrong + correct_alt_path_stop_info.was_ind;

    js["correct_alt_stop"]["bp_wrong"] = ((double)correct_alt_path_stop_info.bp_wrong / (double)correct_alt_total) * 100.00;
    js["correct_alt_stop"]["btb_wrong"] = ((double)correct_alt_path_stop_info.btb_wrong / (double)correct_alt_total) * 100.00;
    js["correct_alt_stop"]["was_ind"] = ((double)correct_alt_path_stop_info.was_ind / (double)correct_alt_total) * 100.00;

    cout << "correct_alt_stop_bp_wrong " << ((double)correct_alt_path_stop_info.bp_wrong / (double)correct_alt_total) * 100.00 << endl;
    cout << "correct_alt_stop_btb_wrong " << ((double)correct_alt_path_stop_info.btb_wrong / (double)correct_alt_total) * 100.00 << endl;
    cout << "correct_alt_stop_was_ind " << ((double)correct_alt_path_stop_info.was_ind / (double)correct_alt_total) * 100.00 << endl;

    cout << "only_cond_h2p_predictor_coverage: "
         << double(h2p_predictor_stats.total_h2p_marked_correctly) / double(h2p_predictor_stats.total_conditional_misses) * 100.0 << endl;
    cout << "only_cond_h2p_predictor_accuracy: "
         << double(h2p_predictor_stats.total_h2p_marked_correctly) / double(h2p_predictor_stats.total_marked_as_h2p) * 100.0 << endl;

    js["only_cond_h2p_predictor_stats"]["coverage"] =
        double(h2p_predictor_stats.total_h2p_marked_correctly) / double(h2p_predictor_stats.total_conditional_misses) * 100.0;
    js["only_cond_h2p_predictor_stats"]["accuracy"] =
        double(h2p_predictor_stats.total_h2p_marked_correctly) / double(h2p_predictor_stats.total_marked_as_h2p) * 100.0;

    cout << "Changed_by_SC: " << (double(changed_by_sc) / double(total_tage_prediction)) * 100 << endl;

    cout << "avg_line_by_ideal_setting " << (double(number_of_lines_by_ideal) / double(number_of_brs)) * 100 << endl;
    js["lines_pref_stats"]["avg_line_by_ideal_setting"] = (double(number_of_lines_by_ideal) / double(number_of_brs)) * 100;

    cout << "critical_uop_checks: " << hpca.critical_checks << endl;
    cout << "critical_uop_hits: " << hpca.critical_hits << endl;
    cout << "UopHit_rate_on_critial_path: " << (double(hpca.critical_hits) / double(hpca.critical_checks)) * 100 << endl;
    js["isca_stats"]["UopHit_rate_on_critial_path"] = (double(hpca.critical_hits) / double(hpca.critical_checks)) * 100;
    js["isca_stats"]["UopMiss_rate_on_critial_path"] = (double(hpca.critical_checks - hpca.critical_hits) / double(hpca.critical_checks)) * 100;

    cout << "FTQ_HEAD_CRITICAL: " << (double(hpca.ftq_head_stalled) / double(m_total_retired)) * 1000.00 << endl;
    js["isca_stats"]["ftq_head_critical"] = (double(hpca.ftq_head_stalled) / double(m_total_retired)) * 1000.00;

    cout << "SWITCH_STALLS_MPKI: " << (double(cpu_stalls.switch_stalls) / double(m_total_retired)) * 1000.00 << endl;
    js["isca_stats"]["switch_stalls_mpki"] = (double(cpu_stalls.switch_stalls) / double(m_total_retired)) * 1000.00;

    sum = 0;
    for (const auto& entry : num_of_lines_pref) {
      sum += entry.second;
    }

    cout << "pref_used_from_wrong_alt_path: " << (double(hpca.wrong_uop_pref_used) / double(hpca.total_pref_on_wrong_h2p_path)) * 100.00 << endl;
    js["lines_pref_stats"]["pref_used_from_wrong_alt_path"] = (double(hpca.wrong_uop_pref_used) / double(hpca.total_pref_on_wrong_h2p_path)) * 100.00;

    // cout << "Total " << lines_pref_stats.line_pref_by_wrong_alt + lines_pref_stats.line_pref_by_correct_alt << " total h2p " << num_of_lines_pref.size()
    //      << "sum " << sum << " wrong " << lines_pref_stats.line_pref_by_wrong_alt << " correct " << lines_pref_stats.line_pref_by_correct_alt << endl;
    cout << "Avg_cache_lines_by_wrong_h2p: " << (double(lines_pref_stats.line_pref_by_wrong_alt) / double(num_of_lines_pref.size())) << endl;
    cout << "Avg_cache_lines_by_correct_h2p: " << (double(lines_pref_stats.line_pref_by_correct_alt) / double(num_of_lines_pref.size())) << endl;

    js["lines_pref_stats"]["Avg_cache_lines_by_wrong_h2p"] = (double(lines_pref_stats.line_pref_by_wrong_alt) / double(num_of_lines_pref.size()));
    js["lines_pref_stats"]["Avg_cache_lines_by_correct_h2p"] = (double(lines_pref_stats.line_pref_by_correct_alt) / double(num_of_lines_pref.size()));

    // Ensure that there are elements in the map to avoid division by zero
    if (!num_of_lines_pref.empty()) {
      double average = static_cast<double>(sum) / num_of_lines_pref.size();
      js["isca_stats"]["AVERAGE_PREF_PER_H2P"] = average;
      cout << "AVERAGE_PREF_PER_H2P " << average << endl;
    } else {
      js["isca_stats"]["AVERAGE_PREF_PER_H2P"] = 0;
    }

    uint64_t range_100_100 = 0;
    uint64_t total = 0;
    for (int x = 0; x < 32; x++) {
      total += sets_used[x];
    }

    total = 0;
    for (int x = 0; x < 64; x++) {
      for (int y = 0; y < 8; y++) {
        total += sets_used_l1i[x][y];
      }
    }

    cout << "ENERGY STATS" << endl;
    cout << "dib_check " << detailed_energy.dib_check << endl;
    cout << "dib_hits " << detailed_energy.dib_hits << endl;
    cout << "addr_gen_events " << detailed_energy.addr_gen_events << endl;
    cout << "addr_gen_events_alt " << detailed_energy.addr_gen_events_alt << endl;
    cout << "rf_reads " << detailed_energy.rf_reads << endl;
    cout << "rf_writes " << detailed_energy.rf_writes << endl;
    cout << "rob_writes " << detailed_energy.rob_writes << endl;
    cout << "lq_writes " << detailed_energy.lq_writes << endl;
    cout << "lq_searches " << detailed_energy.lq_searches << endl;
    cout << "sq_writes " << detailed_energy.sq_writes << endl;
    cout << "sq_searches " << detailed_energy.sq_searches << endl;
    cout << endl;

    js["isca_stats_energy"]["dib_check"] = detailed_energy.dib_check;
    js["isca_stats_energy"]["dib_hits"] = detailed_energy.dib_hits;
    js["isca_stats_energy"]["addr_gen_events"] = detailed_energy.addr_gen_events;
    js["isca_stats_energy"]["addr_gen_events_alt"] = detailed_energy.addr_gen_events_alt;
    js["isca_stats_energy"]["rf_reads"] = detailed_energy.rf_reads;
    js["isca_stats_energy"]["rf_writes"] = detailed_energy.rf_writes;
    js["isca_stats_energy"]["rob_writes"] = detailed_energy.rob_writes;
    js["isca_stats_energy"]["lq_writes"] = detailed_energy.lq_writes;
    js["isca_stats_energy"]["lq_searches"] = detailed_energy.lq_searches;
    js["isca_stats_energy"]["sq_writes"] = detailed_energy.sq_writes;
    js["isca_stats_energy"]["sq_searches"] = detailed_energy.sq_searches;

    js["isca_stats"]["avg_cycle_in_ftq"] = hpca.total_time_in_ftq / m_total_retired;
    js["isca_stats"]["avg_cycle_ftq_head_stalled"] = double(avg_time_ftq_head_stalled / total_ftq_stalls);

    uint64_t max_entry_value = 0;
    uint64_t sum_of_values = 0;

    // for (const auto& entry : histogram_of_returns) {
    //   if (entry.second > max_entry_value) {
    //     max_entry_value = entry.second;
    //   }
    //   sum_of_values += entry.second;
    // }

    // // Calculate average
    // double average_t = sum_of_values / histogram_of_returns.size();

    // // Output results
    // js["isca_stats"]["max_num_return_in_alt_path"] = max_entry_value;
    // js["isca_stats"]["avg_num_of_returns_in_alt_path"] = average_t;

    // for (auto& entry : after_br_miss_uop_cache) {
    //   // cout << "Miss rate at " << entry.first << " access " << entry.second.accesses << " misses " << entry.second.misses << " "
    //   //   << double(double(entry.second.misses) / double(entry.second.accesses)) * 100.00 << endl;
    //   if (entry.second.misses == 0 || entry.second.accesses == 0)
    //     js["after_br_miss"][to_string(entry.first)] = 0;
    //   else
    //     js["after_br_miss"][to_string(entry.first)] = double(double(entry.second.misses) / double(entry.second.accesses)) * 100.00;
    // }

    uint64_t t_tage_miss = tage_hit_misses + tage_alt_misses + tage_bim_misses + tage_bim1in8_misses + tage_sat_misses + tage_loop_misses;
    js["tage_predictor_info"]["tage_hit_misses"] = (double(tage_hit_misses) / double(t_tage_miss)) * 100;
    js["tage_predictor_info"]["tage_alt_misses"] = (double(tage_alt_misses) / double(t_tage_miss)) * 100;
    js["tage_predictor_info"]["tage_bim_misses"] = (double(tage_bim_misses) / double(t_tage_miss)) * 100;
    js["tage_predictor_info"]["tage_bim1in8_misses"] = (double(tage_bim1in8_misses) / double(t_tage_miss)) * 100;
    js["tage_predictor_info"]["tage_sat_misses"] = (double(tage_sat_misses) / double(t_tage_miss)) * 100;
    js["tage_predictor_info"]["tage_loop_misses"] = (double(tage_loop_misses) / double(t_tage_miss)) * 100;

    std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> sorted_entries(btb_banking_stats.begin(), btb_banking_stats.end());

    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    std::cout << "BTB CONFLICT STATS: " << btb_banking_stats.size() << std::endl;
    int btb_count = 0;
    for (const auto& entry : sorted_entries) {
      if (btb_count >= 10) {
        break;
      }

      js["btb_conflict_average"][to_string(btb_count)] = (double(entry.second.first) / double(entry.second.second));
      js["btb_conflict_total_cycle"][to_string(btb_count)] = entry.second.first;
      js["btb_conflict_occurance"][to_string(btb_count)] = entry.second.second;
      std::cout << "BTB Banking conflict IP: " << std::hex << entry.first << std::dec << ", Total cycle waiting: " << entry.second.first
                << ", number of occurances: " << entry.second.second << std::endl;
      ++btb_count;
    }

    // std::cout << "IP with longest BTB conflict! " <<

    // for (auto& btb : btb_banking_stats) {
    //   cout << std::hex << " IP " << btb.first << std::dec << " cycles " << btb.second.first << " times " << btb.second.second << endl;
    // }

    js["btb_banking"]["pref_got_pref_mpki"] = double(pref_got_pref) / double(m_total_retired) * 1000;
    cout << "DEMAND_DELAYED_DUE_TO_PREF_KI: " << double(pref_got_pref) / double(m_total_retired) * 1000 << endl;

    js["btb_banking"]["avg_demand_btb_check"] = double(demand_check_btb) / double(demand_btb_cycle);
    cout << "DEMAND_BTB_CHECK_PER_CYCLE: " << double(demand_check_btb) / double(demand_btb_cycle) << endl;

    cout << "demand_decodes: " << double(m_decode_stats.demand_decode) / double(m_decode_stats.demand_decode + m_decode_stats.prefetch_decode) * 100 << endl;
    cout << "pref_decodes: " << double(m_decode_stats.prefetch_decode) / double(m_decode_stats.demand_decode + m_decode_stats.prefetch_decode) * 100 << endl;
    js["decode_stats"]["demand_decodes"] = double(m_decode_stats.demand_decode) / double(m_decode_stats.demand_decode + m_decode_stats.prefetch_decode) * 100;
    js["decode_stats"]["pref_decodes"] = double(m_decode_stats.prefetch_decode) / double(m_decode_stats.demand_decode + m_decode_stats.prefetch_decode) * 100;
    js["decode_stats"]["num_demand_decodes"] = double(m_decode_stats.demand_decode);
    js["decode_stats"]["num_pref_decodes"] = double(m_decode_stats.prefetch_decode);

    double total_evictions = m_eviction_stats.by_demand + m_eviction_stats.by_pref_not_used + m_eviction_stats.by_pref_used;
    cout << "Eviction_of_demand: " << double(m_eviction_stats.by_demand) / total_evictions * 100 << endl;
    cout << "Eviction_of_pref_used: " << double(m_eviction_stats.by_pref_used) / total_evictions * 100 << endl;
    cout << "Eviction_of_pref_not_used: " << double(m_eviction_stats.by_pref_not_used) / total_evictions * 100 << endl;
    js["eviction_stats"]["Eviction_of_demand"] = double(m_eviction_stats.by_demand) / total_evictions * 100;
    js["eviction_stats"]["Eviction_of_pref_used"] = double(m_eviction_stats.by_pref_used) / total_evictions * 100;
    js["eviction_stats"]["Eviction_of_pref_not_used"] = double(m_eviction_stats.by_pref_not_used) / total_evictions * 100;

    js["eviction_stats"]["num_Eviction_of_demand"] = double(m_eviction_stats.by_demand);
    js["eviction_stats"]["num_Eviction_of_pref_used"] = double(m_eviction_stats.by_pref_used);
    js["eviction_stats"]["num_Eviction_of_pref_not_used"] = double(m_eviction_stats.by_pref_not_used);

    cout << "MRC_reads: " << mrc_checks << endl;
    cout << "MRC_hits: " << mrc_hits << endl;
    cout << "MRC_hit_rate: " << double(mrc_hits) / double(mrc_checks) * 100 << endl;
    cout << "MRC_already_in_uop_cache: " << double(mrc_in_uop_cache) / double(mrc_checks) * 100 << endl;

    js["mrc_stats"]["MRC_hit_rate"] = double(mrc_hits) / double(mrc_checks) * 100;
    js["mrc_stats"]["MRC_already_in_uop_cache"] = double(mrc_in_uop_cache) / double(mrc_hits) * 100;

    cout << "Average_instr_hits_MRC: " << double(mrc_hit_length) / double(mrc_found) << endl;
    js["mrc_stats"]["Average_instr_hits_MRC"] = double(mrc_hit_length) / double(mrc_found);
    cout << "MRC_target_hit: " << double(mrc_target_hit) / double(mrc_target_check) * 100 << endl;
    js["mrc_stats"]["MRC_target_hit"] = double(mrc_target_hit) / double(mrc_target_check) * 100;

    js["cpu_stats"]["instr_till_first_uop_cache_miss_after_pipeline_resteer"] = double(instr_till_first_uop_miss) / double(pipeline_resteer);
    cout << "instr_till_first_uop_cache_miss_after_pipeline_resteer: " << double(instr_till_first_uop_miss) / double(pipeline_resteer) << endl;

    double total_alt_path_stopped = alt_stop_by_max_ip + alt_stop_by_limit_ip + alt_stop_by_btb_miss + alt_stop_by_sat_counter;

    cout << "alt_stop_by_max_ip: " << double(alt_stop_by_max_ip) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_limit_ip: " << double(alt_stop_by_limit_ip) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_btb_miss: " << double(alt_stop_by_btb_miss) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_sat_counter: " << double(alt_stop_by_sat_counter) / total_alt_path_stopped * 100 << endl;

    js["alt_stop_stats"]["alt_stop_by_max_ip"] = double(alt_stop_by_max_ip) / total_alt_path_stopped * 100;
    js["alt_stop_stats"]["alt_stop_by_limit_ip"] = double(alt_stop_by_limit_ip) / total_alt_path_stopped * 100;
    js["alt_stop_stats"]["alt_stop_by_btb_miss"] = double(alt_stop_by_btb_miss) / total_alt_path_stopped * 100;
    js["alt_stop_stats"]["alt_stop_by_sat_counter"] = double(alt_stop_by_sat_counter) / total_alt_path_stopped * 100;

    js["alt_stop_stats"]["conditional_br_alt_path"] = conditional_alt_path;
    js["alt_stop_stats"]["conditional_br_demand_path"] = conditional_demand_path;
    js["alt_stop_stats"]["percentage_cond_br"] = double(conditional_alt_path / conditional_demand_path) * 100.0;
    cout << "conditional_br_alt_path: " << conditional_alt_path << endl;
    cout << "conditional_br_demand_path: " << conditional_demand_path << endl;
    cout << "A_TAGE_VS_D_TAGE: " << double(conditional_alt_path / conditional_demand_path) * 100.0 << endl;
  }

  void test_print()
  {

    double total_alt_path_stopped = alt_stop_by_max_ip + alt_stop_by_limit_ip + alt_stop_by_btb_miss + alt_stop_by_sat_counter;

    cout << "alt_stop_by_max_ip: " << double(alt_stop_by_max_ip) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_limit_ip: " << double(alt_stop_by_limit_ip) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_btb_miss: " << double(alt_stop_by_btb_miss) / total_alt_path_stopped * 100 << endl;
    cout << "alt_stop_by_sat_counter: " << double(alt_stop_by_sat_counter) / total_alt_path_stopped * 100 << endl;
    cout << "TOTAL: "
         << (double(alt_stop_by_max_ip) / total_alt_path_stopped * 100) + (double(alt_stop_by_limit_ip) / total_alt_path_stopped * 100)
                + (double(alt_stop_by_btb_miss) / total_alt_path_stopped * 100) + (double(alt_stop_by_sat_counter) / total_alt_path_stopped * 100)
         << endl;
  }

  void clear_stats_after_warmup()
  {
    cout << "Clearning stats after warmup!!" << endl;
    total_cycles = 0;

    for (auto i = -4; i <= 4; i++) {
      tage_alt[i].misses = 0;
      tage_alt[i].predictions = 0;
      tage_hit[i].misses = 0;
      tage_hit[i].predictions = 0;
      tage_hit_u[i].misses = 0;
      tage_hit_u[i].predictions = 0;

      miss_coverage_alt[i] = 0;
      miss_coverage_hit[i] = 0;
    }

    for (auto i = 0; i <= 4; i++) {
      tage_bimodal[i].misses = 0;
      tage_bimodal[i].predictions = 0;
    }

    for (auto i = 0; i <= 4; i++) {
      tage_bimodal_1in8[i].misses = 0;
      tage_bimodal_1in8[i].predictions = 0;
    }

    tage_sat.clear();
    tage_loop.clear();

    m_total_retired = 0;
    ipc = 0;
    loads = 0;
    stores = 0;

    total_dispatch_width = 0;
    total_dispatch_cycle = 0;

    victim_critical_ways_used = 0;
    victim_critical_ways = 0;

    total_path_stalls = 0;
    total_path_stalls_due_to_return = 0;
    total_path_stalls_due_to_indirect = 0;
    total_path_stalls_due_to_conditional = 0;
    total_pref_paths = 0;

    total_saved_bb_size = 0;

    no_bb_info = 0;
    no_target_info = 0;

    uop_cache_read = 0;
    uop_cache_hit = 0;
    uop_cache_hit_old = 0;
    from_fetch = 0;
    uop_conflict_misses = 0;
    uop_conflict_misses_prefetched = 0;
    l1i_access = 0;
    l1i_hit = 0;
    l1i_miss = 0;

    uop_pq_stalls = 0;

    front_end_energy.decoded = 0;
    front_end_energy.fetched_from_l1i = 0;
    front_end_energy.fetched_from_uop_cache = 0;
    front_end_energy.pref_decoded = 0;

    instr_checked = 0;
    total_distance_from_last_miss = 0;

    total_page_faults = 0;
    h2p_tag_not_found = 0;

    l1d_access = 0;
    l1d_hit = 0;
    l1d_miss = 0;

    uop_cache_pref_stats.clear();

    avg_instr_saved = 0;

    total_instr_in_window = 0;
    window_instr_miss = 0;
    alt_path_with_ind_br = 0;

    branch_mpki = 0;
    uop_pref_used = 0;
    uop_pref_unused = 0;
    total_uop_pref = 0;

    conditional_alt_path = 0;
    conditional_demand_path = 0;

    alt_direction_misses = 0;
    total_predictions = 0;
    total_miss_distance = 0;
    total_h2p_distance = 0;

    hpca.ftq_head_stalled = 0;
    hpca.wrong_uop_pref_used = 0;
    hpca.total_wrong_pref = 0;
    hpca.total_pref_on_wrong_h2p_path = 0;
    hpca.br_miss_used = 0;
    hpca.late_prefs = 0;
    hpca.total_h2p_tried = 0;
    hpca.unable_to_start_pref = 0;
    hpca.total_h2p_miss = 0;
    hpca.total_time_in_ftq = 0;

    hpca.critical_checks = 0;
    hpca.critical_hits = 0;

    uop_pref_stats.hits = 0;
    uop_pref_stats.ip_correctly_predicted = 0;
    uop_pref_stats.late = 0;
    uop_pref_stats.misses = 0;
    uop_pref_stats.ip_prefetched = 0;
    uop_pref_stats.window_prefetched = 0;
    uop_pref_stats.wrong = 0;
    uop_pref_stats.early_pref = 0;
    uop_pref_stats.timely = 0;
    uop_pref_stats.reuse_on_correct_br = 0;
    uop_pref_stats.hits_from_pref = 0;
    uop_pref_stats.after_br_dib_check = 0;
    uop_pref_stats.after_br_dib_hit = 0;

    count1 = 0;
    count1k = 0;
    count10k = 0;
    count100k = 0;
    count100kPlus = 0;

    mrc_checks = 0;
    mrc_hits = 0;

    coverage_miss = 0;
    coverage_miss_bim = 0;
    coverage_miss_hit = 0;
    coverage_miss_alt = 0;
    coverage_miss_other = 0;

    avg_dispatched = 0;
    cycle_dispatched = 0;
    branch_miss_front_end_stall = 0;
    front_end_stall = 0;

    total_branch = 0;
    total_cycle_ftq_stall = 0;

    cpu_stalls.decode_full = 0;
    cpu_stalls.ftq_full = 0;
    cpu_stalls.dispatch_full = 0;
    cpu_stalls.switch_stalls = 0;

    cpu_stalls.pq_full_i = 0;
    cpu_stalls.mshr_full_i = 0;
    cpu_stalls.rq_full_i = 0;
    cpu_stalls.vapq_full_i = 0;
    cpu_stalls.uq_full_i = 0;
    cpu_stalls.puq_full_i = 0;
    cpu_stalls.wq_full_i = 0;
    cpu_stalls.predecoder_full = 0;
    cpu_stalls.uop_queue_full = 0;

    for (int i = 0; i < STATS_TABLE_ENTRIES; i++) {
      stats_table[i].accesses = 0;
      stats_table[i].misses = 0;
      stats_table[i].hits = 0;
      stats_table[i].late = 0;
      stats_table[i].early = 0;
    }

    miss_distance_from_h2p = 0;
    total_counts_from_h2p = 0;
    less_than_8.clear();

    cpu_stalls.lq_full = 0;
    cpu_stalls.sq_full = 0;
    cpu_stalls.rob_full = 0;
    cpu_stalls.empty_dispatch = 0;

    cpu_stalls.front_end_empty = 0;
    cpu_stalls.back_end_full = 0;

    histogram_of_returns.clear();
    avg_time_ftq_head_stalled = 0;
    total_ftq_stalls = 0;

    total_tage_prediction = 0;
    total_loop_prediction = 0;
    total_stat_prediction = 0;

    total_ind_in_alt_path = 0;

    path_has_h2p = 0;
    total_bb_distance_from_h2p = 0;
    average_bb_when_h2p = 0;

    demand_check_btb = 0;
    alt_check_btb = 0;

    demand_btb_cycle = 0;
    alt_btb_cycle = 0;

    num_of_calls = 0;
    avg_call_size = 0;
    h2p_indirect_diff_target = 0;
    h2p_return_diff_target = 0;

    total_calls = 0;
    total_call_miss = 0;

    stats_branch.marked_critical = 0;
    stats_branch.total_branches = 0;
    stats_branch.miss = 0;
    stats_branch.miss_decode = 0;
    stats_branch.miss_execute = 0;
    stats_branch.critical_miss = 0;

    // total_h2p_tried = 0;
    // unable_to_start_pref = 0;

    stats_branch.direct_jump = 0;
    stats_branch.indirect = 0;
    stats_branch.conditional = 0;
    stats_branch.direct_call = 0;
    stats_branch.indirect_call = 0;
    stats_branch.branch_return = 0;
    stats_branch.others = 0;
    stats_branch.btb_miss = 0;
    stats_branch.bp_miss = 0;
    stats_branch.btb_predictions = 0;
    stats_branch.h2p_btb_misses = 0;

    alt_path_stop_info.btb_miss = 0;
    alt_path_stop_info.ind_branch = 0;
    alt_path_stop_info.max_br = 0;
    alt_path_stop_info.max_ip = 0;
    alt_path_stop_info.no_target_other = 0;
    alt_path_stop_info.no_target_ind = 0;
    alt_path_stop_info.stop_at_sat_counter = 0;
    alt_path_stop_info.max_br_allowed = 0;
    alt_path_stop_info.max_ip_limit = 0;

    correct_alt_path_stop_info.bp_wrong = 0;
    correct_alt_path_stop_info.btb_wrong = 0;
    correct_alt_path_stop_info.was_ind = 0;

    for (auto& x : speculative_miss.misses) {
      x = 0;
    }
    for (auto& x : speculative_miss.avg_resolve_time) {
      x = 0;
    }
    for (auto& x : speculative_miss.branch_occurance) {
      x = 0;
    }

    stats_branch_miss.condition_miss = 0;
    stats_branch_miss.target_direct = 0;
    stats_branch_miss.target_indirect = 0;
    stats_branch_miss.target_return = 0;

    l1_bp_stats.mispredictions = 0;
    l1_bp_stats.total_not_taken = 0;
    l1_bp_stats.total_preditions = 0;
    l1_bp_stats.total_taken = 0;

    l2_bp_stats.mispredictions = 0;
    l2_bp_stats.total_not_taken = 0;
    l2_bp_stats.total_preditions = 0;
    l2_bp_stats.total_taken = 0;

    l1_btb_stats.mispredictions = 0;
    l1_btb_stats.total_not_taken = 0;
    l1_btb_stats.total_preditions = 0;
    l1_btb_stats.total_taken = 0;

    l2_btb_stats.mispredictions = 0;
    l2_btb_stats.total_not_taken = 0;
    l2_btb_stats.total_preditions = 0;
    l2_btb_stats.total_taken = 0;

    bp_btb_stats.mispredictions = 0;
    bp_btb_stats.total_not_taken = 0;
    bp_btb_stats.total_preditions = 0;
    bp_btb_stats.total_taken = 0;
    bp_btb_stats.btb_miss = 0;
    bp_btb_stats.btb_prediction = 0;
    bp_btb_stats.bp_miss = 0;
    bp_btb_stats.bp_prediction = 0;

    for (auto& x : bp_btb_stats.misses) {
      x = 0;
    }
    for (auto& x : bp_btb_stats.misses_at_exec) {
      x = 0;
    }

    tage_hit_misses = 0;
    tage_alt_misses = 0;
    tage_bim_misses = 0;
    tage_bim1in8_misses = 0;
    tage_sat_misses = 0;
    tage_loop_misses = 0;

    alt_bp_btb_stats.alt_misses = 0;
    alt_bp_btb_stats.total_alt_predictions = 0;

    alt_bp_btb_stats.total_cond_pred = 0;
    alt_bp_btb_stats.total_cond_miss = 0;

    alt_bp_btb_stats.level_one_predictions = 0;
    alt_bp_btb_stats.level_one_misses = 0;

    alt_bp_btb_stats.alt_miss_type_bim = 0;
    alt_bp_btb_stats.alt_miss_type_tage = 0;
    alt_bp_btb_stats.alt_miss_type_others = 0;

    alt_bp_btb_stats.alt_btb_predictions = 0;
    alt_bp_btb_stats.alt_btb_predicted_correctly = 0;
    alt_bp_btb_stats.alt_btb_same_as_main_path = 0;
    alt_bp_btb_stats.alt_path_btb_miss = 0;
    alt_bp_btb_stats.alt_path_bp_miss = 0;

    lines_pref_stats.line_pref_by_correct_alt = 0;
    lines_pref_stats.line_pref_by_wrong_alt = 0;
    lines_pref_stats.number_of_correct_alt_paths = 0;
    lines_pref_stats.number_of_wrong_alt_paths = 0;

    mrc_target_check = 0;
    mrc_target_hit = 0;

    for (auto& x : alt_bp_btb_stats.misses) {
      x = 0;
    }
    for (auto& x : alt_bp_btb_stats.main_path_misses) {
      x = 0;
    }

    alt_stop_by_max_ip = 0;
    alt_stop_by_limit_ip = 0;
    alt_stop_by_btb_miss = 0;
    alt_stop_by_sat_counter = 0;

    detailed_energy.dib_check = 0;
    detailed_energy.dib_hits = 0;

    mrc_hit_length = 0;
    mrc_found = 0;

    total_saved_bb_size_wh2p = 0;
    total_pref_paths_wh2p = 0;

    detailed_energy.addr_gen_events = 0;
    detailed_energy.addr_gen_events_alt = 0;
    detailed_energy.rf_reads = 0;
    detailed_energy.rf_writes = 0;
    detailed_energy.rob_writes = 0;
    detailed_energy.lq_writes = 0;
    detailed_energy.lq_searches = 0;
    detailed_energy.sq_writes = 0;
    detailed_energy.sq_searches = 0;

    detailed_energy.l1i_load_hit = 0;
    detailed_energy.l1i_total_miss = 0;
    detailed_energy.l1i_rfo_hit = 0;
    detailed_energy.l1i_pref_hit = 0;

    detailed_energy.l1d_load_hit = 0;
    detailed_energy.l1d_total_miss = 0;
    detailed_energy.l1d_rfo_hit = 0;
    detailed_energy.l1d_pref_hit = 0;

    detailed_energy.l2c_load_hit = 0;
    detailed_energy.l2c_total_miss = 0;
    detailed_energy.l2c_rfo_hit = 0;
    detailed_energy.l2c_pref_hit = 0;

    detailed_energy.llc_load_hit = 0;
    detailed_energy.llc_total_miss = 0;
    detailed_energy.llc_rfo_hit = 0;
    detailed_energy.llc_pref_hit = 0;

    critical_uop_checkss = 0;
    critical_uop_hits = 0;
    mrc_in_uop_cache = 0;
    pipeline_resteer = 0;
    instr_till_first_uop_miss = 0;

    pref_got_pref = 0;

    m_decode_stats.demand_decode = 0;
    m_decode_stats.prefetch_decode = 0;

    after_br_miss_uop_cache.clear();
    btb_banking_stats.clear();
    btb_bank_conflict_max.clear();

    number_of_lines_by_ideal = 0;
    number_of_brs = 0;

    prediction_to_dispatch = 0;

    h2p_predictor_stats.total_conditional_misses = 0;
    h2p_predictor_stats.total_h2p_marked_correctly = 0;
    h2p_predictor_stats.total_marked_as_h2p = 0;

    changed_by_sc = 0;
    lsum_negative = 0;

    critical_hits = 0;
    critical_reads = 0;
    total_branch_mispredictions = 0;
    critically_marked_branches = 0;

    avg_branch_recovery_time = {0, 0};

    m_eviction_stats.by_demand = 0;
    m_eviction_stats.by_pref_not_used = 0;
    m_eviction_stats.by_pref_used = 0;

    conflict_stats.conflict_btb = 0;
    conflict_stats.conflict_window = 0;
    conflict_stats.total_btb_conflict_check = 0;
    conflict_stats.total_window_conflict_check = 0;

    uop_cache_hit_stalls.decode_full = 0;
    uop_cache_hit_stalls.uop_queue_full = 0;
    uop_cache_hit_stalls.rob_full = 0;
    uop_cache_hit_stalls.dispatch_full = 0;
    uop_cache_hit_stalls.uop_hit_followed_by_l1i_miss = 0;
    uop_cache_hit_stalls.avg_cycle_in_ftq = 0;
    uop_cache_hit_stalls.build_path_stall = 0;

    for (auto x = 0; x <= sizeof(sets_used) / sizeof(uint64_t); x++) {
      sets_used[x] = 0;
    }

    for (int x = 0; x < 64; x++) {
      for (int y = 0; y < 8; y++)
        sets_used_l1i[x][y] = 0;
    }
    br_distance_map.clear();
    num_of_lines_pref.clear();
    num_of_lines_pref_by_correct_h2p.clear();
    num_of_lines_pref_by_wrong_h2p.clear();
  }

  // Add stats here
  uint64_t total_cycles = 0;
  uint64_t m_total_retired = 0;
  uint64_t m_total_access_l1d[NUM_TYPES] = {0};
  double ipc = 0;
  uint64_t loads = 0;
  uint64_t stores = 0;
  uint64_t uop_cache_read = 0;
  uint64_t uop_cache_hit = 0;
  uint64_t uop_cache_hit_old = 0;
  uint64_t from_fetch = 0;

  bool wrong_path = false;

  uint64_t l1i_access = 0;
  uint64_t l1i_hit = 0;
  uint64_t l1i_miss = 0;

  uint64_t critical_hits = 0;
  uint64_t critical_reads = 0;

  uint64_t total_branch_mispredictions = 0;
  uint64_t critically_marked_branches = 0;

  uint64_t total_branch = 0;

  uint64_t l1d_access;
  uint64_t l1d_hit;
  uint64_t l1d_miss;

  uint64_t uop_pref_used = 0;
  uint64_t uop_pref_unused = 0;
  uint64_t total_uop_pref = 0;

  uint64_t total_dispatch_width = 0;
  uint64_t total_dispatch_cycle = 0;

  uint64_t h2p_indirect_diff_target = 0;
  uint64_t h2p_return_diff_target = 0;

  uint64_t total_instr_in_window = 0;
  uint64_t window_instr_miss = 0;

  uint64_t total_ind_in_alt_path = 0;
  uint64_t alt_path_with_ind_br = 0;

  uint64_t uop_pq_stalls = 0;

  std::map<uint64_t, uint64_t> spec_yout_map;
  std::map<uint64_t, uint64_t> main_yout_map;

  std::map<uint64_t, long long int> branch_map;
  std::map<uint64_t, long long int> h2p_map;
  std::map<uint64_t, long long int> llc_map;
  std::map<uint64_t, long long int> l2i_map;

  std::map<uint64_t, long long int> call_map;
  std::map<uint64_t, long long int> ftq_stall_map;
  std::map<uint64_t, long long int> ftq_stall_avg_distance_map;
  uint64_t total_cycle_ftq_stall = 0;

  std::map<uint64_t, long long int> avg_instr_btwn_missbranch;

  uint64_t victim_critical_ways_used;
  uint64_t victim_critical_ways;

  uint64_t prediction_to_dispatch = 0;
  stats_entry stats_table[STATS_TABLE_ENTRIES];

  uint64_t total_path_stalls = 0;
  uint64_t total_path_stalls_due_to_return = 0;
  uint64_t total_path_stalls_due_to_indirect = 0;
  uint64_t total_path_stalls_due_to_conditional = 0;
  uint64_t total_pref_paths = 0;
  uint64_t no_bb_info = 0;
  uint64_t no_target_info = 0;

  uint64_t num_of_recoveries = 0;
  uint64_t total_instruction_recovered = 0;

  uint64_t coverage_miss = 0;
  uint64_t coverage_miss_bim = 0;
  uint64_t coverage_miss_hit = 0;
  uint64_t coverage_miss_alt = 0;
  uint64_t coverage_miss_other = 0;

  uint64_t count1 = 0;
  uint64_t count1k = 0;
  uint64_t count10k = 0;
  uint64_t count100k = 0;
  uint64_t count100kPlus = 0;

  uint64_t total_calls = 0;
  uint64_t total_call_miss = 0;

  uint64_t avg_dispatched = 0;
  uint64_t cycle_dispatched = 0;
  uint64_t branch_miss_front_end_stall = 0;
  uint64_t front_end_stall = 0;

  uint64_t avg_instr_saved = 0;
  std::pair<uint64_t, uint64_t> avg_branch_recovery_time = {0, 0};

  uint64_t uop_conflict_misses = 0;
  uint64_t uop_conflict_misses_prefetched = 0;

  uint64_t path_has_h2p = 0;
  uint64_t total_bb_distance_from_h2p = 0;
  uint64_t average_bb_when_h2p = 0;

  uint64_t total_saved_bb_size_wh2p = 0;
  uint64_t total_pref_paths_wh2p = 0;

  uint64_t demand_check_btb = 0;
  uint64_t alt_check_btb = 0;

  uint64_t demand_btb_cycle = 0;
  uint64_t alt_btb_cycle = 0;

  std::map<uint64_t, uint64_t> alt_br_correct;

  uint64_t total_miss_distance = 0;
  uint64_t total_h2p_distance = 0;

  double branch_mpki = 0;
  uint64_t m_RQ_SIZE, m_PQ_SIZE, m_WQ_SIZE, m_PTWQ_SIZE, m_OFFSET_BITS;

  bool warmup_complete[NUM_CPUS] = {false};

  uint64_t sets_used[32] = {0};

  uint64_t sets_used_l1i[64][8] = {0};

  // System parameters
  uint32_t cpu;
  double freq_scale;
  std::size_t ifetch_buffer_size;
  std::size_t decode_buffer_size;
  std::size_t dispatch_buffer_size;
  std::size_t rob_size;
  std::size_t lq_size;
  std::size_t sq_size;
  unsigned fetch_width;
  unsigned decode_width;
  unsigned dispatch_width;
  unsigned schedule_width;
  unsigned execute_width;
  unsigned lq_width;
  unsigned sq_width;
  unsigned retire_width;
  unsigned mispredict_penalty;
  unsigned decode_latency;
  unsigned dispatch_latency;
  unsigned schedule_latency;
  unsigned execute_latency;

  unsigned l1cd_sets = 0;
  unsigned l1cd_ways = 0;

  unsigned l1ci_sets = 0;
  unsigned l1ci_ways = 0;
  unsigned l1ci_mshr = 0;

  uint64_t h2p_tag_not_found = 0;

  uint64_t num_of_calls = 0;
  uint64_t avg_call_size = 0;

  uint64_t total_page_faults = 0;

  uint64_t test = 0;
  ofstream stats;
  json js;
  string exe_name;
  string full_file_name;

  struct miss_histo_entry {
    uint64_t predictions = 0;
    uint64_t misses = 0;
  };

  std::map<int, miss_histo_entry> tage_alt;
  std::map<int, miss_histo_entry> tage_hit;
  std::map<int, miss_histo_entry> tage_bimodal;
  std::map<int, miss_histo_entry> tage_bimodal_1in8;
  std::map<int, miss_histo_entry> tage_loop;
  std::map<int, miss_histo_entry> tage_sat;
  std::map<int, miss_histo_entry> tage_hit_u;

  std::map<int, uint64_t> miss_coverage_alt;
  std::map<int, uint64_t> miss_coverage_hit;

  uint64_t total_tage_prediction = 0;
  uint64_t total_loop_prediction = 0;
  uint64_t total_stat_prediction = 0;

  uint64_t lsum_negative = 0;

  std::map<int, miss_histo_entry> gtable_hit_miss_ctr;
  std::map<int, miss_histo_entry> gtable_alt_miss_ctr;
  std::map<int, miss_histo_entry> gtable_comb_miss_ctr;

  std::map<int, miss_histo_entry> gtable_miss_u;

  std::map<int, miss_histo_entry> btalbe_miss_ctr;
  std::map<int, miss_histo_entry> btalbe_miss_u;
  std::map<int, miss_histo_entry> loop_miss_ctr;
  std::map<int, miss_histo_entry> loop_miss_u;

  std::map<int, miss_histo_entry> miss_histogram;
  std::map<int, uint64_t> hit_histogram;
  uint64_t alt_direction_misses = 0;
  uint64_t total_predictions = 0;

  hpca_stats hpca;

  std::map<uint64_t, uop_cache_prefetch_stats> uop_cache_pref_stats;

  std::map<uint64_t, uint64_t> uop_cache_prefetching_hits;
  std::map<uint64_t, uint64_t> uop_cache_prefetched_lines;

  std::map<uint64_t, uint64_t> instr_blocked_ftq; // 1->load, 2->stores, 3->branches,4->other
  uint64_t instr_checked = 0;
  uint64_t total_distance_from_last_miss = 0;

  std::map<uint64_t, std::vector<std::pair<uint64_t, uint64_t>>> indirect_target_map;

  uop_stats_entry uop_pref_stats;

  uint64_t miss_distance_from_h2p;
  uint64_t total_counts_from_h2p;
  std::map<uint64_t, uint64_t> less_than_8;

  std::map<uint64_t, uint64_t> br_distance_map;

  energy_stats front_end_energy;

  // ADDED: Wrong syntax fix
  uint64_t total_saved_bb_size = 0, window_prefetched;
  std::map<uint64_t, path_info> main_path, alt_path;

  uop_hit_stalls uop_cache_hit_stalls;
  stalls cpu_stalls;
  branch_stats stats_branch;
  h2p_predictor h2p_predictor_stats;
  branch_miss stats_branch_miss;
  bp_stats l1_bp_stats;
  bp_stats l2_bp_stats;
  bp_stats l1_btb_stats;
  bp_stats l2_btb_stats;
  bp_stats bp_btb_stats;
  bp_stats alt_bp_btb_stats;
  bp_stats speculative_miss;

  uint64_t pref_got_pref = 0;

  uint64_t number_of_lines_by_ideal = 0;
  uint64_t number_of_brs = 0;

  lines_prefetched lines_pref_stats;

  conflict_info conflict_stats;

  uint64_t critical_uop_checkss = 0;
  uint64_t critical_uop_hits = 0;

  uint64_t changed_by_sc = 0;

  std::map<uint64_t, uint64_t> num_of_lines_pref;
  std::map<uint64_t, uint64_t> num_of_lines_pref_by_correct_h2p;
  std::map<uint64_t, uint64_t> num_of_lines_pref_by_wrong_h2p;
  std::map<uint64_t, uint64_t> histogram_of_returns;

  typedef struct __stats_window_hits {
    uint64_t accesses;
    uint64_t misses;
  } stats_window_hits;

  std::map<uint64_t, stats_window_hits> after_br_miss_uop_cache;

  // ips how long they wait and how many times they are repeated
  std::map<uint64_t, std::pair<uint64_t, uint64_t>> btb_banking_stats;

  std::map<uint64_t, uint64_t> btb_bank_conflict_max;

  uint64_t avg_time_ftq_head_stalled;
  uint64_t total_ftq_stalls;

  alt_path_stop alt_path_stop_info;
  correct_alt_path_stop correct_alt_path_stop_info;
  correct_alt_path_stop wrong_alt_path_stop_info;
  energy_stats_detailed detailed_energy;

  uint64_t tage_hit_misses = 0;
  uint64_t tage_alt_misses = 0;
  uint64_t tage_bim_misses = 0;
  uint64_t tage_bim1in8_misses = 0;
  uint64_t tage_sat_misses = 0;
  uint64_t tage_loop_misses = 0;

  decode_stats m_decode_stats;
  eviction_stats m_eviction_stats;

  uint64_t mrc_checks = 0;
  uint64_t mrc_hits = 0;

  uint64_t mrc_in_uop_cache = 0;

  uint64_t pipeline_resteer = 0;
  uint64_t instr_till_first_uop_miss = 0;

  uint64_t alt_stop_by_max_ip = 0;
  uint64_t alt_stop_by_limit_ip = 0;
  uint64_t alt_stop_by_btb_miss = 0;
  uint64_t alt_stop_by_sat_counter = 0;

  uint64_t mrc_hit_length = 0;
  uint64_t mrc_found = 0;

  uint64_t mrc_target_check = 0;
  uint64_t mrc_target_hit = 0;

  uint64_t conditional_alt_path = 0;
  uint64_t conditional_demand_path = 0;
};

inline Profiler* get_profiler_ptr = new Profiler;
#endif 