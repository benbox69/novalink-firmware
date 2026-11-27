#pragma once

#include <cstdint>

#include "esphome/components/novalink/stove.h"
#include "esphome/components/novalink/stove_entity.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome::novalink {

class NovaLink;

/// Stove on/off. The state published is the one read from the stove, never assumed.
class NovaLinkStoveSwitch final : public switch_::Switch, public StoveEntity {
 public:
  void set_values(uint8_t on, uint8_t off) {
    this->value_on_ = on;
    this->value_off_ = off;
  }
  // No unknown state for a switch: the link sensor tells
  void on_unknown() override { this->filter_.reset(); }
  void on_write_result(const WriteRequest &request, WriteResult result) override;

 protected:
  void write_state(bool state) override;
  void on_value(uint8_t address, uint8_t value) override;
  StateFilter filter_;
  uint8_t value_on_{0x01};
  uint8_t value_off_{0x06};
};

/// Free write of one of two values to a custom address.
class NovaLinkCustomSwitch final : public switch_::Switch, public StoveEntity {
 public:
  void set_values(uint8_t on, uint8_t off) {
    this->value_on_ = on;
    this->value_off_ = off;
  }
  void on_unknown() override {}

 protected:
  void write_state(bool state) override;
  void on_value(uint8_t address, uint8_t value) override { this->publish_state(value == this->value_on_); }
  uint8_t value_on_{0};
  uint8_t value_off_{0};
};

/// "Protection without NovaLink": what the board does when the integration is lost, on or off.
class NovaLinkFallbackSwitch final : public switch_::Switch, public Component {
 public:
  void set_hub(NovaLink *hub) { this->hub_ = hub; }
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  NovaLink *hub_{nullptr};
};

}  // namespace esphome::novalink
