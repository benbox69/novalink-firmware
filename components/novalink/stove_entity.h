#pragma once

#include <cstdint>

#include "esphome/core/component.h"

#include "protocol.h"

namespace esphome::novalink {

class NovaLink;

/// Base of the entities that show a stove address. On each update it asks the hub for a read; the hub hands it the
/// reply (handle_value), a failed read (handle_failure), or tells it the stove is silent (on_unknown).
class StoveEntity : public PollingComponent {
 public:
  void set_hub(NovaLink *hub) { this->hub_ = hub; }
  void set_address(uint8_t location, uint8_t address) {
    this->location_ = location;
    this->address_ = address;
  }
  void update() override;
  void dump_config() override;

  /// True when a reply for this address belongs to this entity. The display text covers 16 addresses.
  virtual bool matches(uint8_t location, uint8_t address) const {
    return location == this->location_ && address == this->address_;
  }
  void handle_value(uint8_t address, uint8_t value);
  void handle_failure();
  /// The stove does not answer: show "unknown", never a stale value.
  virtual void on_unknown() = 0;
  /// A write to the address of this entity is over.
  virtual void on_write_result(const WriteRequest &request, WriteResult result) {}

 protected:
  virtual void on_value(uint8_t address, uint8_t value) = 0;
  virtual void on_failure() {}

  /// Failed reads in a row before the entity shows "unknown".
  static constexpr uint8_t MAX_FAILURES = 2;

  NovaLink *hub_{nullptr};
  uint8_t location_{0};
  uint8_t address_{0};
  uint8_t failures_{0};
};

}  // namespace esphome::novalink
