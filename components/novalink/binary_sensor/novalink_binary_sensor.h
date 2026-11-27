#pragma once

#include <cstdint>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/novalink/stove.h"
#include "esphome/components/novalink/stove_entity.h"

namespace esphome::novalink {

/// On when the stove state is an alarm (codes 8 to 10).
class NovaLinkAlarmBinarySensor final : public binary_sensor::BinarySensor, public StoveEntity {
 public:
  void on_unknown() override {
    this->filter_.reset();
    this->invalidate_state();
  }

 protected:
  void on_value(uint8_t address, uint8_t value) override {
    if (this->filter_.accept(value))
      this->publish_state(value < 32 && (ALARM_STATES & (1UL << value)) != 0);
  }
  StateFilter filter_;
};

}  // namespace esphome::novalink
