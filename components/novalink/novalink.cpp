#include "novalink.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::novalink {

static const char *const TAG = "novalink";

// ---- Component ---------------------------------------------------------------------------------

void NovaLink::setup() {
  // Kept in flash: after a power cut the board protects the stove even if the integration does not come back
  this->armed_pref_ = global_preferences->make_preference<bool>(fnv1_hash("novalink_armed"), true);
  bool armed = false;
  if (this->armed_pref_.load(&armed))
    this->protection_.armed = armed;
  this->protection_.begin(millis());
  if (this->protection_enabled_) {
    this->allow_write(this->state_location_, this->state_address_);
    this->set_interval(PROTECTION_INTERVAL, [this]() { this->run_protection_(); });
  }
  this->publish_alive_();
#ifdef USE_SENSOR
  if (this->eeprom_writes_sensor_ != nullptr)
    this->eeprom_writes_sensor_->publish_state(0);
#endif
  if (this->enable_rx_pin_ != nullptr) {
    this->enable_rx_pin_->setup();
    this->enable_rx_pin_->digital_write(!this->rx_idle_on_);
  }
}

void NovaLink::dump_config() {
  ESP_LOGCONFIG(TAG,
                "NovaLink:\n"
                "  Port: %s\n"
                "  Board: %s\n"
                "  Echo: %s\n"
                "  Read only: %s\n"
                "  Free writes: %s\n"
                "  EEPROM: %" PRIu32 " ms between writes, %u writes per 24 h (0: no limit)\n"
                "  Protection: %s, maximum %.1f °C, without NovaLink: %s %.1f °C, armed: %s",
                this->port_name_, this->board_name_, YESNO(this->bus_.config.echo),
                YESNO(this->bus_.config.read_only), YESNO(this->bus_.config.allow_custom_writes),
                this->bus_.config.eeprom_write_interval, this->bus_.config.eeprom_daily_limit,
                YESNO(this->protection_enabled_ && !this->bus_.config.read_only), this->protection_.max_temperature,
                this->protection_.fallback_mode == FallbackMode::STOP ? "stop at" : "setpoint",
                this->protection_.fallback_mode == FallbackMode::STOP ? this->protection_.fallback_stop
                                                                      : this->protection_.fallback_setpoint,
                YESNO(this->protection_.armed));
  LOG_PIN("  Enable RX pin: ", this->enable_rx_pin_);
}

void NovaLink::loop() {
  this->bus_.loop(millis());
  uint8_t location, address;
  // The queue may be full for now: the dump tries again at the next loop
  if (this->dump_.next(location, address) && this->request(location, address))
    this->dump_.sent();
}

// ---- Line --------------------------------------------------------------------------------------

void NovaLink::send_(const uint8_t *data, size_t len) {
  if (this->enable_rx_pin_ == nullptr) {
    this->write_array(data, len);
    return;
  }
  // High: reception off while the frame leaves
  this->enable_rx_pin_->digital_write(true);
  this->write_array(data, len);
  this->flush();
  this->enable_rx_pin_->digital_write(false);
}

void NovaLink::release_() {
  if (this->enable_rx_pin_ != nullptr && !this->rx_idle_on_)
    this->enable_rx_pin_->digital_write(true);
}

void NovaLink::set_fast(bool fast) {
  if (fast) {
    this->high_freq_.start();
  } else {
    this->high_freq_.stop();
  }
}

// ---- Replies -----------------------------------------------------------------------------------

void NovaLink::on_value(uint8_t location, uint8_t address, uint8_t value) {
  for (auto *entity : this->entities_) {
    if (entity->matches(location, address))
      entity->handle_value(address, value);
  }
  if (location == this->probe_location_ && address == this->probe_address_) {
    this->probe_ = value / 2.0f;
    this->probe_time_ = millis();
  }
  if (location == this->state_location_ && address == this->state_address_ && this->state_filter_.accept(value)) {
    this->state_ = value;
    this->state_time_ = millis();
  }
  this->dump_.on_result(location, address, value);
}

void NovaLink::on_read_failed(uint8_t location, uint8_t address) {
  ESP_LOGW(TAG, "No valid reply for %02X:%02X", location, address);
  for (auto *entity : this->entities_) {
    if (entity->matches(location, address))
      entity->handle_failure();
  }
  this->dump_.on_result(location, address, MemoryDump::NO_VALUE);
}

void NovaLink::on_link(bool up) {
  if (up) {
    ESP_LOGI(TAG, "The stove answers");
  } else {
    ESP_LOGW(TAG, "The stove does not answer");
    for (auto *entity : this->entities_)
      entity->on_unknown();
  }
#ifdef USE_BINARY_SENSOR
  if (this->link_sensor_ != nullptr)
    this->link_sensor_->publish_state(up);
#endif
}

void NovaLink::on_state_changed() {
  if (this->display_ != nullptr)
    this->display_->update();
}

// ---- Writes ------------------------------------------------------------------------------------

bool NovaLink::write(const WriteRequest &request) {
  const WriteResult result = this->bus_.queue_write(request);
  if (result == WriteResult::ACCEPTED) {
    ESP_LOGD(TAG, "Write %02X:%02X = %u queued", request.location, request.address, request.value);
    return true;
  }
  ESP_LOGW(TAG, "Write %02X:%02X = %u %s", request.location, request.address, request.value,
           write_result_to_string(result));
  return false;
}

bool NovaLink::write_custom(uint8_t location, uint8_t address, uint8_t value, bool verify) {
  WriteRequest request;
  request.location = location;
  request.address = address;
  request.value = value;
  request.verify = verify;
  request.custom = true;
  return this->write(request);
}

void NovaLink::on_write_result(const WriteRequest &request, WriteResult result) {
  const bool ok = result == WriteResult::DONE || result == WriteResult::UNCHANGED || result == WriteResult::SUPERSEDED;
  // A free write always leaves a trace at the warning level
  if (ok && !request.custom) {
    ESP_LOGI(TAG, "Write %02X:%02X = %u: %s", request.location, request.address, request.value,
             write_result_to_string(result));
  } else {
    ESP_LOGW(TAG, "%s %02X:%02X = %u: %s", request.custom ? "Free write" : "Write", request.location, request.address,
             request.value, write_result_to_string(result));
  }
#ifdef USE_SENSOR
  if (this->eeprom_writes_sensor_ != nullptr &&
      this->eeprom_writes_sensor_->get_state() != static_cast<float>(this->bus_.eeprom_writes()))
    this->eeprom_writes_sensor_->publish_state(this->bus_.eeprom_writes());
#endif
  // Stop sent by the protection
  if (this->stopping_ && request.location == this->state_location_ && request.address == this->state_address_ &&
      request.value == this->off_value_) {
    this->stopping_ = false;
    if (result == WriteResult::DONE)
      this->protection_done_(protection_to_string(this->stop_event_));
  }
#ifdef USE_NUMBER
  // Fallback setpoint
  if (this->setting_fallback_ && request.location == this->setpoint_location_ &&
      request.address == this->setpoint_address_) {
    this->setting_fallback_ = false;
    if (result == WriteResult::DONE || result == WriteResult::UNCHANGED)
      this->protection_.setpoint_written();
    if (result == WriteResult::DONE)
      this->protection_done_(FALLBACK_SETPOINT_TEXT);
  }
#endif
  for (auto *entity : this->entities_) {
    if (entity->matches(request.location, request.address))
      entity->on_write_result(request, result);
  }
}

// ---- Protection --------------------------------------------------------------------------------

void NovaLink::set_protection(bool enabled, float max_temperature, float fallback_stop, uint32_t heartbeat_timeout,
                              uint32_t probe_timeout) {
  this->protection_enabled_ = enabled;
  this->protection_.max_temperature = max_temperature;
  this->protection_.fallback_stop = fallback_stop;
  this->protection_.heartbeat_timeout = heartbeat_timeout;
  this->protection_.probe_timeout = probe_timeout;
}

void NovaLink::set_protection_addresses(uint8_t probe_location, uint8_t probe_address, uint8_t state_location,
                                        uint8_t state_address) {
  this->probe_location_ = probe_location;
  this->probe_address_ = probe_address;
  this->state_location_ = state_location;
  this->state_address_ = state_address;
}

void NovaLink::heartbeat() {
  if (this->protection_.heartbeat(millis())) {
    ESP_LOGI(TAG, "First report from NovaLink: protection armed");
    const bool armed = true;
    this->armed_pref_.save(&armed);
    global_preferences->sync();
  }
  this->publish_alive_();
}

void NovaLink::publish_alive_() {
#ifdef USE_BINARY_SENSOR
  if (this->alive_sensor_ != nullptr)
    this->alive_sensor_->publish_state(this->protection_.alive());
#endif
}

void NovaLink::protection_done_(const char *text) {
  ESP_LOGW(TAG, "%s", text);
#ifdef USE_TEXT_SENSOR
  if (this->last_protection_ != nullptr)
    this->last_protection_->publish_state(text);
#endif
}

void NovaLink::run_protection_() {
  const uint32_t now = millis();
  // Its own readings: it does not depend on the entities of the configuration
  this->request(this->probe_location_, this->probe_address_);
  this->request(this->state_location_, this->state_address_);
  if (!std::isnan(this->probe_) && now - this->probe_time_ > MAX_AGE)
    this->probe_ = NAN;
  if (this->state_ >= 0 && now - this->state_time_ > MAX_AGE)
    this->state_ = -1;

  const ProtectionEvent event = this->protection_.evaluate(now, this->probe_, this->state_);
  this->publish_alive_();
#ifdef USE_SWITCH
  if (this->term_switch_ != nullptr && this->term_switch_->state && this->protection_.term_must_open()) {
    ESP_LOGW(TAG, "NovaLink lost: TERM contact opened");
    this->term_switch_->turn_off();
  }
#endif
  if (this->bus_.config.read_only)
    return;
  this->run_fallback_setpoint_(now);
  if (event == ProtectionEvent::NONE)
    return;
  ESP_LOGW(TAG, "Protection: %.1f °C, state %d, stop of the stove", this->probe_, this->state_);
  WriteRequest request;
  request.location = this->state_location_;
  request.address = this->state_address_;
  request.value = this->off_value_;
  request.guard = Guard::in_states(PROTECTION_STOP_STATES);
  request.verify = false;
  this->stop_event_ = event;
  this->stopping_ = true;
  if (!this->write(request))
    this->stopping_ = false;
}

void NovaLink::run_fallback_setpoint_(uint32_t now) {
#ifdef USE_NUMBER
  if (this->setpoint_number_ == nullptr)
    return;
  const float setpoint = this->setpoint_number_->has_state() ? this->setpoint_number_->state : NAN;
  if (!this->protection_.setpoint_due(now, setpoint))
    return;
  const float value = this->protection_.fallback_setpoint;
  ESP_LOGW(TAG, "NovaLink lost: stove setpoint %.0f -> %.0f °C, the stove regulates by itself", setpoint, value);
  this->setting_fallback_ = true;
  auto call = this->setpoint_number_->make_call();
  call.set_value(value);
  call.perform();
#endif
}

}  // namespace esphome::novalink
