#include "novalink_switch.h"

#include "esphome/components/novalink/novalink.h"
#include "esphome/core/log.h"

namespace esphome::novalink {

static const char *const TAG = "novalink";

static constexpr uint32_t STATES_OFF = 1UL << 0;
static constexpr uint32_t STATES_RUNNING = 0x3EUL | (1UL << 7);  // 1 to 5, and 7

// ---- Stove on/off ------------------------------------------------------------------------------

void NovaLinkStoveSwitch::write_state(bool state) {
  WriteRequest request;
  request.location = this->location_;
  request.address = this->address_;
  request.value = state ? this->value_on_ : this->value_off_;
  // Light only an off stove, stop only a running one
  request.guard = Guard::in_states(state ? STATES_OFF : STATES_RUNNING);
  request.verify = false;
  this->hub_->write(request);
}

void NovaLinkStoveSwitch::on_value(uint8_t address, uint8_t value) {
  if (this->filter_.accept(value))
    this->publish_state(value < 32 && (STATES_RUNNING & (1UL << value)) != 0);
}

void NovaLinkStoveSwitch::on_write_result(const WriteRequest &request, WriteResult result) {
  if (result == WriteResult::DONE)
    this->update();
}

// ---- Custom ------------------------------------------------------------------------------------

void NovaLinkCustomSwitch::write_state(bool state) {
  this->hub_->write_custom(this->location_, this->address_, state ? this->value_on_ : this->value_off_, true);
}

// ---- Protection without NovaLink ---------------------------------------------------------------

void NovaLinkFallbackSwitch::setup() {
  if (auto state = this->get_initial_state_with_restore_mode())
    this->write_state(*state);
}

void NovaLinkFallbackSwitch::dump_config() { LOG_SWITCH("", "Protection without NovaLink", this); }

void NovaLinkFallbackSwitch::write_state(bool state) {
  this->hub_->protection().fallback_enabled = state;
  this->publish_state(state);
}

}  // namespace esphome::novalink
