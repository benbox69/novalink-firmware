#pragma once

#include <cstdint>

#include "esphome/components/button/button.h"
#include "esphome/components/novalink/novalink.h"

namespace esphome::novalink {

/// Writes the whole stove memory to the log (read only).
class NovaLinkDumpButton final : public button::Button {
 public:
  void set_hub(NovaLink *hub) { this->hub_ = hub; }

 protected:
  void press_action() override { this->hub_->start_dump(); }
  NovaLink *hub_{nullptr};
};

/// Free write of one value: a command, sent once (the address may not keep the value).
class NovaLinkCustomButton final : public button::Button {
 public:
  void set_hub(NovaLink *hub) { this->hub_ = hub; }
  void set_write(uint8_t location, uint8_t address, uint8_t value) {
    this->location_ = location;
    this->address_ = address;
    this->value_ = value;
  }

 protected:
  void press_action() override { this->hub_->write_custom(this->location_, this->address_, this->value_, false); }
  NovaLink *hub_{nullptr};
  uint8_t location_{0};
  uint8_t address_{0};
  uint8_t value_{0};
};

}  // namespace esphome::novalink
