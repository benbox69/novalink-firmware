"""Texts read from the stove."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_UPDATE_INTERVAL, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.core import CORE

from ..boards import BOARD_X100_EVO
from ..const import CONF_NOVALINK_ID, DOMAIN, NovaLink, StoveEntity, novalink_ns
from ..entity import entity_schema, final_validate_entities, register_entity

NovaLinkStateText = novalink_ns.class_(
    "NovaLinkStateText", text_sensor.TextSensor, StoveEntity
)
NovaLinkAlarmText = novalink_ns.class_(
    "NovaLinkAlarmText", text_sensor.TextSensor, StoveEntity
)
NovaLinkDisplayText = novalink_ns.class_(
    "NovaLinkDisplayText", text_sensor.TextSensor, StoveEntity
)

CONF_ALARM = "alarm"
CONF_DISPLAY_TEXT = "display_text"
CONF_LAST_PROTECTION = "last_protection"
CONF_STOVE_STATE = "stove_state"
CONF_WINDOW = "window"

ADDRESSES = {
    CONF_STOVE_STATE: "stove_state",
    CONF_ALARM: "alarm_code",
    CONF_DISPLAY_TEXT: "display_text",
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            cv.Optional(CONF_STOVE_STATE): text_sensor.text_sensor_schema(
                NovaLinkStateText
            ).extend(entity_schema()),
            cv.Optional(CONF_ALARM): text_sensor.text_sensor_schema(
                NovaLinkAlarmText, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ).extend(entity_schema()),
            cv.Optional(CONF_LAST_PROTECTION): text_sensor.text_sensor_schema(
                icon="mdi:shield-alert", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_DISPLAY_TEXT): text_sensor.text_sensor_schema(
                NovaLinkDisplayText, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            )
            .extend(entity_schema())
            .extend(
                {
                    cv.Optional(CONF_UPDATE_INTERVAL, default="30s"): cv.update_interval,
                    # Long enough to see each of the messages that the stove alternates
                    cv.Optional(
                        CONF_WINDOW, default="5s"
                    ): cv.positive_time_period_milliseconds,
                }
            ),
        }
    ),
    cv.has_at_least_one_key(*ADDRESSES, CONF_LAST_PROTECTION),
)

FINAL_VALIDATE_SCHEMA = final_validate_entities(ADDRESSES)


async def to_code(config):
    hub_id = config[CONF_NOVALINK_ID]
    if conf := config.get(CONF_LAST_PROTECTION):
        hub = await cg.get_variable(hub_id)
        var = await text_sensor.new_text_sensor(conf)
        cg.add(hub.set_last_protection(var))
    x100_evo = CORE.config[DOMAIN][CONF_BOARD] == BOARD_X100_EVO
    for key, name in ADDRESSES.items():
        if (conf := config.get(key)) is None:
            continue
        var = await text_sensor.new_text_sensor(conf)
        hub = await register_entity(var, conf, hub_id, name)
        if key == CONF_DISPLAY_TEXT:
            cg.add(var.set_window(conf[CONF_WINDOW].total_milliseconds))
            cg.add(hub.set_display(var))
        elif key == CONF_STOVE_STATE:
            cg.add(var.set_x100_evo(x100_evo))
