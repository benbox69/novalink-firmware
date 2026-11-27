#include "novalink_text_sensor.h"

#include <cstdio>
#include <cstring>

#include "esphome/components/novalink/novalink.h"
#include "esphome/core/hal.h"

namespace esphome::novalink {

static const char *state_name(uint8_t code, bool x100_evo) {
  switch (code) {
    case 0:
      return "Off";
    case 1:
      return "Start";
    case 2:
      return "Pellets loading";
    case 3:
      return "Ignition";
    case 4:
      return "Working";
    case 5:
      return "Brazier cleaning";
    case 6:
      return "Final cleaning";
    case 7:
      return "Standby";
    case 8:
      return x100_evo ? "Alarm" : "No pellets alarm";
    case 9:
      return x100_evo ? "Alarm" : "No ignition alarm";
    case 10:
      return "Undefined alarm";
    default:
      return nullptr;
  }
}

// ---- Stove state -------------------------------------------------------------------------------

void NovaLinkStateText::on_value(uint8_t address, uint8_t value) {
  if (!this->filter_.accept(value) || value == this->last_code_)
    return;
  this->last_code_ = value;
  char text[16];
  const char *name = state_name(value, this->x100_evo_);
  if (name == nullptr) {
    snprintf(text, sizeof(text), "Unknown (%u)", value);
    name = text;
  }
  this->publish_state(name);
  this->hub_->on_state_changed();
}

void NovaLinkStateText::on_unknown() {
  this->filter_.reset();
  this->last_code_ = -1;
  this->publish_state("unknown");
}

// ---- Alarm -------------------------------------------------------------------------------------

void NovaLinkAlarmText::on_value(uint8_t address, uint8_t value) {
  if (value == this->last_code_)
    return;
  this->last_code_ = value;
  // Only the codes seen with the text of the stove display, see https://github.com/benbox69/novalink-firmware/wiki/Stove-addresses
  char text[16];
  const char *name;
  switch (value) {
    case 0x00:
      name = "None";
      break;
    case 0x01:
      name = "Fumes probe";
      break;
    case 0x10:
      name = "Out of pellets";
      break;
    default:
      snprintf(text, sizeof(text), "Code 0x%02X", value);
      name = text;
  }
  this->publish_state(name);
}

// ---- Display -----------------------------------------------------------------------------------

void NovaLinkDisplayText::update() {
  if (this->reader_.active())
    return;
  this->reader_.start(millis());
  this->hub_->set_fast(true);
  this->request_pass_();
}

void NovaLinkDisplayText::request_pass_() {
  for (size_t i = 0; i < DisplayReader::SIZE; i++)
    this->hub_->request(this->location_, this->address_ + i);
}

void NovaLinkDisplayText::on_value(uint8_t address, uint8_t value) {
  this->next_(this->reader_.on_char(address - this->address_, value, millis()));
}

void NovaLinkDisplayText::on_failure() { this->next_(this->reader_.on_failure(millis())); }

void NovaLinkDisplayText::next_(DisplayReader::Result result) {
  if (result == DisplayReader::Result::NEXT_PASS) {
    this->request_pass_();
  } else if (result == DisplayReader::Result::DONE) {
    this->hub_->set_fast(false);
    char text[DisplayReader::JOINED_SIZE];
    this->reader_.join(text, sizeof(text));
    if (this->reader_.text_count() > 0 && std::strcmp(this->last_text_, text) != 0) {
      std::memcpy(this->last_text_, text, sizeof(text));
      this->publish_state(text);
    }
  }
}

void NovaLinkDisplayText::on_unknown() {
  this->reader_.stop();
  this->last_text_[0] = '\0';
  this->hub_->set_fast(false);
  this->publish_state("unknown");
}

}  // namespace esphome::novalink
