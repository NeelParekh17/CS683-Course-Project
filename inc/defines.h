#define UOP_BUFFER_SIZE 144
#define UOP_CACHE_THROUGHPUT 8
#define ALT_RAS_SIZE 16
#define UOP_CACHE_NUM_TICKS 1000
#define NUM_PREF_CACHE_LINE 2
#define UOP_CACHE_PQ_SIZE 32
#define NUM_BRANCHES_SAVED 8
#define PREF_DECODE_WIDTH 8
#define NUM_HIST_PATH_BITS 16
#define UOP_CACHE_NUM_SETS 64
#define ALT_SIZE_8KB
#define H2P_T 500
#define UOP_MSHR 32
#define MAX_IP_CHECK 64
#define STOP_AT_MAX_CYCLE
#define MAX_CYCLE_IP 20000
#define NUM_OF_ENTRIES 64
#define NUM_OF_INSTR_PER_ENTRY 64
#define IDEAL_BTB_BANKING
#define BTB_BANKS 32
#define BTB_CONF_PROB 10
#define NO_ALT_INDIRECT_PREDICTOR
#define H2P_TAGE_NON_BOUNDARY
#define H2P_TAGE_STYLE_CTR_SC_BIMH
#define L1I_PREF_AT_TRY_HIT
#define UOP_CACHE_LRU_RP
#define UOP_CACHE_RECOVERY_SIZE 8
#define REAL_BRANCH_RECOVERY
#define PREFETCH_PATHS
#define H2P_INDIRECT_SIZE_ITTAGE 4
#define USE_ITTAGE_AS_H2P
#define CONFIGURATION "real_br_with_pref-mrc_size-64-max_ip_check-64-limit_ip-20000-btb_probability-10-no_instr-4KB_ITTAGE-500-no_alt_indirect_pred-lru-at_try_hit-8-64-tage_non_boundary_ctr_ctr_all_sc_bim_hist-16"
