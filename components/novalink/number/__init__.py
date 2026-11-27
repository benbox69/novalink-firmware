"""Stove settings, free writes, protection settings."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_MULTIPLY,
    CONF_OFFSET,
    CONF_STEP,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
)
from esphome.core import CORE

import esphome.final_validate as fv

from ..const import (
    CONF_CUSTOM,
    CONF_FALLBACK,
    CONF_FALLBACK_SETPOINT,
    CONF_FALLBACK_STOP,
    CONF_MAX_TEMPERATURE,
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_NOVALINK_ID,
    CONF_PROTECTION,
    DOMAIN,
    FALLBACK_SETPOINT,
    FALLBACK_STOP,
    NovaLink,
    StoveEntity,
    novalink_ns,
)
from ..entity import (
    address_of,
    entity_schema,
    final_validate_writes,
    memory_address,
    register_entity,
    write_location,
)

NovaLinkNumber = novalink_ns.class_("NovaLinkNumber", number.Number, StoveEntity)
NovaLinkProtectionNumber = novalink_ns.class_(
    "NovaLinkProtectionNumber", number.Number, cg.Component
)
NovaLinkHeartbeatNumber = novalink_ns.class_("NovaLinkHeartbeatNumber", number.Number)

CONF_FALLBACK_SETPOINT_NUMBER = "fallback_setpoint"
CONF_MAXIMUM_TEMPERATURE = "maximum_temperature"
CONF_MIRROR = "mirror"
CONF_NOVALINK_HEARTBEAT = "novalink_heartbeat"
CONF_POWER_LEVEL = "power_level"
CONF_STOP_WITHOUT_NOVALINK = "stop_without_novalink"
CONF_THERMOSTAT_SETPOINT = "thermostat_setpoint"

ADDRESSES = {
    CONF_THERMOSTAT_SETPOINT: CONF_THERMOSTAT_SETPOINT,
    CONF_POWER_LEVEL: CONF_POWER_LEVEL,
}


def _min_max(config):
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid(f"{CONF_MAX_VALUE} must be greater than {CONF_MIN_VALUE}")
    return config


def _write_schema(schema, **limits):
    return (
        schema.extend(entity_schema())
        .extend({cv.Optional(CONF_MEMORY_LOCATION): write_location})
        .extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=limits["min_value"]): cv.int_range(
                    0, 255
                ),
                cv.Optional(CONF_MAX_VALUE, default=limits["max_value"]): cv.int_range(
                    0, 255
                ),
                cv.Optional(CONF_OFFSET, default=0): cv.int_range(0, 255),
            }
        )
    )


def _protection_number(icon):
    return number.number_schema(
        NovaLinkProtectionNumber,
        icon=icon,
        device_class=DEVICE_CLASS_TEMPERATURE,
        unit_of_measurement=UNIT_CELSIUS,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend({cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True)})


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            cv.Optional(CONF_THERMOSTAT_SETPOINT): cv.All(
                _write_schema(
                    number.number_schema(
                        NovaLinkNumber,
                        device_class=DEVICE_CLASS_TEMPERATURE,
                        unit_of_measurement=UNIT_CELSIUS,
                    ),
                    min_value=6,
                    max_value=40,
                ),
                _min_max,
            ),
            cv.Optional(CONF_POWER_LEVEL): cv.All(
                _write_schema(
                    number.number_schema(NovaLinkNumber, icon="mdi:fire"),
                    min_value=1,
                    max_value=5,
                ).extend(
                    {
                        # Some boards apply the level only once its RAM copy is written too
                        cv.Optional(CONF_MIRROR): cv.Schema(
                            {
                                cv.Optional(
                                    CONF_MEMORY_LOCATION, default="ram"
                                ): write_location,
                                cv.Required(CONF_MEMORY_ADDRESS): memory_address,
                            }
                        ),
                    }
                ),
                _min_max,
            ),
            cv.Optional(CONF_CUSTOM): cv.ensure_list(
                number.number_schema(NovaLinkNumber)
                .extend(entity_schema(address_required=True))
                .extend({cv.Required(CONF_MEMORY_LOCATION): write_location})
                .extend(
                    {
                        cv.Required(CONF_MIN_VALUE): cv.float_,
                        cv.Required(CONF_MAX_VALUE): cv.float_,
                        cv.Optional(CONF_STEP, default=1): cv.positive_float,
                        cv.Optional(CONF_OFFSET, default=0): cv.int_range(0, 255),
                        cv.Optional(CONF_MULTIPLY, default=1): cv.positive_float,
                    }
                )
                .add_extra(_min_max)
            ),
            cv.Optional(CONF_NOVALINK_HEARTBEAT): number.number_schema(
                NovaLinkHeartbeatNumber,
                icon="mdi:heart-pulse",
                unit_of_measurement=UNIT_CELSIUS,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend({cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True)}),
            cv.Optional(CONF_MAXIMUM_TEMPERATURE): _protection_number(
                "mdi:thermometer-alert"
            ),
            cv.Optional(CONF_STOP_WITHOUT_NOVALINK): _protection_number(
                "mdi:thermometer-off"
            ),
            cv.Optional(CONF_FALLBACK_SETPOINT_NUMBER): _protection_number(
                "mdi:thermostat-auto"
            ),
        }
    ),
    cv.has_at_least_one_key(
        *ADDRESSES,
        CONF_CUSTOM,
        CONF_NOVALINK_HEARTBEAT,
        CONF_MAXIMUM_TEMPERATURE,
        CONF_STOP_WITHOUT_NOVALINK,
        CONF_FALLBACK_SETPOINT_NUMBER,
    ),
)

_validate_writes = final_validate_writes(ADDRESSES, CONF_CUSTOM)
# Each fallback number only means something in its own mode
_FALLBACK_NUMBERS = {
    CONF_STOP_WITHOUT_NOVALINK: FALLBACK_STOP,
    CONF_FALLBACK_SETPOINT_NUMBER: FALLBACK_SETPOINT,
}


def _final_validate(config):
    config = _validate_writes(config)
    hub = fv.full_config.get().get(DOMAIN)
    if not hub:
        return config
    mode = hub[CONF_PROTECTION][CONF_FALLBACK]
    for key, needed in _FALLBACK_NUMBERS.items():
        if key in config and mode != needed:
            raise cv.Invalid(
                f"'{key}' is only used with '{CONF_FALLBACK}: {needed}' under '{DOMAIN}: {CONF_PROTECTION}:'",
                path=[key],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub_id = config[CONF_NOVALINK_ID]
    hub = await cg.get_variable(hub_id)
    for key in ADDRESSES:
        if (conf := config.get(key)) is None:
            continue
        var = await number.new_number(
            conf, min_value=conf[CONF_MIN_VALUE], max_value=conf[CONF_MAX_VALUE], step=1
        )
        await register_entity(var, conf, hub_id, key)
        cg.add(var.set_conversion(1, conf[CONF_OFFSET]))
        cg.add(hub.allow_write(*address_of(key, conf)))
        if key == CONF_THERMOSTAT_SETPOINT:
            cg.add(hub.set_setpoint_number(var, *address_of(key, conf)))
        if mirror := conf.get(CONF_MIRROR):
            address = (mirror[CONF_MEMORY_LOCATION], mirror[CONF_MEMORY_ADDRESS])
            cg.add(var.set_mirror(*address))
            cg.add(hub.allow_write(*address))
    for conf in config.get(CONF_CUSTOM, []):
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await register_entity(var, conf, hub_id)
        cg.add(var.set_conversion(conf[CONF_MULTIPLY], conf[CONF_OFFSET]))
        cg.add(var.set_custom(True))
    if conf := config.get(CONF_NOVALINK_HEARTBEAT):
        var = await number.new_number(conf, min_value=-20, max_value=60, step=0.1)
        cg.add(var.set_hub(hub))
    protection = CORE.config[DOMAIN][CONF_PROTECTION]
    kinds = NovaLinkProtectionNumber.enum("Kind")
    for key, setting, low, high, kind in (
        (CONF_MAXIMUM_TEMPERATURE, CONF_MAX_TEMPERATURE, 24, 38, kinds.MAXIMUM),
        (CONF_STOP_WITHOUT_NOVALINK, CONF_FALLBACK_STOP, 18, 32, kinds.FALLBACK_STOP),
        (CONF_FALLBACK_SETPOINT_NUMBER, CONF_FALLBACK_SETPOINT, 6, 30, kinds.FALLBACK_SETPOINT),
    ):
        if (conf := config.get(key)) is None:
            continue
        # The stove setpoint is a whole number of degrees
        step = 1 if key == CONF_FALLBACK_SETPOINT_NUMBER else 0.5
        var = await number.new_number(conf, min_value=low, max_value=high, step=step)
        await cg.register_component(var, conf)
        cg.add(var.set_hub(hub))
        cg.add(var.set_kind(kind))
        cg.add(var.set_initial_value(protection[setting]))
