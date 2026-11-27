#pragma once

// Stove protection in the board: works without Home Assistant. Plain C++, tested on a computer.
// It never lights the stove, never stops it during an ignition or a cleaning. Its only EEPROM write is the fallback
// setpoint, once per loss of the integration.

#include <cstdint>

namespace esphome::novalink {

enum class ProtectionEvent : uint8_t { NONE, MAX_TEMPERATURE, NO_HEARTBEAT, NO_PROBE };

const char *protection_to_string(ProtectionEvent event);

/// What the board does when the integration is lost: give the stove a setpoint and let it regulate, or stop it.
enum class FallbackMode : uint8_t { SETPOINT, STOP };

static constexpr const char *FALLBACK_SETPOINT_TEXT = "Fallback setpoint: NovaLink lost";

/// States in which the protection may stop the stove: work and standby.
static constexpr uint32_t PROTECTION_STOP_STATES = (1UL << 4) | (1UL << 7);
static constexpr uint32_t PROTECTION_COOLDOWN_MS = 180000;
/// When the stove or its probe comes back, time left to the integration to report again before a fallback stop.
static constexpr uint32_t PROTECTION_GRACE_MS = 120000;
/// Fallback setpoint: attempts per loss of the integration, PROTECTION_COOLDOWN_MS apart.
static constexpr uint8_t PROTECTION_SETPOINT_ATTEMPTS = 3;

class Protection {
 public:
  float max_temperature{30.0f};  ///< stop, whatever Home Assistant says
  float fallback_stop{24.0f};    ///< stop when the heartbeat is lost (STOP mode)
  float fallback_setpoint{22.0f};  ///< stove setpoint when the heartbeat is lost (SETPOINT mode)
  FallbackMode fallback_mode{FallbackMode::SETPOINT};
  uint32_t heartbeat_timeout{600000};  ///< ms
  uint32_t probe_timeout{600000};      ///< ms without a probe value, used only when the heartbeat is lost
  bool fallback_enabled{true};
  bool armed{false};  ///< a heartbeat was received once, ever: to be kept in flash

  /// At boot: the silences are counted from now.
  void begin(uint32_t now) {
    this->last_heartbeat_ = now;
    this->last_probe_ = now;
  }
  /// The integration reported. Returns true when this arms the protection (to be saved).
  bool heartbeat(uint32_t now);
  /// To be called every few seconds. `probe` and `state` are NAN / negative when unknown or outdated.
  /// Anything but NONE means: stop the stove now.
  ProtectionEvent evaluate(uint32_t now, float probe, int state);
  /// SETPOINT mode, to be called after evaluate(). `setpoint` is the stove setpoint read, NAN when unknown.
  /// True means: write fallback_setpoint into the stove setpoint now. Never raises the setpoint; once the stove shows
  /// a setpoint at or below it, nothing more until the next loss, so that a change on the stove panel is kept.
  bool setpoint_due(uint32_t now, float setpoint);
  /// The stove confirmed the fallback setpoint: nothing more until the next loss.
  void setpoint_written() { this->setpoint_done_ = true; }

  /// The integration is reporting now.
  bool alive() const { return this->had_heartbeat_ && !this->lost_; }
  /// The TERM contact must be open (modulation).
  bool term_must_open() const { return this->armed && this->fallback_enabled && this->lost_; }

 protected:
  /// Follows one source of the integration's readings (stove state, stove probe). If the heartbeat was lost while the
  /// source was unknown, the loss may only come from there: the integration gets a delay. Never twice for one heartbeat
  /// loss: at the next outage the heartbeat is already lost.
  struct Source {
    bool known{true};
    bool lost_before{false};  ///< the heartbeat was already lost when the source became unknown
  };
  void follow_(Source &source, bool known, uint32_t now);
  void update_(uint32_t now, float probe, int state);

  uint32_t last_heartbeat_{0};  ///< or boot
  uint32_t last_probe_{0};      ///< or boot
  uint32_t last_command_{0};
  uint32_t grace_start_{0};
  uint32_t last_setpoint_{0};
  uint8_t setpoint_attempts_{0};
  bool setpoint_done_{false};  ///< for this loss of the integration
  Source state_source_{};
  Source probe_source_{};
  bool grace_{false};  ///< fallback stops wait
  bool had_heartbeat_{false};
  bool lost_{false};       ///< latched: the 32-bit clock rolls over after 49.7 days
  bool probe_lost_{false};  ///< latched too
  bool commanded_{false};  ///< a stop was sent less than PROTECTION_COOLDOWN_MS ago
};

}  // namespace esphome::novalink
