#include "novalink_number.h"

#include <cmath>

#include "esphome/components/novalink/novalink.h"
#include "esphome/core/log.h"

namespace esphome::novalink {

static const char *const TAG = "novalink";

// ---- Stove setting -----------------------------------------------------------------------------

int NovaLinkNumber::to_raw_(float value) const {
  const long raw = std::lroundf(value / this->multiplier_) + this->offset_;
  return raw < 0 || raw > 0xFF ? -1 : static_cast<int>(raw);
}

void NovaLinkNumber::control(float value) {
  const int raw = this->to_raw_(value);
  if (raw < 0) {
    ESP_LOGW(TAG, "%.1f does not fit in a byte, nothing written", value);
    return;
  }
  WriteRequest request;
  request.location = this->location_;
  request.address = this->address_;
  request.value = raw;
  request.custom = this->custom_;
  if (!this->custom_) {
    // The value in place must look like one of this setting: otherwise the address means something else on this board
    const int low = this->to_raw_(this->traits.get_min_value()), high = this->to_raw_(this->traits.get_max_value());
    if (low < 0 || high < 0)
      return;
    request.guard = Guard::range(low, high);
    request.retry = true;
  }
  this->hub_->write(request);
}

void NovaLinkNumber::on_value(uint8_t address, uint8_t value) {
  if (value < this->offset_) {
    ESP_LOGW(TAG, "%02X:%02X reads %u, below the offset", this->location_, this->address_, value);
    this->publish_state(NAN);
    return;
  }
  this->publish_state((value - this->offset_) * this->multiplier_);
}

void NovaLinkNumber::on_write_result(const WriteRequest &request, WriteResult result) {
  if (!this->has_mirror_ || request.custom || result != WriteResult::DONE)
    return;
  WriteRequest mirror;
  mirror.location = this->mirror_location_;
  mirror.address = this->mirror_address_;
  mirror.value = request.value;
  mirror.verify = false;
  this->hub_->write(mirror);
}

// ---- Protection temperature --------------------------------------------------------------------

void NovaLinkProtectionNumber::setup() {
  this->pref_ = this->make_entity_preference<float>();
  float value = this->initial_value_;
  if (!this->pref_.load(&value) || std::isnan(value) || value < this->traits.get_min_value() ||
      value > this->traits.get_max_value())
    value = this->initial_value_;
  this->apply_(value);
}

void NovaLinkProtectionNumber::dump_config() { LOG_NUMBER("", "NovaLink protection", this); }

void NovaLinkProtectionNumber::control(float value) {
  this->apply_(value);
  this->pref_.save(&value);
}

void NovaLinkProtectionNumber::apply_(float value) {
  switch (this->kind_) {
    case MAXIMUM:
      this->hub_->protection().max_temperature = value;
      break;
    case FALLBACK_STOP:
      this->hub_->protection().fallback_stop = value;
      break;
    case FALLBACK_SETPOINT:
      this->hub_->protection().fallback_setpoint = value;
      break;
  }
  this->publish_state(value);
}

// ---- Heartbeat ---------------------------------------------------------------------------------

void NovaLinkHeartbeatNumber::control(float value) {
  this->publish_state(value);
  this->hub_->heartbeat();
}

}  // namespace esphome::novalink
