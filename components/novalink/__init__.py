"""NovaLink: Micronova pellet stoves. Derived from the ESPHome micronova component (@jorre05, @edenhaus).

This file: the hub (`novalink:`), its protection and the `novalink.write` action. The entities are in the platform
folders (sensor/, switch/...), the shared entity helpers in entity.py, the stove addresses in boards.py.
"""

import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import switch, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BOARD,
    CONF_ID,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_VALUE,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv

from .boards import BLOCKED_WRITES, BOARD_GENERIC, BOARDS
from .const import (
    CONF_ALLOW_CUSTOM_WRITES,
    CONF_FALLBACK,
    CONF_FALLBACK_SETPOINT,
    CONF_FALLBACK_STOP,
    CONF_MAX_TEMPERATURE,
    CONF_MEMORY_ADDRESS,
    CONF_PROTECTION,
    DOMAIN,
    FALLBACK_SETPOINT,
    FALLBACK_STOP,
    NovaLink,
    novalink_ns,
)
from .entity import address_of, entity_count, memory_address, write_location

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@benbox69"]
DEPENDENCIES = ["uart"]

CONF_ECHO = "echo"
CONF_EEPROM_DAILY_LIMIT = "eeprom_daily_limit"
CONF_EEPROM_WRITE_INTERVAL = "eeprom_write_interval"
CONF_ENABLE_RX_IDLE = "enable_rx_idle"
CONF_ENABLE_RX_PIN = "enable_rx_pin"
CONF_ENABLED = "enabled"
CONF_HEARTBEAT_TIMEOUT = "heartbeat_timeout"
CONF_LOCATION = "location"
CONF_PROBE_TIMEOUT = "probe_timeout"
CONF_READ_ONLY = "read_only"
CONF_REPLY_TIMEOUT = "reply_timeout"
CONF_TERM_SWITCH = "term_switch"

# Serial settings of each stove port. "custom": set them yourself under `uart:`.
PORTS = {
    "navel_inside": {"baud_rate": 19200, "data_bits": 8, "parity": "NONE"},
    "serial": {"baud_rate": 1200, "data_bits": 8, "parity": "NONE", "stop_bits": 2},
    "custom": {},
}

FallbackMode = novalink_ns.enum("FallbackMode", is_class=True)
FALLBACK_MODES = {
    FALLBACK_SETPOINT: FallbackMode.SETPOINT,
    FALLBACK_STOP: FallbackMode.STOP,
}
WriteAction = novalink_ns.class_("WriteAction", automation.Action)

# Reads of one display pass, and some room
EXTRA_READS = 20


# ---- Configuration ----


def _set_echo_default(config):
    config = config.copy()
    # With a reception gate, the interface does not hear itself
    config.setdefault(CONF_ECHO, CONF_ENABLE_RX_PIN not in config)
    if CONF_ENABLE_RX_IDLE in config and CONF_ENABLE_RX_PIN not in config:
        raise cv.Invalid(f"{CONF_ENABLE_RX_IDLE} needs {CONF_ENABLE_RX_PIN}")
    return config


_TIMEOUT = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(minutes=2), max=cv.TimePeriod(hours=24)),
)

PROTECTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_MAX_TEMPERATURE, default=30): cv.float_range(24, 38),
        # Integration lost: give the stove a setpoint and let it regulate, or stop it
        cv.Optional(CONF_FALLBACK, default=FALLBACK_SETPOINT): cv.one_of(
            *FALLBACK_MODES, lower=True
        ),
        cv.Optional(CONF_FALLBACK_SETPOINT, default=22): cv.int_range(6, 30),
        cv.Optional(CONF_FALLBACK_STOP, default=24): cv.float_range(18, 32),
        cv.Optional(CONF_HEARTBEAT_TIMEOUT, default="10min"): _TIMEOUT,
        cv.Optional(CONF_PROBE_TIMEOUT, default="10min"): _TIMEOUT,
        cv.Optional(CONF_TERM_SWITCH): cv.use_id(switch.Switch),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NovaLink),
            cv.Required(CONF_PORT): cv.one_of(*PORTS, lower=True),
            cv.Optional(CONF_BOARD, default=BOARD_GENERIC): cv.one_of(
                *BOARDS, lower=True
            ),
            cv.Optional(CONF_ECHO): cv.boolean,
            cv.Optional(CONF_ENABLE_RX_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_ENABLE_RX_IDLE): cv.boolean,
            cv.Optional(
                CONF_REPLY_TIMEOUT, default="200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_READ_ONLY, default=False): cv.boolean,
            cv.Optional(CONF_ALLOW_CUSTOM_WRITES, default=False): cv.boolean,
            cv.Optional(
                CONF_EEPROM_WRITE_INTERVAL, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_EEPROM_DAILY_LIMIT, default=50): cv.int_range(0, 65535),
            cv.Optional(CONF_PROTECTION, default={}): PROTECTION_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _set_echo_default,
    cv.require_esphome_version(2026, 10, 0),
)


def _has_setpoint_number(full_config):
    return any(
        platform.get(CONF_PLATFORM) == DOMAIN and "thermostat_setpoint" in platform
        for platform in full_config.get("number", [])
    )


def _final_validate(config):
    config = uart.final_validate_device_schema(
        DOMAIN, require_rx=True, require_tx=True, **PORTS[config[CONF_PORT]]
    )(config)
    protection = config[CONF_PROTECTION]
    if (
        protection[CONF_ENABLED]
        and protection[CONF_FALLBACK] == FALLBACK_SETPOINT
        and not _has_setpoint_number(fv.full_config.get())
    ):
        raise cv.Invalid(
            f"'{CONF_FALLBACK}: {FALLBACK_SETPOINT}' writes the stove setpoint: add the number "
            f"'thermostat_setpoint' (platform {DOMAIN}), or set '{CONF_FALLBACK}: {FALLBACK_STOP}'",
            path=[CONF_PROTECTION, CONF_FALLBACK],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# ---- Code generation ----


def _configured_address(name, places):
    """Address read by the protection: the one of the entity, when its configuration changes it."""
    for domain, key in places:
        for platform in CORE.config.get(domain, []):
            if platform.get(CONF_PLATFORM) == DOMAIN and CONF_MEMORY_ADDRESS in (
                platform.get(key) or {}
            ):
                return address_of(name, platform[key])
    return address_of(name, {})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_names(config[CONF_PORT], config[CONF_BOARD]))
    cg.add(var.set_echo(config[CONF_ECHO]))
    cg.add(var.set_reply_timeout(config[CONF_REPLY_TIMEOUT].total_milliseconds))
    if enable_rx_pin := config.get(CONF_ENABLE_RX_PIN):
        pin = await cg.gpio_pin_expression(enable_rx_pin)
        cg.add(var.set_enable_rx_pin(pin, config.get(CONF_ENABLE_RX_IDLE, True)))

    # Writes
    cg.add(var.set_read_only(config[CONF_READ_ONLY]))
    cg.add(var.set_allow_custom_writes(config[CONF_ALLOW_CUSTOM_WRITES]))
    cg.add(
        var.set_eeprom_limits(
            config[CONF_EEPROM_WRITE_INTERVAL].total_milliseconds,
            config[CONF_EEPROM_DAILY_LIMIT],
        )
    )
    for location, address in BLOCKED_WRITES[config[CONF_BOARD]]:
        cg.add(var.block_write(location, address))
    if config[CONF_ALLOW_CUSTOM_WRITES]:
        _LOGGER.warning(
            "Free writes to the stove are allowed. A wrong one can raise an alarm, change the combustion or a factory "
            "setting for good, and wear the EEPROM. See docs/custom-writes.md"
        )

    # Protection
    protection = config[CONF_PROTECTION]
    if protection[CONF_ENABLED] and config[CONF_READ_ONLY]:
        _LOGGER.warning("Read only: the stove protection cannot stop the stove")
    cg.add(
        var.set_protection(
            protection[CONF_ENABLED],
            protection[CONF_MAX_TEMPERATURE],
            protection[CONF_FALLBACK_STOP],
            protection[CONF_HEARTBEAT_TIMEOUT].total_milliseconds,
            protection[CONF_PROBE_TIMEOUT].total_milliseconds,
        )
    )
    cg.add(
        var.set_fallback(
            FALLBACK_MODES[protection[CONF_FALLBACK]],
            protection[CONF_FALLBACK_SETPOINT],
        )
    )
    cg.add(
        var.set_protection_addresses(
            *_configured_address("room_temperature", [("sensor", "room_temperature")]),
            *_configured_address(
                "stove_state",
                [
                    ("switch", "stove"),
                    ("sensor", "state_code"),
                    ("text_sensor", "stove_state"),
                ],
            ),
        )
    )
    if CONF_TERM_SWITCH in protection:
        term = await cg.get_variable(protection[CONF_TERM_SWITCH])
        cg.add(var.set_term_switch(term))
    CORE.add_job(_size_lists)


@coroutine_with_priority(CoroPriority.FINAL)
async def _size_lists():
    """Once every platform is done: size of the entity list and of the read queue."""
    count = entity_count()
    cg.add_define("NOVALINK_ENTITY_COUNT", max(count, 1))
    cg.add_define("NOVALINK_MAX_READS", count + EXTRA_READS)


# ---- Action novalink.write ----


@automation.register_action(
    "novalink.write",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(NovaLink),
            cv.Required(CONF_LOCATION): write_location,
            cv.Required(CONF_ADDRESS): memory_address,
            cv.Required(CONF_VALUE): cv.templatable(cv.hex_int_range(0x00, 0xFF)),
        }
    ),
    synchronous=True,
)
async def write_action_to_code(config, action_id, template_arg, args):
    hub = CORE.config[DOMAIN]
    address = (config[CONF_LOCATION], config[CONF_ADDRESS])
    if not hub[CONF_ALLOW_CUSTOM_WRITES]:
        raise cv.Invalid(
            f"novalink.write needs '{CONF_ALLOW_CUSTOM_WRITES}: true' under '{DOMAIN}:'"
        )
    if address in BLOCKED_WRITES[hub[CONF_BOARD]]:
        raise cv.Invalid(
            f"0x{address[0]:02X}:0x{address[1]:02X} must never be written on this board"
        )
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_location(config[CONF_LOCATION]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint8)
    cg.add(var.set_value(template_))
    return var
