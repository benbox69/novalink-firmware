"""On/off of the stove, free writes, protection."""

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from ..const import (
    CONF_CUSTOM,
    CONF_MEMORY_LOCATION,
    CONF_NOVALINK_ID,
    NovaLink,
    StoveEntity,
    novalink_ns,
)
from ..entity import (
    address_of,
    entity_schema,
    final_validate_writes,
    register_entity,
    write_location,
)

NovaLinkStoveSwitch = novalink_ns.class_(
    "NovaLinkStoveSwitch", switch.Switch, StoveEntity
)
NovaLinkCustomSwitch = novalink_ns.class_(
    "NovaLinkCustomSwitch", switch.Switch, StoveEntity
)
NovaLinkFallbackSwitch = novalink_ns.class_(
    "NovaLinkFallbackSwitch", switch.Switch, cg.Component
)

CONF_PROTECTION_WITHOUT_NOVALINK = "protection_without_novalink"
CONF_STOVE = "stove"
CONF_VALUE_OFF = "value_off"
CONF_VALUE_ON = "value_on"

_BYTE = cv.hex_int_range(0x00, 0xFF)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            # No restore mode: nothing is ever written at boot
            cv.Optional(CONF_STOVE): switch.switch_schema(
                NovaLinkStoveSwitch,
                icon="mdi:fire",
                block_inverted=True,
                default_restore_mode="DISABLED",
            )
            .extend(entity_schema())
            .extend(
                {
                    cv.Optional(CONF_MEMORY_LOCATION): write_location,
                    cv.Optional(CONF_VALUE_ON, default=0x01): _BYTE,
                    cv.Optional(CONF_VALUE_OFF, default=0x06): _BYTE,
                }
            ),
            cv.Optional(CONF_CUSTOM): cv.ensure_list(
                switch.switch_schema(
                    NovaLinkCustomSwitch,
                    block_inverted=True,
                    default_restore_mode="DISABLED",
                )
                .extend(entity_schema(address_required=True))
                .extend(
                    {
                        cv.Required(CONF_MEMORY_LOCATION): write_location,
                        cv.Required(CONF_VALUE_ON): _BYTE,
                        cv.Required(CONF_VALUE_OFF): _BYTE,
                    }
                )
            ),
            cv.Optional(CONF_PROTECTION_WITHOUT_NOVALINK): switch.switch_schema(
                NovaLinkFallbackSwitch,
                icon="mdi:shield-home",
                block_inverted=True,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_ON",
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_STOVE, CONF_CUSTOM, CONF_PROTECTION_WITHOUT_NOVALINK),
)

FINAL_VALIDATE_SCHEMA = final_validate_writes({CONF_STOVE: "stove_state"}, CONF_CUSTOM)


async def to_code(config):
    hub_id = config[CONF_NOVALINK_ID]
    hub = await cg.get_variable(hub_id)
    if conf := config.get(CONF_STOVE):
        var = await switch.new_switch(conf)
        await register_entity(var, conf, hub_id, "stove_state")
        cg.add(var.set_values(conf[CONF_VALUE_ON], conf[CONF_VALUE_OFF]))
        cg.add(hub.set_off_value(conf[CONF_VALUE_OFF]))
        cg.add(hub.allow_write(*address_of("stove_state", conf)))
    for conf in config.get(CONF_CUSTOM, []):
        var = await switch.new_switch(conf)
        await register_entity(var, conf, hub_id)
        cg.add(var.set_values(conf[CONF_VALUE_ON], conf[CONF_VALUE_OFF]))
    if conf := config.get(CONF_PROTECTION_WITHOUT_NOVALINK):
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_hub(hub))
