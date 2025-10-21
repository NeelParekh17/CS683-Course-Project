
#ifndef SAT_COUNTER_H
#define SAT_COUNTER_H

#include <iostream>

class SatCounter {

public:
  // Default init to borderline taken
  SatCounter(unsigned num_bits) {
    m_bits_counter = num_bits; m_counter = (unsigned)1 << (num_bits - 1);
  }
  SatCounter(unsigned num_bits, unsigned init) {
    m_bits_counter = num_bits; m_counter = init;
  }
  SatCounter(unsigned num_bits, unsigned init, unsigned conf_level) {
    m_bits_counter = num_bits; m_counter = init; m_conf_level = conf_level;
  }
  ~SatCounter() {}

  // Public Methods
  void resetToWeakTaken() {
    m_counter = 1 << (m_bits_counter - 1);
  }
  void resetToWeakNotTaken() {
    m_counter = (1 << (m_bits_counter - 1)) - 1;
  }
  void resetToZero() {
    m_counter = 0;
  }
  void increment() {
    if (m_counter < (((unsigned)1 << m_bits_counter) - 1)) m_counter++;
  }
  int value() {
    return m_counter;
  }
  void decrement() {
    if (m_counter > 0) m_counter--;
  };
  void shitfRight() {
    m_counter = (m_counter >> 1);
  };
  bool Saturated() {
    return(m_counter == ((unsigned)1 << m_bits_counter) - 1);
  };

  bool getMostSignificantBit() const {
    return (m_counter >> (m_bits_counter - 1));
  }

  bool ConfEnough() const {
    return (m_counter >= m_conf_level);
  }

  bool isWeak() const {
    return (m_counter == ((unsigned)1 << (m_bits_counter - 1)) || m_counter == (((unsigned)1 << (m_bits_counter - 1)) - 1));
  }
  bool isZero() const {
    return (m_counter == 0);
  }

private:
  // Data Members (m_ prefix)
  unsigned m_bits_counter;
  unsigned m_counter;
  unsigned m_conf_level;
};

#endif // SAT_COUNTER_H