#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::novalink {

/// Reads the whole stove memory (RAM then EEPROM, 512 addresses) and writes it to the log, 16 values per line.
/// Read only. Addresses that cannot be read safely are shown as "--".
class MemoryDump {
 public:
  void start();
  bool active() const { return this->index_ != OFF; }
  /// The next address to read, or false when nothing is due (dump off, or waiting for the reply).
  bool next(uint8_t &location, uint8_t &address);
  /// The read of `next()` was queued.
  void sent() { this->waiting_ = true; }
  /// Reply of a read, or NO_VALUE when it failed. Ignored unless it is the address the dump waits for.
  void on_result(uint8_t location, uint8_t address, int16_t value);

  static constexpr int16_t NO_VALUE = -1;

 protected:
  uint8_t location_() const;
  uint8_t address_() const { return this->index_ & 0xFF; }

  static constexpr int16_t OFF = -1;
  static constexpr int16_t SIZE = 512;
  static constexpr size_t LINE = 16;

  int16_t index_{OFF};
  int16_t line_[LINE]{};
  bool waiting_{false};
};

}  // namespace esphome::novalink
