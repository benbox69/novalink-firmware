#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "memory_dump.h"
#include "protection.h"
#include "protocol.h"
#include "stove.h"
#include "stove_entity.h"

namespace esphome::novalink {

/// The hub: owns the line to the stove (Bus), polls the addresses of the entities, sends the writes, runs the
/// protection. The entities live in the platform folders (sensor/, switch/...) and register here.
class NovaLink final : public Component, public uart::UARTDevice, public Listener {
 public:
  NovaLink() : transport_(this), bus_(&this->transport_, this) {}

  // ---- Configuration (from __init__.py) ----
  void set_names(const char *port, const char *board) {
    this->port_name_ = port;
    this->board_name_ = board;
  }
  void set_echo(bool echo) { this->bus_.config.echo = echo; }
  void set_reply_timeout(uint32_t timeout) { this->bus_.config.reply_timeout = timeout; }
  /// `idle_on`: reception stays open between two exchanges.
  void set_enable_rx_pin(GPIOPin *pin, bool idle_on) {
    this->enable_rx_pin_ = pin;
    this->rx_idle_on_ = idle_on;
  }
  void set_read_only(bool read_only) { this->bus_.config.read_only = read_only; }
  void set_allow_custom_writes(bool allow) { this->bus_.config.allow_custom_writes = allow; }
  void set_eeprom_limits(uint32_t interval, uint16_t daily) {
    this->bus_.config.eeprom_write_interval = interval;
    this->bus_.config.eeprom_daily_limit = daily;
  }
  void allow_write(uint8_t location, uint8_t address) { this->bus_.allow_write(location, address); }
  void block_write(uint8_t location, uint8_t address) { this->bus_.block_write(location, address); }
  void set_protection(bool enabled, float max_temperature, float fallback_stop, uint32_t heartbeat_timeout,
                      uint32_t probe_timeout);
  void set_fallback(FallbackMode mode, float setpoint) {
    this->protection_.fallback_mode = mode;
    this->protection_.fallback_setpoint = setpoint;
  }
  /// Addresses that the protection reads by itself: the stove probe and the stove state.
  void set_protection_addresses(uint8_t probe_location, uint8_t probe_address, uint8_t state_location,
                                uint8_t state_address);
  /// Value written to the state address to stop the stove.
  void set_off_value(uint8_t value) { this->off_value_ = value; }

  // ---- Entities (from the platform files) ----
  void register_entity(StoveEntity *entity) { this->entities_.push_back(entity); }
  /// Display text: read again at once when the stove state changes.
  void set_display(StoveEntity *display) { this->display_ = display; }
#ifdef USE_NUMBER
  /// Stove setpoint: the fallback setpoint is written through it (its limits and write rules).
  void set_setpoint_number(number::Number *number, uint8_t location, uint8_t address) {
    this->setpoint_number_ = number;
    this->setpoint_location_ = location;
    this->setpoint_address_ = address;
  }
#endif
#ifdef USE_SWITCH
  void set_term_switch(switch_::Switch *term) { this->term_switch_ = term; }
#endif
#ifdef USE_SENSOR
  void set_eeprom_writes_sensor(sensor::Sensor *sensor) { this->eeprom_writes_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_protection(text_sensor::TextSensor *sensor) { this->last_protection_ = sensor; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_link_sensor(binary_sensor::BinarySensor *sensor) { this->link_sensor_ = sensor; }
  void set_alive_sensor(binary_sensor::BinarySensor *sensor) { this->alive_sensor_ = sensor; }
#endif

  // ---- Component ----
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ---- Used by the entities ----
  bool request(uint8_t location, uint8_t address) { return this->bus_.queue_read(location, address); }
  /// Queue a write. Every refusal is logged.
  bool write(const WriteRequest &request);
  /// Free write (custom entities, `novalink.write`).
  bool write_custom(uint8_t location, uint8_t address, uint8_t value, bool verify);
  /// The Home Assistant integration reports (heartbeat number).
  void heartbeat();
  Protection &protection() { return this->protection_; }
  void start_dump() { this->dump_.start(); }
  void on_state_changed();
  /// Reads follow each other quickly while the display is read.
  void set_fast(bool fast);

  // ---- Listener (from the Bus) ----
  void on_value(uint8_t location, uint8_t address, uint8_t value) override;
  void on_read_failed(uint8_t location, uint8_t address) override;
  void on_write_result(const WriteRequest &request, WriteResult result) override;
  void on_link(bool up) override;

 protected:
  /// The Bus talks to the UART through this.
  class UartTransport final : public Transport {
   public:
    explicit UartTransport(NovaLink *parent) : parent_(parent) {}
    size_t available() override { return this->parent_->available(); }
    uint8_t read_byte() override {
      uint8_t byte = 0;
      this->parent_->read_byte(&byte);
      return byte;
    }
    void send(const uint8_t *data, size_t len) override { this->parent_->send_(data, len); }
    void release() override { this->parent_->release_(); }

   protected:
    NovaLink *parent_;
  };

  void send_(const uint8_t *data, size_t len);
  void release_();
  void run_protection_();
  void run_fallback_setpoint_(uint32_t now);
  void protection_done_(const char *text);
  void publish_alive_();

  static constexpr uint32_t PROTECTION_INTERVAL = 15000;
  static constexpr uint32_t MAX_AGE = 60000;  ///< an older reading counts as unknown for the protection

  // Line
  UartTransport transport_;
  Bus bus_;
  HighFrequencyLoopRequester high_freq_;
  GPIOPin *enable_rx_pin_{nullptr};
  bool rx_idle_on_{true};
  const char *port_name_{""};
  const char *board_name_{""};

  // Entities
  StaticVector<StoveEntity *, NOVALINK_ENTITY_COUNT> entities_;
  StoveEntity *display_{nullptr};
  MemoryDump dump_;
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *link_sensor_{nullptr};
  binary_sensor::BinarySensor *alive_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *eeprom_writes_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_protection_{nullptr};
#endif

  // Protection
  Protection protection_;
  bool protection_enabled_{false};
  ESPPreferenceObject armed_pref_;
  StateFilter state_filter_;
  float probe_{NAN};
  uint32_t probe_time_{0};
  int16_t state_{-1};
  uint32_t state_time_{0};
  uint8_t probe_location_{0};
  uint8_t probe_address_{0};
  uint8_t state_location_{0};
  uint8_t state_address_{0};
  uint8_t off_value_{0x06};
  bool stopping_{false};  ///< a stop of the protection is under way
  ProtectionEvent stop_event_{ProtectionEvent::NONE};
#ifdef USE_SWITCH
  switch_::Switch *term_switch_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *setpoint_number_{nullptr};
  uint8_t setpoint_location_{0};
  uint8_t setpoint_address_{0};
  bool setting_fallback_{false};  ///< the fallback setpoint is being written
#endif
};

}  // namespace esphome::novalink
