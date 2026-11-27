#include "stove_entity.h"

#include "esphome/core/log.h"

#include "novalink.h"

namespace esphome::novalink {

static const char *const TAG = "novalink";

void StoveEntity::update() {
  if (!this->hub_->request(this->location_, this->address_))
    ESP_LOGW(TAG, "Read of %02X:%02X not queued", this->location_, this->address_);
}

void StoveEntity::dump_config() {
  ESP_LOGCONFIG(TAG, "  Entity at %02X:%02X", this->location_, this->address_);
  LOG_UPDATE_INTERVAL(this);
}

void StoveEntity::handle_value(uint8_t address, uint8_t value) {
  this->failures_ = 0;
  this->on_value(address, value);
}

void StoveEntity::handle_failure() {
  this->on_failure();
  if (this->failures_ < MAX_FAILURES && ++this->failures_ == MAX_FAILURES)
    this->on_unknown();
}

}  // namespace esphome::novalink
