#pragma once

#include <cstdint>

#include "esphome/components/novalink/stove.h"
#include "esphome/components/novalink/stove_entity.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::novalink {

/// Stove state as a text ("Working", "Standby"...). A change makes the display text read again.
class NovaLinkStateText final : public text_sensor::TextSensor, public StoveEntity {
 public:
  /// On the X100 EVO, states 8 and 9 are general alarms, whatever the cause: both are named "Alarm" (the alarm text
  /// gives the cause). See https://github.com/benbox69/novalink-firmware/wiki/Stove-addresses.
  void set_x100_evo(bool x100_evo) { this->x100_evo_ = x100_evo; }
  void on_unknown() override;

 protected:
  void on_value(uint8_t address, uint8_t value) override;
  StateFilter filter_;
  int16_t last_code_{-1};  ///< published last (the published text may be changed by filters), -1 = none
  bool x100_evo_{false};
};

/// Alarm code as a text. Only the codes seen with the text of the stove display have a name.
class NovaLinkAlarmText final : public text_sensor::TextSensor, public StoveEntity {
 public:
  void on_unknown() override {
    this->last_code_ = -1;
    this->publish_state("unknown");
  }

 protected:
  void on_value(uint8_t address, uint8_t value) override;
  int16_t last_code_{-1};
};

/// Text of the stove display (16 addresses), see DisplayReader.
class NovaLinkDisplayText final : public text_sensor::TextSensor, public StoveEntity {
 public:
  void set_window(uint32_t window) { this->reader_.window = window; }
  void update() override;
  bool matches(uint8_t location, uint8_t address) const override {
    return location == this->location_ && address >= this->address_ &&
           address < this->address_ + DisplayReader::SIZE;
  }
  void on_unknown() override;

 protected:
  void on_value(uint8_t address, uint8_t value) override;
  void on_failure() override;
  void next_(DisplayReader::Result result);
  void request_pass_();
  DisplayReader reader_;
  char last_text_[DisplayReader::JOINED_SIZE]{};
};

}  // namespace esphome::novalink
