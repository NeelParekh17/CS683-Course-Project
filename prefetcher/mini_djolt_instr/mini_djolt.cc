#include "cache.h"
#include "instruction.h"

#include <array>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory>
namespace { // anonymous

  // ============================================================
  //  mini D-JOLT parameters. (24.9 KiB)
  // ============================================================
  static constexpr size_t UpperBitPtrBits = 5; // This determines that the number of upper bit variations that D-JOLT can handle.
  static constexpr size_t UpperBitMask = 0xffffffffffe00000; // This defines where upper bit is.

  static constexpr size_t SignatureBits = 16;
  static constexpr size_t SignatureHashParam = 5; // A good parameter choice is that SignatureBits and SignatureHashParam are coprime.

#define LongRangePrefetcherSiggen Siggen_FifoRetCnt<1>
  static constexpr size_t LongRangePrefetcherDistance = 15;
  static constexpr size_t LongRangePrefetcher_N_Sets = 512;
  static constexpr size_t LongRangePrefetcher_N_Ways = 4;
  static constexpr size_t LongRangePrefetcher_N_Vectors = 2;
  static constexpr size_t LongRangePrefetcher_VectorSize = 8;
  static constexpr size_t LongRangePrefetcher_TagBits = 7;
  // -------------------------------------
  //  miss table of long-range prefetcher
  // -------------------------------------
  // 4 way, 512 Sets (2048 entry)
  // Budget:
  //  Signature Tag :       (16 - 9) bits X 2048 entry =  14336 bits
  //  miss vector : (5 + 15 + 8) bits X 2 X 2048 entry = 114688 bits
  //     compressed upper address :  5 bits
  //     lower address            : 15 bits
  //     bit vector               :  8 bits
  //  lru bit :                    2 bits X 2048 entry =   4096 bits
  //                                               Total 133120 bits

  // ----------------------------------------------
  //  signature generator of long-range prefetcher
  // ----------------------------------------------
  // Budget:
  //  1 entry Queue
  //   address : 32 bits X 1 entry = 32 bits
  //   head pointer :                 0 bits
  //   return counter :              32 bits
  //                           Total 64 bits

  // ----------------------------------------------
  //  signature queue of long-range prefetcher
  // ----------------------------------------------
  // Budget:
  //  15 entry Queue
  //   signature : 16 bits X 15 entry = 240 bits
  //   head pointer :                     4 bits
  //                              Total 244 bits

#define ShortRangePrefetcherSiggen Siggen_FifoRetCnt<1>
  static constexpr size_t ShortRangePrefetcherDistance = 4;
  static constexpr size_t ShortRangePrefetcher_N_Sets = 128;
  static constexpr size_t ShortRangePrefetcher_N_Ways = 4;
  static constexpr size_t ShortRangePrefetcher_N_Vectors = 2;
  static constexpr size_t ShortRangePrefetcher_VectorSize = 8;
  static constexpr size_t ShortRangePrefetcher_TagBits = 9;
  // --------------------------------------
  //  miss table of short-range prefetcher
  // --------------------------------------
  // 4 way, 128 Sets (512 entry)
  // Budget:
  //  Signature Tag :       (16 - 7) bits X 512 entry =  4608 bits
  //  miss vector : (5 + 15 + 8) bits X 2 X 512 entry = 28672 bits
  //     compressed upper address :  5 bits
  //     lower address            : 15 bits
  //     bit vector               :  8 bits
  //  lru bit :                    2 bits X 512 entry =  1024 bits
  //                                              Total 34304 bits

  // ----------------------------------------------
  //  signature generator of short-range prefetcher
  // ----------------------------------------------
  // Budget:
  //  1 entry Queue
  //   address : 32 bits X 4 entry = 32 bits
  //   head pointer :                 0 bits
  //   return counter :              32 bits
  //                           Total 64 bits

  // ----------------------------------------------
  //  signature queue of short-range prefetcher
  // ----------------------------------------------
  // Budget:
  //  4 entry Queue
  //   signature : 16 bits X 4 entry = 64 bits
  //   head pointer :                   2 bits
  //                             Total 66 bits

  static constexpr size_t ExtraMissTable_N_Sets = 128;
  static constexpr size_t ExtraMissTable_N_Ways = 4;
  static constexpr size_t ExtraMissTable_N_Vectors = 2;
  static constexpr size_t ExtraMissTable_VectorSize = 8;
  static constexpr size_t ExtraMissTable_TagBits = 9;
  // ---------------------
  //  extra miss table
  // ---------------------
  // 4 way, 128 Sets (512 entry)
  // Budget:
  //  signature Tag :         (16 - 7) bits X 512 entry =  4608 bits
  //  miss vector :   (5 + 15 + 8) bits X 2 X 512 entry = 28672 bits
  //     compressed upper address :  5 bits
  //     lower address            : 15 bits
  //     bit vector               :  8 bits
  //  lru bit :                      2 bits X 512 entry =  1024 bits
  //                                                Total 34304 bits

  // ============================================================

  // This is a compressed expression of upper bit.
  struct UpperBitPtr {
    size_t ptr;
    bool operator==(const UpperBitPtr& rhs) const noexcept {
      return ptr == rhs.ptr;
    }
    bool operator!=(const UpperBitPtr& rhs) const noexcept {
      return ptr != rhs.ptr;
    }
  };

  // This is a compressed representation of line address.
  struct CompressedLineAddress {
    UpperBitPtr upper_part;
    uint64_t lower_part;
    bool isValid() const noexcept {
      return upper_part.ptr != 0;
    }
  };

  // This table records the correspondence between the compressed expression and the original expression.
  class UpperBitTable {
    struct Entry {
      bool valid; uint64_t upper_bits;
    };
    std::array<Entry, (1ull << UpperBitPtrBits) - 1> table = {};
  public:
    std::pair<bool, CompressedLineAddress> compress(uint64_t full_address) {
      const uint64_t upper_bits = full_address & UpperBitMask;
      const uint64_t lower_bits = (full_address & ~UpperBitMask) >> LOG2_BLOCK_SIZE;

      const auto exists_pos = std::find_if(table.begin(), table.end(), [upper_bits](const Entry& e) noexcept { return e.valid && upper_bits == e.upper_bits; });
      const bool entry_exists = exists_pos != table.end();

      if (entry_exists) {
        return { true, { static_cast<size_t>(exists_pos - table.begin()) + 1, lower_bits } };
      } else {
        const auto invalid_pos = std::find_if(table.begin(), table.end(), [](const Entry& e) noexcept { return !e.valid; });
        const bool invalid_entry_found = invalid_pos != table.end();

        if (invalid_entry_found) {
          (*invalid_pos) = { true, upper_bits };
          return { true, { static_cast<size_t>(invalid_pos - table.begin()) + 1, lower_bits } };
        } else {
          return { false, {} };
        }
      }
    }

    uint64_t decompress(CompressedLineAddress cla) const {
      return table.at(cla.upper_part.ptr - 1).upper_bits + (cla.lower_part << LOG2_BLOCK_SIZE);
    }
  };

  // ---------------------
  //  upper bit table
  // ---------------------
  // 31 Sets Fully-asociative table
  // Budget:
  //  upper bit : 43 bits X 31 entry = 1333 bits
  //  Valid :      1 bit  X 31 entry =   31 bits
  //                             Total 1364 bits

  // utility functions

  template<size_t N>
  std::array<size_t, N> make_initialized_lru_order() {
    std::array<size_t, N> ret;
    std::iota(ret.begin(), ret.end(), 0);
    return ret;
  }

  template<class T>
  void update_lru_order(T& lru_order, size_t touch_pos) noexcept {
    assert(touch_pos < lru_order.size());
    for (auto& e : lru_order) {
      if (e < lru_order.at(touch_pos)) {
        ++e;
      }
    }
    lru_order.at(touch_pos) = 0;
  }

  template<size_t HistLen>
  class Siggen_FifoRetCnt {
    std::array<uint32_t, HistLen> ghist = {};
    size_t head = 0;
    uint32_t return_count = 0;

    uint32_t makeSig() const noexcept {
      uint32_t sig = 0;
      for (size_t i = head; i < head + HistLen; ++i) {
        const uint32_t pc = ghist.at(i % HistLen);
        sig = (sig << (SignatureBits - SignatureHashParam)) | (sig >> SignatureHashParam);
        sig ^= pc ^ pc >> 2; // work well on both A64/x86
        sig &= ((1ull << SignatureBits) - 1);
      }
      sig ^= return_count * 0xabcd;
      return sig & ((1ull << SignatureBits) - 1);
    }
  public:
    uint32_t onReturnInstruction(uint64_t, uint64_t) {
      ++return_count;
      return makeSig();
    }
    uint32_t onCallInstruction(uint64_t ip, uint64_t) {
      return_count = 0;
      ghist.at(head) = ip;
      head = (head + 1) % HistLen;
      return makeSig();
    }
  };

  template<size_t VectorSize>
  class MissInfo {
    CompressedLineAddress base_address = {};
    std::array<bool, VectorSize> bit_vector = {};
  public:
    bool add(CompressedLineAddress address) noexcept {
      if (base_address.isValid()) {
        if (base_address.upper_part.ptr != address.upper_part.ptr) {
          return false;
        }
        const int64_t diff = address.lower_part - base_address.lower_part;
        if (diff < 0) {
          return false;
        } else if (diff == 0) {
          return true;
        } else if (static_cast<size_t>(diff - 1) < bit_vector.size()) {
          bit_vector.at(diff - 1) = true;
          return true;
        } else {
          return false;
        }
      } else {
        base_address = address;
        return true;
      }
    }
    bool isValid() const noexcept {
      return base_address.isValid();
    }
    std::vector<CompressedLineAddress> getAddresses() const {
      assert(isValid());
      std::vector<CompressedLineAddress> ret;
      ret.push_back(base_address);
      for (size_t i = 0; i < bit_vector.size(); ++i) {
        if (bit_vector.at(i)) {
          CompressedLineAddress tmp = base_address; tmp.lower_part += (i + 1); ret.push_back(tmp);
        }
      }
      return ret;
    }
  };

  template<size_t N_Vectors, size_t VectorSize>
  class MissTableEntry {
    std::array<MissInfo<VectorSize>, N_Vectors> elems = {};
  public:
    bool insert_but_do_not_evict(CompressedLineAddress address) {
      for (size_t i = 0; i < N_Vectors; ++i) {
        const bool success = elems.at(i).add(address);
        if (success) {
          return true;
        }
      }
      return false;
    }
    std::vector<MissInfo<VectorSize>> getValidEntries() const {
      std::vector<MissInfo<VectorSize>> ret;
      for (const auto& e : elems) {
        if (e.isValid()) {
          ret.push_back(e);
        }
      }
      return ret;
    }
  };

  template<size_t N_Ways, class T, class U, class Hasher>
  class FullyAssociativeLRUTable {
    struct Entry {
      size_t tag;
      U value;
      bool valid;
      Entry() : tag(0), value(), valid(false) {}
    };
    std::array<Entry, N_Ways> table = {};
    std::array<size_t, N_Ways> lru_order = make_initialized_lru_order<N_Ways>();

    size_t find_index_of(const T& key) const {
      const size_t tag = Hasher{}(key);
      return std::find_if(table.begin(), table.end(), [tag](const Entry& entry) noexcept { return entry.valid && entry.tag == tag; }) - table.begin();
    }
  public:
    const U& operator[](const T& key) const {
      assert(contains(key));
      return table.at(find_index_of(key)).value;
    }
    U& operator[](const T& key) {
      assert(contains(key));
      return table.at(find_index_of(key)).value;
    }

    void touch(const T& key) {
      assert(contains(key));
      update_lru_order(lru_order, find_index_of(key));
    }
    void insert(const T& key, const U& elem) {
      if (contains(key)) {
        const size_t index = find_index_of(key);
        table.at(index).value = elem;
        touch(key);
      } else {
        const size_t victim_index = std::max_element(lru_order.begin(), lru_order.end()) - lru_order.begin();
        table.at(victim_index).tag = Hasher{}(key);
        table.at(victim_index).value = elem;
        table.at(victim_index).valid = true;
        touch(key);
      }
    }
    bool contains(const T& key) const {
      return find_index_of(key) != N_Ways;
    }
  };

  template<size_t N_Sets, size_t N_Ways, class T, class U, class Hasher>
  class SetAssociativeLRUTable {
    struct HasherForIndex {
      size_t operator()(const T& key) const {
        return Hasher{}(key) % N_Sets;
      }
    };
    struct HasherForTag {
      size_t operator()(const T& key) const {
        return Hasher{}(key) / N_Sets;
      }
    };
    using Entry = FullyAssociativeLRUTable<N_Ways, T, U, HasherForTag>;
    std::array<Entry, N_Sets> table = {};
  public:
    const U& operator[](const T& key) const {
      return table.at(HasherForIndex{}(key))[key];
    }
    U& operator[](const T& key) {
      return table.at(HasherForIndex{}(key))[key];
    }
    void touch(const T& key) {
      table.at(HasherForIndex{}(key)).touch(key);
    }
    void insert(const T& key, const U& elem) {
      table.at(HasherForIndex{}(key)).insert(key, elem);
    }
    bool contains(const T& key) const {
      return table.at(HasherForIndex{}(key)).contains(key);
    }
  };

  template<size_t N>
  class SignatureQueue {
    std::array<uint32_t, N> queue = {};
    size_t head = 0;
  public:
    uint32_t back() const noexcept {
      return queue.at((head + 1) % N);
    }
    void insert(uint32_t x) noexcept {
      queue.at((head + 1) % N) = x; head = (head + 1) % N;
    }
  };


  class D_JOLT_PREFETCHER {
    CACHE* m_cache;
    struct SigHasher {
      size_t operator()(const uint32_t& x) const noexcept {
        return x;
      }
    };

    ShortRangePrefetcherSiggen siggen_1 = {};
    LongRangePrefetcherSiggen siggen_2 = {};

    SignatureQueue<ShortRangePrefetcherDistance> sig_history_1 = {};
    SignatureQueue<LongRangePrefetcherDistance> sig_history_2 = {};

    SetAssociativeLRUTable<ShortRangePrefetcher_N_Sets, ShortRangePrefetcher_N_Ways, uint32_t, MissTableEntry<ShortRangePrefetcher_N_Vectors, ShortRangePrefetcher_VectorSize>, SigHasher> miss_table_1 = {};
    SetAssociativeLRUTable<LongRangePrefetcher_N_Sets, LongRangePrefetcher_N_Ways, uint32_t, MissTableEntry<LongRangePrefetcher_N_Vectors, LongRangePrefetcher_VectorSize>, SigHasher> miss_table_2 = {};
    SetAssociativeLRUTable<ExtraMissTable_N_Sets, ExtraMissTable_N_Ways, uint32_t, MissTableEntry<ExtraMissTable_N_Vectors, ExtraMissTable_VectorSize>, SigHasher> extra_miss_table = {};

    UpperBitTable upper_bit_table = {};

    static constexpr size_t ConservativeDegree = 2;
    static constexpr size_t AggressiveDegree = 5;
    bool prefetch_issued; // used for determining NL degree
    // ---------------------
    //  fallback prefetcher
    // ---------------------
    // Budget: 1 bit

  public:
    D_JOLT_PREFETCHER(CACHE* m_cache);

    template<class Table>
    void prefetch_with_sig(const Table& table, uint32_t sig);
    template<class Table>
    void learn_with_sig(Table& table, uint32_t sig, CompressedLineAddress c_address);
    void branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target);
    void cache_operate(uint64_t addr, uint8_t cache_hit, uint8_t prefetch_hit);
    void cycle_operate();
    void final_stats();
    void cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr);
  };

  D_JOLT_PREFETCHER::D_JOLT_PREFETCHER(CACHE* m_cache): m_cache(m_cache) {
    std::cout << "L1I D-JOLT instruction prefetcher has been constructed!" << std::endl;
  }

  template<class Table>
  void D_JOLT_PREFETCHER::prefetch_with_sig(const Table& table, uint32_t sig) {
    if (table.contains(sig)) {
      for (const auto& v : table[sig].getValidEntries()) {
        for (const auto& address : v.getAddresses()) {
          const uint64_t pf_addr = upper_bit_table.decompress(address);
          const int success = m_cache->prefetch_line(pf_addr, true, 0);
        }
      }
    }
  }

  void D_JOLT_PREFETCHER::branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {
    uint32_t sig_1;
    uint32_t sig_2;

    if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
      sig_1 = siggen_1.onCallInstruction(ip, branch_target);
      sig_2 = siggen_2.onCallInstruction(ip, branch_target);
    } else if (branch_type == BRANCH_RETURN) {
      sig_1 = siggen_1.onReturnInstruction(ip, branch_target);
      sig_2 = siggen_2.onReturnInstruction(ip, branch_target);
    } else {
      return;
    }

    // Make sure storage limits are adhered to...
    assert(sig_1 < (1ull << ExtraMissTable_TagBits) * ExtraMissTable_N_Sets);
    assert(sig_2 < (1ull << ExtraMissTable_TagBits) * ExtraMissTable_N_Sets);
    assert(sig_1 < (1ull << ShortRangePrefetcher_TagBits) * ShortRangePrefetcher_N_Sets);
    assert(sig_2 < (1ull << LongRangePrefetcher_TagBits) * LongRangePrefetcher_N_Sets);

    sig_history_1.insert(sig_1);
    sig_history_2.insert(sig_2);

    prefetch_issued = false; // reset
    prefetch_with_sig(miss_table_1, sig_1);
    prefetch_with_sig(extra_miss_table, sig_1);
    prefetch_with_sig(miss_table_2, sig_2);
    prefetch_with_sig(extra_miss_table, sig_2);
  }

  template<class Table>
  void D_JOLT_PREFETCHER::learn_with_sig(Table& table, uint32_t sig, CompressedLineAddress c_address) {
    if (!table.contains(sig)) {
      table.insert(sig, {});
    } else {
      table.touch(sig);
    }
    const bool success = table[sig].insert_but_do_not_evict(c_address);
    if (!success) {
      if (!extra_miss_table.contains(sig)) {
        extra_miss_table.insert(sig, {});
      } else {
        extra_miss_table.touch(sig);
      }
      extra_miss_table[sig].insert_but_do_not_evict(c_address);
    } else if (extra_miss_table.contains(sig)) {
      extra_miss_table.touch(sig);
    }
  }

  void D_JOLT_PREFETCHER::cache_operate(uint64_t addr, uint8_t cache_hit, uint8_t prefetch_hit) {
    const bool miss = cache_hit == 0;

    if (miss) {
      // fallback prefetcher (next-k-line prefetcher)
      for (size_t i = 1; i <= (prefetch_issued ? ConservativeDegree : AggressiveDegree); ++i) {
        const uint64_t pf_addr = addr + i * BLOCK_SIZE;
        m_cache->prefetch_line(pf_addr, true, 0);
      }

      const auto compress_result = upper_bit_table.compress(addr);
      const bool compress_success = compress_result.first;
      const CompressedLineAddress c_address = compress_result.second;

      if (!compress_success) {
        assert(false); return;
      }

      learn_with_sig(miss_table_1, sig_history_1.back(), c_address);
      learn_with_sig(miss_table_2, sig_history_2.back(), c_address);
    }
  }

  void D_JOLT_PREFETCHER::cycle_operate()
  {
  }

  void D_JOLT_PREFETCHER::final_stats()
  {
  }

  void D_JOLT_PREFETCHER::cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr)
  {
  }

  std::array<std::unique_ptr<D_JOLT_PREFETCHER>, NUM_CPUS> l1i_prefetcher;

} // namespace anonymous

void CACHE::prefetcher_initialize() {
  ::l1i_prefetcher.at(cpu).reset(new ::D_JOLT_PREFETCHER(this));
}

void CACHE::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {
  ::l1i_prefetcher.at(cpu)->branch_operate(ip, branch_type, branch_target);
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in){
  ::l1i_prefetcher.at(cpu)->cache_operate(ip, cache_hit, type);
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  ::l1i_prefetcher.at(cpu)->cycle_operate();
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  ::l1i_prefetcher.at(cpu)->cache_fill(addr, set, way, prefetch, evicted_addr);
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {
  ::l1i_prefetcher.at(cpu)->final_stats();
}
