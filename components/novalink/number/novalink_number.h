#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/novalink/stove_entity.h"
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::novalink {

class NovaLink;

/// A stove setting (setpoint, power level, or a custom address). Publishes only what the stove holds: a refused,
/// delayed or failed write leaves the former value.
class NovaLinkNumber final : public number::Number, public StoveEntity {
 public:
  /// value shown = (raw - offset) × multiplier
  void set_conversion(float multiplier, uint8_t offset) {
    this->multiplier_ = multiplier;
    this->offset_ = offset;
  }
  void set_custom(bool custom) { this->custom_ = custom; }
  /// Some boards apply the power level only once its RAM copy is written too.
  void set_mirror(uint8_t location, uint8_t address) {
    this->has_mirror_ = true;
    this->mirror_location_ = location;
    this->mirror_address_ = address;
  }
  void on_unknown() override { this->publish_state(NAN); }
  void on_write_result(const WriteRequest &request, WriteResult result) override;

 protected:
  void control(float value) override;
  void on_value(uint8_t address, uint8_t value) override;
  /// Raw value, or -1 when it does not fit in a byte.
  int to_raw_(float value) const;

  float multiplier_{1.0f};
  uint8_t offset_{0};
  bool custom_{false};
  bool has_mirror_{false};
  uint8_t mirror_location_{0};
  uint8_t mirror_address_{0};
};

/// A temperature of the protection (maximum, fallback stop, fallback setpoint), kept in flash.
class NovaLinkProtectionNumber final : public number::Number, public Component {
 public:
  enum Kind : uint8_t { MAXIMUM, FALLBACK_STOP, FALLBACK_SETPOINT };
  void set_hub(NovaLink *hub) { this->hub_ = hub; }
  void set_kind(Kind kind) { this->kind_ = kind; }
  void set_initial_value(float value) { this->initial_value_ = value; }
  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;
  void apply_(float value);
  ESPPreferenceObject pref_;
  NovaLink *hub_{nullptr};
  float initial_value_{0};
  Kind kind_{MAXIMUM};
};

/// Written by the Home Assistant integration every few minutes (room temperature): proves it is alive.
class NovaLinkHeartbeatNumber final : public number::Number {
 public:
  void set_hub(NovaLink *hub) { this->hub_ = hub; }

 protected:
  void control(float value) override;
  NovaLink *hub_{nullptr};
};

}  // namespace esphome::novalink
