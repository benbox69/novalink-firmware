"""Sensors read from the stove."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from ..const import CONF_CUSTOM, CONF_NOVALINK_ID, NovaLink, StoveEntity, novalink_ns
from ..entity import entity_schema, final_validate_entities, register_entity

NovaLinkSensor = novalink_ns.class_("NovaLinkSensor", sensor.Sensor, StoveEntity)
NovaLinkStateSensor = novalink_ns.class_(
    "NovaLinkStateSensor", sensor.Sensor, StoveEntity
)

CONF_EEPROM_WRITES = "eeprom_writes"
CONF_RPM_OFFSET = "rpm_offset"
UNIT_BAR = "bar"


def _temperature(decimals):
    return sensor.sensor_schema(
        NovaLinkSensor,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=decimals,
    )


# key -> (schema, multiplier)
TYPES = {
    "room_temperature": (_temperature(1), 0.5),
    "fumes_temperature": (_temperature(0), 1),
    "water_temperature": (_temperature(1), 0.5),
    "water_pressure": (
        sensor.sensor_schema(
            NovaLinkSensor,
            unit_of_measurement=UNIT_BAR,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        0.1,
    ),
    "stove_power": (
        sensor.sensor_schema(
            NovaLinkSensor, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=0
        ),
        1,
    ),
    "fumes_fan_speed": (
        sensor.sensor_schema(
            NovaLinkSensor,
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ).extend({cv.Optional(CONF_RPM_OFFSET, default=0): cv.int_range(0, 255)}),
        10,
    ),
    "fumes_fan_raw": (
        sensor.sensor_schema(
            NovaLinkSensor, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=0
        ),
        1,
    ),
    "thermostat_setpoint": (_temperature(0), 1),
    "power_level": (
        sensor.sensor_schema(NovaLinkSensor, accuracy_decimals=0),
        1,
    ),
    "brazier_cleaning": (
        sensor.sensor_schema(
            NovaLinkSensor, unit_of_measurement=UNIT_MINUTE, accuracy_decimals=0
        ),
        1,
    ),
    "alarm_code": (
        sensor.sensor_schema(
            NovaLinkSensor,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        1,
    ),
    "state_code": (
        sensor.sensor_schema(NovaLinkStateSensor, accuracy_decimals=0),
        1,
    ),
}

ADDRESSES = {key: key for key in TYPES} | {"state_code": "stove_state"}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            **{
                cv.Optional(key): schema.extend(entity_schema())
                for key, (schema, _) in TYPES.items()
            },
            # Since boot, sent by this board
            cv.Optional(CONF_EEPROM_WRITES): sensor.sensor_schema(
                icon="mdi:counter",
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CUSTOM): cv.ensure_list(
                sensor.sensor_schema(NovaLinkSensor, accuracy_decimals=0).extend(
                    entity_schema(address_required=True)
                )
            ),
        }
    ),
    cv.has_at_least_one_key(*TYPES, CONF_CUSTOM, CONF_EEPROM_WRITES),
)

FINAL_VALIDATE_SCHEMA = final_validate_entities(ADDRESSES)


async def to_code(config):
    hub_id = config[CONF_NOVALINK_ID]
    for key, (_, multiplier) in TYPES.items():
        if (conf := config.get(key)) is None:
            continue
        var = await sensor.new_sensor(conf)
        await register_entity(var, conf, hub_id, ADDRESSES[key])
        if key != "state_code":
            cg.add(var.set_multiplier(multiplier))
        if CONF_RPM_OFFSET in conf:
            cg.add(var.set_running_offset(conf[CONF_RPM_OFFSET]))
    if conf := config.get(CONF_EEPROM_WRITES):
        hub = await cg.get_variable(hub_id)
        var = await sensor.new_sensor(conf)
        cg.add(hub.set_eeprom_writes_sensor(var))
    for conf in config.get(CONF_CUSTOM, []):
        var = await sensor.new_sensor(conf)
        await register_entity(var, conf, hub_id)
