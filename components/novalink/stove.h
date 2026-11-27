#pragma once

// What the stove readings mean: state codes and display text. Plain C++, tested on a computer.

#include <cstddef>
#include <cstdint>

namespace esphome::novalink {

static constexpr uint8_t MAX_STATE_CODE = 10;
static constexpr uint32_t ALARM_STATES = (1UL << 8) | (1UL << 9) | (1UL << 10);

/// Codes above 10 are communication glitches now and then (223 was read): accepted only when read twice in a row.
class StateFilter {
 public:
  bool accept(uint8_t code) {
    const bool ok = code <= MAX_STATE_CODE || (this->has_last_ && this->last_ == code);
    this->last_ = code;
    this->has_last_ = true;
    return ok;
  }
  void reset() { this->has_last_ = false; }

 protected:
  uint8_t last_{0};
  bool has_last_{false};
};

/// Text of the stove display, read one character at a time while the stove alternates its messages. A text counts only
/// when two passes in a row are identical. The messages seen during a window are put together: "A / B".
class DisplayReader {
 public:
  static constexpr size_t SIZE = 16;
  static constexpr size_t LINE = 8;  ///< the display has two lines of 8 characters
  static constexpr size_t MAX_TEXTS = 3;
  static constexpr size_t TEXT_SIZE = SIZE + 2;  ///< lines joined by a space, and the end
  static constexpr uint8_t MAX_FAILURES = 5;
  enum class Result : uint8_t {
    IDLE,       ///< not reading
    WAIT,       ///< the pass is not complete
    NEXT_PASS,  ///< read the 16 characters again
    DONE,       ///< window over, see text_count()
  };

  uint32_t window{5000};  ///< ms

  void start(uint32_t now);
  void stop() { this->active_ = false; }
  bool active() const { return this->active_; }
  Result on_char(size_t index, uint8_t value, uint32_t now);
  Result on_failure(uint32_t now);

  size_t text_count() const { return this->count_; }
  /// Writes the texts in alphabetical order, so that the result does not depend on the start. Returns the length.
  size_t join(char *out, size_t size) const;
  static constexpr size_t JOINED_SIZE = MAX_TEXTS * (TEXT_SIZE + 3);

 protected:
  void add_text_();

  uint8_t pass_[SIZE]{};
  uint8_t previous_[SIZE]{};
  char texts_[MAX_TEXTS][TEXT_SIZE]{};
  size_t count_{0};
  uint32_t start_{0};
  uint16_t received_{0};
  uint8_t failures_{0};
  bool has_previous_{false};
  bool active_{false};
};

}  // namespace esphome::novalink
