#include "ooo_cpu.h"

template<uint32_t RAS_SIZE, uint32_t CALL_INSTR_SIZE_TRACKERS>
class H2P_RAS {

private:

  uint64_t ras[RAS_SIZE];
  int ras_index;

  uint64_t save_ras[RAS_SIZE];
  int save_ras_index;
  bool speculative_state = false;
  /*
   * The following two variables are used to automatically identify the
   * size of call instructions, in bytes, which tells us the appropriate
   * target for a call's corresponding return.
   * They exist because ChampSim does not model a specific ISA, and
   * different ISAs could use different sizes for call instructions,
   * and even within the same ISA, calls can have different sizes.
   */
  uint64_t call_instr_sizes[CALL_INSTR_SIZE_TRACKERS];
  uint64_t save_call_instr_sizes[CALL_INSTR_SIZE_TRACKERS];

  uint64_t abs_addr_dist(uint64_t addr1, uint64_t addr2) {
    if (addr1 > addr2) {
      return addr1 - addr2;
    }
    return addr2 - addr1;
  }

  void push_ras(uint64_t ip) {
    ras_index++;
    if (ras_index == RAS_SIZE) {
      ras_index = 0;
    }
    ras[ras_index] = ip;
  }

  uint64_t peek_ras() {
    return ras[ras_index];
  }

  uint64_t pop_ras() {
    uint64_t target = ras[ras_index];
    ras[ras_index] = 0;
    ras_index--;
    if (ras_index == -1) {
      ras_index += RAS_SIZE;
    }
    return target;
  }

  uint64_t call_size_tracker_hash(uint64_t ip) {
    return (ip & (CALL_INSTR_SIZE_TRACKERS - 1));
  }

  uint64_t get_call_size(uint64_t ip) {
    return call_instr_sizes[call_size_tracker_hash(ip)];
  }

public:

  void initialize() {
    std::cout << "RAS size: " << RAS_SIZE << std::endl;
    for (uint32_t i = 0; i < RAS_SIZE; i++) {
      ras[i] = 0;
      save_ras[i] = 0;
    }
    ras_index = 0;
    for (uint32_t i = 0; i < CALL_INSTR_SIZE_TRACKERS; i++) {
      call_instr_sizes[i] = 4;
      save_call_instr_sizes[i] = 4;
    }
  }

  uint64_t predict() {
    // peek at the top of the RAS
    // and adjust for the size of the call instr
    uint64_t target = peek_ras();
    if (target) target += get_call_size(target);
    //cout << "Predict Target " << std::hex << target << std::dec << endl;
    return target;
  }

  void update(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    //cout << "ALT Update RAS " << ip << " target " << branch_target << " taken " << int(taken) << " type " << int(branch_type) << endl;
    if ((branch_type == BRANCH_DIRECT_CALL) || (branch_type == BRANCH_INDIRECT_CALL)) {
      // add something to the RAS
      push_ras(ip);
      //cout << "push Target " << std::hex << ip << std::dec << " index " << ras_index << endl;
    }
    if (branch_type == BRANCH_RETURN) {
      // recalibrate call-return offset
      // if our return prediction got us into the right ball park, but not the
      // exactly correct byte target, then adjust our call instr size tracker
      uint64_t call_ip = pop_ras();
      //cout << "pop Target " << std::hex << call_ip << std::dec << " index " << ras_index << endl;
      uint64_t estimated_call_instr_size = abs_addr_dist(call_ip, branch_target);
      if (estimated_call_instr_size <= 10) {
        call_instr_sizes[call_size_tracker_hash(call_ip)] = estimated_call_instr_size;
      }
    }
  }

  void speculative_begin() {
    assert(!speculative_state);
    speculative_state = true;
    for (int i = 0; i < RAS_SIZE; i++) {
      save_ras[i] = ras[i];
    }
    save_ras_index = ras_index;
    for (uint32_t i = 0; i < CALL_INSTR_SIZE_TRACKERS; i++) {
      save_call_instr_sizes[i] = call_instr_sizes[i];
    }
    //cout << "Saving at spec begin! index " << save_ras_index << " peek " << std::hex << ras[save_ras_index] << std::dec << endl;
  }

  void speculative_end() {
    assert(speculative_state);
    speculative_state = false;
    for (int i = 0; i < RAS_SIZE; i++) {
      ras[i] = save_ras[i];
    }
    ras_index = save_ras_index;
    for (uint32_t i = 0; i < CALL_INSTR_SIZE_TRACKERS; i++) {
      call_instr_sizes[i] = save_call_instr_sizes[i];
    }
    //cout << "Restoring at spec end! index " << ras_index << " peek " << std::hex << ras[save_ras_index] << std::dec << endl;
  }
};