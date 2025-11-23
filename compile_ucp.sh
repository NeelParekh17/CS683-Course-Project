#!/bin/bash
set -e
for bin in "real_br_with_pref" #"base" #"base" #"ideal_uop" #"base" #"real_br_with_pref" #"base_with_mrc" #"real_br_with_pref" #"base" #"base_with_mrc" #"base" #"real_br_with_pref" #"base_with_mrc" #"real_br_with_pref" #"base" "base_with_mrc" #"real_br_with_pref" #"base" #"base_with_mrc" #"real_br_with_pref" #"base_with_mrc" #"real_br_with_pref" #"base" #"real_br_with_pref" #"base_with_mrc" #"real_br_with_pref" #"base" # #"ideal_br_only_cond"  #"real_br_with_pref" #"ideal_br" #"ideal_br_only_cond" #"base" "uop_l1i_hits" #"real_br_with_pref_8KB_50_32MSHR" #"ideal_br_only_cond" "ideal_br" #"base" #"uop_l1i_hits" #"real_br_with_pref_8KB_50_32MSHR" #"base" "uop_l1i_hits" #"ideal_br_only_cond" #"real_br_with_pref_8KB_50_32MSHR" #"real_br_with_pref_8KB_110" "real_br_with_pref_8KB_120" "real_br_with_pref_8KB_130" "real_br_with_pref_8KB_140" "real_br_with_pref_8KB_150" #"real_br_with_pref_8KB_30" #"real_br_with_pref_8KB_40" "real_br_with_pref_8KB_50" "real_br_with_pref_8KB_10" "real_br_with_pref_8KB_20" "real_br_with_pref_8KB_60" "real_br_with_pref_8KB_70" "real_br_with_pref_8KB_80" "real_br_with_pref_8KB_90" "real_br_with_pref_8KB_100"  #"real_br_with_pref_8KB_50" #"real_br_with_pref_8KB_50" #"real_br_with_pref_8KB_40" "real_br_with_pref_8KB_50" "real_br_with_pref_8KB_60"
do
    for l1i_pref_training in "at_try_hit" #"at_ftq"
    do
        for l1i_pref in "no_instr" #"ISCA_Entangling_4Ke_instr" "FNL-MMA_instr" "D-JOLT_instr" #"no_instr" #"ISCA_Entangling_4Ke_instr" #"FNL-MMA_instr" #"D-JOLT_instr" #"no_instr" #"ISCA_Entangling_4Ke_instr" #"no_instr" "FNL-MMA_instr" #"next_line_instr" "D-JOLT_instr" "mini_djolt_instr" "ISCA_Entangling_4Ke_instr" "FNL-MMA_instr" #"ISCA_Entangling_4Ke_instr" #"D-JOLT_instr" "next_line_instr"  #"no_instr"
        do
            for h2p_indirect_size in "4KB_ITTAGE" #"8KB_ITTAGE" #"16KB_ITTAGE" "32KB_ITTAGE" "64KB_ITTAGE" #"8KB_ITTAGE" "4KB_ITTAGE" #"1KB" #"2KB" "3KB" #"4KB" "5KB" "6KB"  #"1KB" "2KB" "3KB" "4KB" "5KB" "6KB" "7KB"
            do
                for h2p_sat_value in "500" #"500" #"400" #"100" #"10" "20" "30" "40" "50" "60" "70" "80" "90" "100" "110" "120" "130" "140" "150" "160" "170" "180" "190" "200" "500" "1000" "2000" "3000" "4000" "5000" "6000" "7000" "8000" "9000" "10000" #"100" #"10" "50" "100" "500" "1000" "1500" "2000" "2500" "3000" #"10" "50" "100" "150" "200" "
                do
                    for use_alt_indirect_predictor in "alt_indirect_pred" #"no_alt_indirect_pred"
                    do
                        for uop_cache_replacement in "lru" #"hotloop" #"lru" #"hotloop" #"lru" #"lru" #"hotloopcritical" #"hotloop" #"lru" #"smartlru" #"lru" #"hotloop"
                        do
                            for recovery_size in "8" #"16" #"8" #"16" #"64" #"32" "64"  "96"  "128" #"8"
                            do
                                for uop_cache_sets in "64" #"128" "256" "512" "1024"
                                do
                                    for h2p_type in "tage_non_boundary_ctr_ctr_all_sc_bim_hist" #"tage_non_boundary_ctr_seznec" #"tage_non_boundary_ctr_ctr_all_sc_bim_hist" #"tage_non_boundary_ctr_seznec" "tage_non_boundary_ctr_ctr_all_sc_bim_hist" #"tage_non_boundary_ctr_btb" #"tage_non_boundary_ctr_seznec_btb"  "tage_non_boundary_ctr_seznec" "tage_non_boundary_ctr_ctr" #"tage_non_boundary_ctr_seznec" "tage_non_boundary_ctr_ctr" "h2p_table_prediction" "tage_non_boundary_ctr_seznec_btb"  #"hashed_perceptron_50" #"tage_non_boundary_ctr_btb" "tage_non_boundary_ctr_seznec" "tage_non_boundary_ctr_ctr" "hashed_perceptron_50" "h2p_table_prediction" "tage_non_boundary_ctr_seznec_btb"
                                    do
                                        for alt_ras_size in "16" #"2" "4" "8" "16" "32" "64" #"2" "1" "0" #"99999" #"4" "8" "10" #"0" "1" "2" "3" "4" "5" "10" #"15" "20" "25" "30" "35" "40" "45" "50" #"60" "70" "80" "90" "100" #"16" #"8" "4" #"8" "4" #"4" #"6" #"4" #"5" "6" "7" "8" #"4" "2" #"3"
                                        do
                                            for btb_probability in "10" #"0" #"10" "20" "30" #"50" "60" "70" "80" #"10" "20" "30" "40" #"4" "5" #"20" "25" "50" "100"
                                            do
                                                #set max_ip_check to 256 for the sensitivity analysis
                                                #in general set it to 64
                                                for max_ip_check in "64" #"256" #"64" #"128" #"64" #"256" #"64" #"256" #"32" "64" "128" "256" #"256" "512" "1024"
                                                do
                                                    for limit_ip in "20000" #"1024" #"5092" #"1024" #"5092" #"1024" #"3092" #"1024" #"2046" "3092"
                                                    do
                                                        for mrc_size in "64" #"8192" "16384" "32768" "65536" #"1024" #"3092" #"1024" #"2046" "3092"
                                                        do
                                                            #echo "Creating defines.h"
                                                            rm inc/defines.h
                                                            touch inc/defines.h
                                                            echo "#define UOP_BUFFER_SIZE 144" >> inc/defines.h #https://www.anandtech.com/show/16881/a-deep-dive-into-intels-alder-lake-microarchitectures/3
                                                            echo "#define UOP_CACHE_THROUGHPUT 8" >> inc/defines.h #https://www.anandtech.com/show/16881/a-deep-dive-into-intels-alder-lake-microarchitectures/3
                                                            echo "#define ALT_RAS_SIZE ${alt_ras_size}" >> inc/defines.h
                                                            echo "#define UOP_CACHE_NUM_TICKS 1000" >> inc/defines.h
                                                            echo "#define NUM_PREF_CACHE_LINE 2" >> inc/defines.h
                                                            echo "#define UOP_CACHE_PQ_SIZE 32" >> inc/defines.h
                                                            echo "#define NUM_BRANCHES_SAVED ${recovery_size}" >> inc/defines.h
                                                            echo "#define PREF_DECODE_WIDTH 8" >> inc/defines.h
                                                            echo "#define NUM_HIST_PATH_BITS 16" >> inc/defines.h #not used
                                                            echo "#define UOP_CACHE_NUM_SETS ${uop_cache_sets}" >> inc/defines.h #not used
                                                            
                                                            
                                                            ################################################################################
                                                            #UCP configs
                                                            echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                            echo "#define H2P_T ${h2p_sat_value}" >> inc/defines.h
                                                            echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            
                                                            echo "#define MAX_IP_CHECK ${max_ip_check}" >> inc/defines.h
                                                            echo "#define STOP_AT_MAX_CYCLE" >> inc/defines.h
                                                            echo "#define MAX_CYCLE_IP ${limit_ip}" >> inc/defines.h
                                                            
                                                            #MRC CONFIG
                                                            echo "#define NUM_OF_ENTRIES ${mrc_size}" >> inc/defines.h
                                                            echo "#define NUM_OF_INSTR_PER_ENTRY 64" >> inc/defines.h
                                                            
                                                            #echo "#define DECODE_SHARED" >> inc/defines.h
                                                            #echo "#define PREF_ONLY_TILL_L1I" >> inc/defines.h
                                                            #echo "#define IDEAL_BTB_BANKING" >> inc/defines.h
                                                            
                                                            echo "#define BTB_BANKS 32" >> inc/defines.h
                                                            echo "#define BTB_CONF_PROB ${btb_probability}" >> inc/defines.h
                                                            
                                                            
                                                            ################################################################################
                                                            
                                                            if [ $use_alt_indirect_predictor == "alt_indirect_pred" ] ; then
                                                                echo "#define USE_ALT_INDIRECT_PREDICTOR" >> inc/defines.h
                                                            fi
                                                            if [ $use_alt_indirect_predictor == "no_alt_indirect_pred" ] ; then
                                                                echo "#define NO_ALT_INDIRECT_PREDICTOR" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $h2p_type == "hashed_perceptron_50" ] ; then
                                                                echo "#define H2P_HP_50" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_conf" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY_CONF" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_seznec" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_SEZNEC" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_seznec_btb" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_SEZNEC_BTB" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_ctr" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_CTR" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_ctr_all_sc" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_CTR_SC" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_ctr_all_sc_bim_hist" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_CTR_SC_BIMH" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_non_boundary_ctr_btb" ] ; then
                                                                echo "#define H2P_TAGE_NON_BOUNDARY" >> inc/defines.h
                                                                echo "#define H2P_TAGE_STYLE_CTR_BTB" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "tage_prediction_type" ] ; then
                                                                echo "#define H2P_TAGE_CONF_TYPE" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "h2p_table_prediction" ] ; then
                                                                echo "#define H2P_PRED_TABLE" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_type == "ideal_h2p" ] ; then
                                                                echo "#define H2P_IDEAL" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $l1i_pref_training == "at_ftq" ] ; then
                                                                echo "#define L1I_PREF_AT_FTQ" >> inc/defines.h
                                                            fi
                                                            if [ $l1i_pref_training == "at_try_hit" ] ; then
                                                                echo "#define L1I_PREF_AT_TRY_HIT" >> inc/defines.h
                                                            fi
                                                            
                                                            
                                                            if [ $uop_cache_replacement == "lru" ] ; then
                                                                echo "#define UOP_CACHE_LRU_RP" >> inc/defines.h
                                                            fi
                                                            if [ $uop_cache_replacement == "smartlru" ] ; then
                                                                echo "#define UOP_CACHE_SMARTLRU_RP" >> inc/defines.h
                                                            fi
                                                            echo "#define UOP_CACHE_RECOVERY_SIZE ${recovery_size}" >> inc/defines.h
                                                            if [ $uop_cache_replacement == "hotloop" ] ; then
                                                                echo "#define UOP_CACHE_HOTLOOP_RP" >> inc/defines.h
                                                            fi
                                                            if [ $uop_cache_replacement == "lruhotloop" ] ; then
                                                                echo "#define UOP_CACHE_LRUHOTLOOP_RP" >> inc/defines.h
                                                            fi
                                                            if [ $uop_cache_replacement == "lrucritical" ] ; then
                                                                echo "#define UOP_CACHE_LRUCRITICAL_RP" >> inc/defines.h
                                                            fi
                                                            if [ $uop_cache_replacement == "hotloopcritical" ] ; then
                                                                echo "#define UOP_CACHE_HOTLOOPCRITICAL_RP" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $bin == "branch_uop_pref" ] ; then
                                                                echo "#define BRANCH_UOP_PREF" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "uop_l1i_hits" ] ; then
                                                                echo "#define UOP_PREF_L1I_HIT" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                                echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "l1ipref_uop_pref" ] ; then
                                                                echo "#define L1IPREF" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "branch_recovery" ] ; then
                                                                echo "#define BRANCH_RECOVERY" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_branch_recovery" ] ; then
                                                                echo "#define BRANCH_RECOVERY_HITS" >> inc/defines.h
                                                                echo "#define BRANCH_RECOVERY" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "base_no_uop_cache" ] ; then
                                                                echo "#define NO_UOP_CACHE" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_l1i" ] ; then
                                                                echo "#define IDEAL_L1I" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_uop" ] ; then
                                                                echo "#define IDEAL_UOP_CACHE" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "real_br_with_pref" ] ; then
                                                                echo "#define REAL_BRANCH_RECOVERY" >> inc/defines.h
                                                                echo "#define PREFETCH_PATHS" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "base_with_mrc" ] ; then
                                                                echo "#define WITH_MRC" >> inc/defines.h
                                                                echo "#define BASELINE" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                                echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $bin == "ideal_uop_pref_no_add" ] ; then
                                                                echo "#define REAL_BRANCH_RECOVERY" >> inc/defines.h
                                                                echo "#define NO_PREFETCH_ALL_HIT" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_uop_pref_fill" ] ; then
                                                                echo "#define REAL_BRANCH_RECOVERY" >> inc/defines.h
                                                                echo "#define UOP_PREFETCH_FILL" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $bin == "ideal_br" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                                echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_cond" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_COND" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                                echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_cond_ind" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_COND_IND" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_cond_ind_return" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_cond_ind_return_direct" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_RETURN_DIRECT" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_cond_ind_direct" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_COND_IND_DIRECT" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_ind" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_IND" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_direct" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_DIRECT" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "ideal_br_only_return" ] ; then
                                                                echo "#define IDEAL_BRANCH_RECOVERY_ONLY_RETURN" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $bin == "base_tage_8KB" ] ; then
                                                                echo "#define BASELINE" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                            fi
                                                            if [ $bin == "base" ] ; then
                                                                echo "#define BASELINE" >> inc/defines.h
                                                                echo "#define ALT_SIZE_8KB" >> inc/defines.h
                                                                echo "#define H2P_T 10" >> inc/defines.h
                                                                echo "#define UOP_MSHR 32" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $h2p_indirect_size == "1KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 1" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "2KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 2" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "3KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 3" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "4KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 4" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "5KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 5" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "6KB" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE 6" >> inc/defines.h
                                                            fi
                                                            
                                                            if [ $h2p_indirect_size == "64KB_ITTAGE" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE_ITTAGE 64" >> inc/defines.h
                                                                echo "#define USE_ITTAGE_AS_H2P" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "32KB_ITTAGE" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE_ITTAGE 32" >> inc/defines.h
                                                                echo "#define USE_ITTAGE_AS_H2P" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "16KB_ITTAGE" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE_ITTAGE 16" >> inc/defines.h
                                                                echo "#define USE_ITTAGE_AS_H2P" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "8KB_ITTAGE" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE_ITTAGE 8" >> inc/defines.h
                                                                echo "#define USE_ITTAGE_AS_H2P" >> inc/defines.h
                                                            fi
                                                            if [ $h2p_indirect_size == "4KB_ITTAGE" ] ; then
                                                                echo "#define H2P_INDIRECT_SIZE_ITTAGE 4" >> inc/defines.h
                                                                echo "#define USE_ITTAGE_AS_H2P" >> inc/defines.h
                                                            fi
                                                            if [ $l1i_pref == "ISCA_Entangling_4Ke_instr" ] ; then
                                                                echo "#define EP_PREF" >> inc/defines.h
                                                            fi
                                                            if [ $l1i_pref == "ISCA_Entangling_4Ke_opt_instr" ] ; then
                                                                echo "#define EP_PREF" >> inc/defines.h
                                                            fi
                                                            if [ $l1i_pref == "FNL-MMA_instr" ] ; then
                                                                echo "#define FNL_MMA" >> inc/defines.h
                                                            fi
                                                            if [ $l1i_pref == "D-JOLT_instr" ] ; then
                                                                echo "#define D_JOLT" >> inc/defines.h
                                                            fi
                                                            #config=${bin}-mrc_size-${mrc_size}-max_ip_check-${max_ip_check}-limit_ip-${limit_ip}-btb_probability-${btb_probability}-${l1i_pref}-${h2p_indirect_size}-${h2p_sat_value}-${use_alt_indirect_predictor}-${uop_cache_replacement}-${l1i_pref_training}-${recovery_size}-${uop_cache_sets}-${h2p_type}-${alt_ras_size}
                                                            config=UCP
                                                            echo "#define CONFIGURATION "'"'"$config"'"'"" >> inc/defines.h
                                                            cp -f alder_lake_oc.json generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/BINARY/$config/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/uop_cache_replacement/$uop_cache_replacement/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/DIB_SETS/$uop_cache_sets/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/IFETCH_BUFFER_SIZE_VALUE/$pref_nesting_level/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/WINDOW_SIZE_DIB/16/g" -e "s/NUM_WAYS_DIB/8/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/NUM_SETS_DIB/$dib_sets/g" generated_configs/champsim_config-$config.json
                                                            sed -i -e "s/L1I_PREFETCHER/$l1i_pref/g" generated_configs/champsim_config-$config.json
                                                            
                                                            echo "Compiling ---> $config"
                                                            ./config.sh generated_configs/champsim_config-$config.json
                                                            make -j > /dev/null 2>&1
                                                        done
                                                    done
                                                done
                                            done
                                        done
                                    done
                                done
                            done
                        done
                    done
                done
            done
        done
    done
done