#pragma once

// Micronova stove protocol core. Plain C++ without ESPHome dependency, so that it can be tested on a computer.

#include <cstddef>
#include <cstdint>

#if __has_include("esphome/core/defines.h")
#include "esphome/core/defines.h"  // NOVALINK_MAX_READS, the same in every file
#endif

namespace esphome::novalink {

static constexpr uint8_t LOCATION_RAM = 0x00;
static constexpr uint8_t LOCATION_EEPROM = 0x20;
static constexpr uint8_t WRITE_BIT = 0x80;

#ifndef NOVALINK_MAX_READS
#define NOVALINK_MAX_READS 64
#endif
static constexpr size_t MAX_READS = NOVALINK_MAX_READS;
static constexpr size_t MAX_WRITES = 8;
static constexpr size_t MAX_RULES = 16;
static constexpr size_t MAX_FRAME = 4;
static constexpr size_t REPLY_SIZE = 2;

/// Checksum of a frame: sum of the bytes, kept to 8 bits.
uint8_t frame_checksum(const uint8_t *data, size_t len);
/// Reads refused: write codes, and addresses that the stove would take for a write code if a byte was lost.
bool is_readable(uint8_t location, uint8_t address);

/// Byte access to the stove line.
class Transport {
 public:
  virtual ~Transport() = default;
  virtual size_t available() = 0;
  virtual uint8_t read_byte() = 0;
  /// Send a frame. With a reception gate, the implementation opens reception again when the frame has left.
  virtual void send(const uint8_t *data, size_t len) = 0;
  /// The exchange is over (reception may be gated until the next one).
  virtual void release() {}
};

/// Condition on the value read just before a write.
struct Guard {
  enum Type : uint8_t { NONE, RANGE, STATES };
  Type type{NONE};
  uint8_t min{0};
  uint8_t max{0xFF};
  uint32_t states{0};  ///< bit n set: value n is accepted

  static Guard range(uint8_t min, uint8_t max) { return {RANGE, min, max, 0}; }
  static Guard in_states(uint32_t states) { return {STATES, 0, 0xFF, states}; }
  bool accepts(uint8_t value) const;
};

struct WriteRequest {
  uint8_t location{LOCATION_RAM};  ///< without the write bit
  uint8_t address{0};
  uint8_t value{0};
  Guard guard{};
  bool verify{true};     ///< the address must read back as `value` (false for commands such as on/off)
  bool retry{false};     ///< one new attempt if the read back differs (never for commands)
  bool custom{false};    ///< free write asked by the user
  uint8_t attempts{0};   ///< internal
};

enum class WriteResult : uint8_t {
  ACCEPTED,    ///< queued, the final result comes through Listener::on_write_result
  DONE,        ///< written and confirmed
  UNCHANGED,   ///< the stove already holds this value, nothing written
  SUPERSEDED,  ///< replaced by a newer request for the same address
  REFUSED_READ_ONLY,
  REFUSED_BLOCKED,
  REFUSED_NOT_ALLOWED,
  REFUSED_ADDRESS,
  REFUSED_GUARD,  ///< value read before the write not plausible, or stove not in the right state
  REFUSED_LIMIT,  ///< too many EEPROM writes in 24 h
  QUEUE_FULL,
  FAILED_READ,  ///< the address could not be read before the write: nothing written
  FAILED,       ///< sent, but not confirmed by the stove
};

const char *write_result_to_string(WriteResult result);

class Listener {
 public:
  virtual ~Listener() = default;
  virtual void on_value(uint8_t location, uint8_t address, uint8_t value) = 0;
  virtual void on_read_failed(uint8_t location, uint8_t address) = 0;
  virtual void on_write_result(const WriteRequest &request, WriteResult result) = 0;
  virtual void on_link(bool up) = 0;
};

struct BusConfig {
  bool echo{true};
  bool read_only{false};
  bool allow_custom_writes{false};
  uint32_t reply_timeout{200};           ///< ms, for the echo and for the reply
  uint32_t exchange_gap{10};             ///< ms of silence between two exchanges
  uint32_t recovery_delay{100};          ///< ms of silence after a failed exchange
  uint32_t eeprom_write_interval{5000};  ///< ms between two EEPROM writes
  uint16_t eeprom_daily_limit{50};       ///< EEPROM writes per 24 h, 0 = no limit
  uint8_t read_retries{2};
  uint8_t link_down_after{5};  ///< consecutive failed exchanges
};

class Bus {
 public:
  Bus(Transport *transport, Listener *listener) : transport_(transport), listener_(listener) {}

  BusConfig config{};

  bool allow_write(uint8_t location, uint8_t address);
  bool block_write(uint8_t location, uint8_t address);

  /// Queue a read. Returns false if it is refused or if the queue is full; a read already queued is accepted.
  bool queue_read(uint8_t location, uint8_t address);
  WriteResult queue_write(const WriteRequest &request);
  /// To be called often; `now` in ms.
  void loop(uint32_t now);

  bool link_up() const { return this->link_ == Link::UP; }
  bool idle() const { return this->task_ == Task::NONE && this->read_count_ == 0 && this->write_count_ == 0; }
  /// EEPROM writes sent since boot
  uint32_t eeprom_writes() const { return this->eeprom_writes_; }
  size_t pending_reads() const { return this->read_count_; }

 protected:
  struct Key {
    uint8_t location;
    uint8_t address;
  };
  enum class Phase : uint8_t { IDLE, ECHO, REPLY, RECOVER };
  enum class Task : uint8_t { NONE, READ, BEFORE_WRITE, WRITE, AFTER_WRITE };
  enum class Link : uint8_t { UNKNOWN, UP, DOWN };

  static bool is_eeprom_(uint8_t location) { return location != LOCATION_RAM; }
  static bool contains_(const Key *list, size_t count, uint8_t location, uint8_t address);

  void start_next_();
  bool eeprom_limit_reached_() const;
  bool take_write_();
  void start_read_(Task task, uint8_t location, uint8_t address);
  void send_write_();
  void send_frame_(const uint8_t *data, size_t len);
  void receive_();
  void exchange_failed_();
  void exchange_done_(uint8_t value);
  void recover_();
  void finish_write_(WriteResult result);
  void retry_or_fail_write_();
  void set_link_(bool up);

  Transport *transport_;
  Listener *listener_;

  Key allowed_[MAX_RULES]{};
  size_t allowed_count_{0};
  Key blocked_[MAX_RULES]{};
  size_t blocked_count_{0};

  Key reads_[MAX_READS]{};
  size_t read_head_{0};
  size_t read_count_{0};
  WriteRequest writes_[MAX_WRITES]{};
  size_t write_count_{0};

  uint32_t now_{0};
  uint32_t phase_start_{0};
  uint32_t last_exchange_{0};
  bool had_exchange_{false};
  uint32_t last_eeprom_write_{0};
  bool eeprom_written_{false};
  bool eeprom_window_active_{false};
  uint16_t eeprom_window_count_{0};
  uint32_t eeprom_window_start_{0};
  uint32_t eeprom_writes_{0};
  Phase phase_{Phase::IDLE};
  Task task_{Task::NONE};
  Link link_{Link::UNKNOWN};
  uint8_t failures_{0};
  uint8_t retries_left_{0};

  Key current_{};
  WriteRequest write_{};
  uint8_t frame_[MAX_FRAME]{};
  size_t frame_len_{0};
  size_t received_{0};
  uint8_t reply_[REPLY_SIZE]{};
};

}  // namespace esphome::novalink
