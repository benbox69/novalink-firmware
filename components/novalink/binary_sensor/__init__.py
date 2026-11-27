"""Link with the stove, alarm."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from ..const import CONF_NOVALINK_ID, NovaLink, StoveEntity, novalink_ns
from ..entity import entity_schema, final_validate_entities, register_entity

NovaLinkAlarmBinarySensor = novalink_ns.class_(
    "NovaLinkAlarmBinarySensor", binary_sensor.BinarySensor, StoveEntity
)

CONF_ALARM = "alarm"
CONF_LINK = "link"
CONF_NOVALINK = "novalink"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            cv.Optional(CONF_LINK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # The Home Assistant integration reports
            cv.Optional(CONF_NOVALINK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ALARM): binary_sensor.binary_sensor_schema(
                NovaLinkAlarmBinarySensor, device_class=DEVICE_CLASS_PROBLEM
            ).extend(entity_schema()),
        }
    ),
    cv.has_at_least_one_key(CONF_LINK, CONF_NOVALINK, CONF_ALARM),
)

FINAL_VALIDATE_SCHEMA = final_validate_entities({CONF_ALARM: "stove_state"})


async def to_code(config):
    hub_id = config[CONF_NOVALINK_ID]
    if conf := config.get(CONF_LINK):
        hub = await cg.get_variable(hub_id)
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_link_sensor(var))
    if conf := config.get(CONF_NOVALINK):
        hub = await cg.get_variable(hub_id)
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_alive_sensor(var))
    if conf := config.get(CONF_ALARM):
        var = await binary_sensor.new_binary_sensor(conf)
        await register_entity(var, conf, hub_id, "stove_state")
