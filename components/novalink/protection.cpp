#include "protection.h"

#include <cmath>

namespace esphome::novalink {

const char *protection_to_string(ProtectionEvent event) {
  switch (event) {
    case ProtectionEvent::MAX_TEMPERATURE:
      return "Stopped: maximum temperature";
    case ProtectionEvent::NO_HEARTBEAT:
      return "Stopped: NovaLink lost";
    case ProtectionEvent::NO_PROBE:
      return "Stopped: NovaLink and probe lost";
    default:
      return "None";
  }
}

bool Protection::heartbeat(uint32_t now) {
  this->last_heartbeat_ = now;
  this->had_heartbeat_ = true;
  this->lost_ = false;
  this->setpoint_done_ = false;
  this->setpoint_attempts_ = 0;
  if (this->armed)
    return false;
  this->armed = true;
  return true;
}

void Protection::follow_(Source &source, bool known, uint32_t now) {
  if (known == source.known)
    return;
  source.known = known;
  if (!known) {
    source.lost_before = this->lost_;
  } else if (this->lost_ && !source.lost_before) {
    this->grace_ = true;
    this->grace_start_ = now;
  }
}

void Protection::update_(uint32_t now, float probe, int state) {
  if (!std::isnan(probe)) {
    this->last_probe_ = now;
    this->probe_lost_ = false;
  } else if (now - this->last_probe_ >= this->probe_timeout) {
    this->probe_lost_ = true;
  }
  if (now - this->last_heartbeat_ >= this->heartbeat_timeout)
    this->lost_ = true;
  if (now - this->grace_start_ >= PROTECTION_GRACE_MS)
    this->grace_ = false;
  this->follow_(this->state_source_, state >= 0, now);
  this->follow_(this->probe_source_, !std::isnan(probe), now);
  if (now - this->last_command_ >= PROTECTION_COOLDOWN_MS)
    this->commanded_ = false;
}

ProtectionEvent Protection::evaluate(uint32_t now, float probe, int state) {
  this->update_(now, probe, state);
  if (state < 0 || state > 31 || (PROTECTION_STOP_STATES & (1UL << state)) == 0)
    return ProtectionEvent::NONE;
  // A former stop is still being carried out
  if (this->commanded_)
    return ProtectionEvent::NONE;
  ProtectionEvent event = ProtectionEvent::NONE;
  // Without the integration the board never interferes: only the maximum applies
  const bool fallback = this->armed && this->fallback_enabled && this->lost_ && !this->grace_;
  if (probe >= this->max_temperature) {  // false when the probe is unknown (NAN), as below
    event = ProtectionEvent::MAX_TEMPERATURE;
  } else if (fallback && this->fallback_mode == FallbackMode::STOP && probe >= this->fallback_stop) {
    event = ProtectionEvent::NO_HEARTBEAT;
  } else if (fallback && this->probe_lost_) {
    event = ProtectionEvent::NO_PROBE;
  }
  if (event != ProtectionEvent::NONE) {
    this->commanded_ = true;
    this->last_command_ = now;
  }
  return event;
}

bool Protection::setpoint_due(uint32_t now, float setpoint) {
  if (this->fallback_mode != FallbackMode::SETPOINT || !this->armed || !this->fallback_enabled || !this->lost_ ||
      this->grace_ || this->setpoint_done_ || std::isnan(setpoint))
    return false;
  if (setpoint <= this->fallback_setpoint) {
    this->setpoint_done_ = true;
    return false;
  }
  if (this->setpoint_attempts_ >= PROTECTION_SETPOINT_ATTEMPTS ||
      (this->setpoint_attempts_ > 0 && now - this->last_setpoint_ < PROTECTION_COOLDOWN_MS))
    return false;
  this->setpoint_attempts_++;
  this->last_setpoint_ = now;
  return true;
}

}  // namespace esphome::novalink
