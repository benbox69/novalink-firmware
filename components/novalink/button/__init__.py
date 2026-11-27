"""Memory dump to the log (read only), free writes."""

import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_VALUE, ENTITY_CATEGORY_DIAGNOSTIC

from ..const import (
    CONF_CUSTOM,
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_NOVALINK_ID,
    NovaLink,
    novalink_ns,
)
from ..entity import final_validate_writes, memory_address, write_location

NovaLinkDumpButton = novalink_ns.class_("NovaLinkDumpButton", button.Button)
NovaLinkCustomButton = novalink_ns.class_("NovaLinkCustomButton", button.Button)

CONF_MEMORY_DUMP = "memory_dump"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_NOVALINK_ID): cv.use_id(NovaLink),
            cv.Optional(CONF_MEMORY_DUMP): button.button_schema(
                NovaLinkDumpButton,
                icon="mdi:memory",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CUSTOM): cv.ensure_list(
                button.button_schema(NovaLinkCustomButton).extend(
                    {
                        cv.Required(CONF_MEMORY_LOCATION): write_location,
                        cv.Required(CONF_MEMORY_ADDRESS): memory_address,
                        cv.Required(CONF_VALUE): cv.hex_int_range(0x00, 0xFF),
                    }
                )
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_MEMORY_DUMP, CONF_CUSTOM),
)

FINAL_VALIDATE_SCHEMA = final_validate_writes({}, CONF_CUSTOM)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NOVALINK_ID])
    if conf := config.get(CONF_MEMORY_DUMP):
        var = await button.new_button(conf)
        cg.add(var.set_hub(hub))
    for conf in config.get(CONF_CUSTOM, []):
        var = await button.new_button(conf)
        cg.add(var.set_hub(hub))
        cg.add(
            var.set_write(
                conf[CONF_MEMORY_LOCATION], conf[CONF_MEMORY_ADDRESS], conf[CONF_VALUE]
            )
        )
