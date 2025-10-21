#include "ooo_cpu.h"

struct indirect_entry {
  uint64_t target;
  uint64_t taken_bb_size;
  uint64_t not_taken_bb_size;
};

template<uint32_t INDIRECT_SIZE>
class H2PIndirect {

private:

  indirect_entry indirect[INDIRECT_SIZE];
  indirect_entry speculative_indirect[INDIRECT_SIZE];
  uint64_t conditional_history;
  uint64_t speculative_conditional_history;
  bool speculative_state;

  uint64_t indirect_hash(uint64_t ip) {
    return (((ip >> 2) ^ (conditional_history)) & (INDIRECT_SIZE - 1));
  }

public:

  void initialize() {
    std::cout << "Indirect buffer size: " << INDIRECT_SIZE << std::endl;
    for (uint32_t i = 0; i < INDIRECT_SIZE; i++) {

      indirect[i].target = 0;
      indirect[i].taken_bb_size = 0;
      indirect[i].not_taken_bb_size = 0;

      speculative_indirect[i].target = 0;
      speculative_indirect[i].taken_bb_size = 0;
      speculative_indirect[i].not_taken_bb_size = 0;
    }
    conditional_history = 0;
    speculative_conditional_history = 0;
    speculative_state = false;
  }

  uint64_t predict(uint64_t ip) {
    // if (ip == 0xffffb307e718) {
    //   cout << "Looking for target " << std::hex << ip << " target " << indirect[indirect_hash(ip)].target << std::dec << " index " << indirect_hash(ip) << endl;
    // }
    return indirect[indirect_hash(ip)].target;
  }

  void update(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
      // if (ip == 0xffffb307e718) {
      //   cout << "Saved target " << std::hex << ip << " target " << branch_target << std::dec << " index " << indirect_hash(ip) << endl;
      // }
      indirect[indirect_hash(ip)].target = branch_target;
    }
    if (branch_type == BRANCH_CONDITIONAL) {
      conditional_history <<= 1;
      if (taken) {
        conditional_history |= 1;
      }
    }
  }

  std::pair<uint64_t, uint64_t> get_bb_size(uint64_t ip) {
    return std::pair{indirect[indirect_hash(ip)].taken_bb_size, indirect[indirect_hash(ip)].not_taken_bb_size};
  }

  void m_update_bb_size(uint64_t ip, uint64_t bb_size, uint8_t branch_type, bool taken, uint64_t global_history) {
    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
      if (taken) {
        indirect[indirect_hash(ip)].taken_bb_size = bb_size;
      } else {
        indirect[indirect_hash(ip)].not_taken_bb_size = bb_size;
      }
    }
  }

  void speculative_begin() {
    assert(!speculative_state);
    speculative_state = true;
    speculative_conditional_history = conditional_history;
    for (uint32_t i = 0; i < INDIRECT_SIZE; i++) {
      speculative_indirect[i] = indirect[i];
    }
  }

  void speculative_end() {
    assert(speculative_state);
    speculative_state = false;
    conditional_history = speculative_conditional_history;
    for (uint32_t i = 0; i < INDIRECT_SIZE; i++) {
      indirect[i] = speculative_indirect[i];
    }
  }
};
