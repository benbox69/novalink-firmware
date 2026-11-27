#pragma once

#include <cstdint>

#include "esphome/core/automation.h"

#include "novalink.h"

namespace esphome::novalink {

/// `novalink.write`: free write from an automation. Needs `allow_custom_writes: true`.
template<typename... Ts> class WriteAction final : public Action<Ts...>, public Parented<NovaLink> {
 public:
  TEMPLATABLE_VALUE(uint8_t, value)
  void set_location(uint8_t location) { this->location_ = location; }
  void set_address(uint8_t address) { this->address_ = address; }

 protected:
  void play(const Ts &...x) override {
    this->parent_->write_custom(this->location_, this->address_, this->value_.value(x...), false);
  }
  uint8_t location_{0};
  uint8_t address_{0};
};

}  // namespace esphome::novalink
