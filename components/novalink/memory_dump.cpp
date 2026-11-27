#include "memory_dump.h"

#include <cstdio>
#include <cstring>

#include "esphome/core/log.h"

#include "protocol.h"

namespace esphome::novalink {

static const char *const TAG = "novalink";

void MemoryDump::start() {
  if (this->active())
    return;
  ESP_LOGI(TAG, "Memory dump: location, first address, 16 values (-- = not read)");
  this->index_ = 0;
  this->waiting_ = false;
}

uint8_t MemoryDump::location_() const { return this->index_ < 256 ? LOCATION_RAM : LOCATION_EEPROM; }

bool MemoryDump::next(uint8_t &location, uint8_t &address) {
  if (!this->active() || this->waiting_)
    return false;
  location = this->location_();
  address = this->address_();
  if (is_readable(location, address))
    return true;
  this->waiting_ = true;
  this->on_result(location, address, NO_VALUE);
  return false;
}

void MemoryDump::on_result(uint8_t location, uint8_t address, int16_t value) {
  if (!this->waiting_ || !this->active() || location != this->location_() || address != this->address_())
    return;
  this->waiting_ = false;
  this->line_[address % LINE] = value;
  if (address % LINE == LINE - 1) {
    char line[LINE * 3 + 1];
    for (size_t i = 0; i < LINE; i++) {
      if (this->line_[i] == NO_VALUE) {
        std::memcpy(line + i * 3, " --", 4);
      } else {
        snprintf(line + i * 3, 4, " %02X", static_cast<unsigned>(this->line_[i]) & 0xFF);
      }
    }
    ESP_LOGI(TAG, "%s %02X:%s", location == LOCATION_RAM ? "RAM" : "EEPROM", static_cast<unsigned>(address) & 0xF0,
             line);
  }
  if (++this->index_ == SIZE) {
    this->index_ = OFF;
    ESP_LOGI(TAG, "Memory dump: end");
  }
}

}  // namespace esphome::novalink
