#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/novalink/stove.h"
#include "esphome/components/novalink/stove_entity.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::novalink {

/// A number read from the stove: raw value × multiplier, plus an offset when it is not 0 (speed of a running fan).
class NovaLinkSensor final : public sensor::Sensor, public StoveEntity {
 public:
  void set_multiplier(float multiplier) { this->multiplier_ = multiplier; }
  void set_running_offset(float offset) { this->running_offset_ = offset; }
  void on_unknown() override { this->publish_state(NAN); }

 protected:
  void on_value(uint8_t address, uint8_t value) override {
    this->publish_state(value == 0 ? 0.0f : value * this->multiplier_ + this->running_offset_);
  }
  float multiplier_{1.0f};
  float running_offset_{0.0f};
};

/// Stove state code, without the communication glitches (see StateFilter).
class NovaLinkStateSensor final : public sensor::Sensor, public StoveEntity {
 public:
  void on_unknown() override {
    this->filter_.reset();
    this->publish_state(NAN);
  }

 protected:
  void on_value(uint8_t address, uint8_t value) override {
    if (this->filter_.accept(value))
      this->publish_state(value);
  }
  StateFilter filter_;
};

}  // namespace esphome::novalink
