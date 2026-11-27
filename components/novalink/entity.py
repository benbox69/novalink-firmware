"""What every entity that reads or writes a stove address needs: its address, checks and registration."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BOARD
from esphome.core import CORE
import esphome.final_validate as fv

from .boards import BLOCKED_WRITES, BOARDS, EEPROM, RAM
from .const import (
    CONF_ALLOW_CUSTOM_WRITES,
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    DOMAIN,
)

DEFAULT_UPDATE_INTERVAL = "10s"


# ---- Address validators ----


def memory_location(value):
    """ram, eeprom, or a number below 0x80 (the write bit)."""
    if isinstance(value, str) and value.lower() in ("ram", "eeprom"):
        return RAM if value.lower() == "ram" else EEPROM
    return cv.hex_int_range(min=0x00, max=0x7F)(value)


def memory_address(value):
    value = cv.hex_int_range(min=0x00, max=0xFF)(value)
    if value in (0x80, 0xA0):
        raise cv.Invalid(
            "0x80 and 0xA0 cannot be read: if a byte was lost, the stove would take them for a write code"
        )
    return value


def write_location(value):
    """A permanent write must be a visible choice."""
    if str(value).lower() not in ("ram", "eeprom"):
        raise cv.Invalid("Use 'ram' or 'eeprom' (an EEPROM write is permanent)")
    return memory_location(value)


def entity_schema(address_required=False):
    """Address (default: the one of the board, see boards.py) and update interval of an entity."""
    key = cv.Required if address_required else cv.Optional
    return cv.Schema(
        {
            key(CONF_MEMORY_LOCATION): memory_location,
            key(CONF_MEMORY_ADDRESS): memory_address,
        }
    ).extend(cv.polling_component_schema(DEFAULT_UPDATE_INTERVAL))


# ---- Address of an entity ----


def _resolve(board, name, config):
    """Address set in the configuration, else the one of the board for `name`. (None, None) when unknown."""
    default = BOARDS[board].get(name)
    location = config.get(CONF_MEMORY_LOCATION, default[0] if default else None)
    address = config.get(CONF_MEMORY_ADDRESS, default[1] if default else None)
    return location, address


def address_of(name, config):
    """(location, address) of an entity, during code generation."""
    return _resolve(CORE.config[DOMAIN][CONF_BOARD], name, config)


# ---- Final checks, once the whole configuration is known ----


def final_validate_entities(addresses):
    """Every entity must have an address on the board. `addresses`: configuration key -> name in boards.py."""

    def validate(config):
        hub = fv.full_config.get().get(DOMAIN)
        if not hub:
            return config
        board = hub[CONF_BOARD]
        for key, name in addresses.items():
            if key not in config:
                continue
            if None in _resolve(board, name, config[key]):
                raise cv.Invalid(
                    f"No known address for '{key}' on board '{board}': remove it, or set "
                    f"{CONF_MEMORY_LOCATION} and {CONF_MEMORY_ADDRESS}",
                    path=[key],
                )
        return config

    return validate


def final_validate_writes(addresses, custom=None):
    """Same as final_validate_entities, and refuses the blocked addresses. `custom`: key of the free writes."""
    check_addresses = final_validate_entities(addresses)

    def validate(config):
        config = check_addresses(config)
        hub = fv.full_config.get().get(DOMAIN)
        if not hub:
            return config
        board = hub[CONF_BOARD]
        items = [
            (key, _resolve(board, name, config[key]))
            for key, name in addresses.items()
            if key in config
        ]
        if custom and config.get(custom):
            if not hub[CONF_ALLOW_CUSTOM_WRITES]:
                raise cv.Invalid(
                    f"Free writes need '{CONF_ALLOW_CUSTOM_WRITES}: true' under '{DOMAIN}:'. Read "
                    "https://github.com/benbox69/novalink-firmware/wiki/Free-writes first: a wrong write can stop the stove "
                    "or change it for good",
                    path=[custom],
                )
            items += [(custom, _resolve(board, None, c)) for c in config[custom]]
        for key, address in items:
            if address in BLOCKED_WRITES[board]:
                raise cv.Invalid(
                    f"0x{address[0]:02X}:0x{address[1]:02X} must never be written on board '{board}'",
                    path=[key],
                )
        return config

    return validate


# ---- Code generation ----


def entity_count():
    """Polling entities registered so far: sizes the lists of the hub."""
    return CORE.data.setdefault(DOMAIN, {"entities": 0})["entities"]


async def register_entity(var, config, hub_id, name=None):
    """Polling entity reading one stove address. `name`: its default address in boards.py. Returns the hub."""
    hub = await cg.get_variable(hub_id)
    location, address = address_of(name, config)
    await cg.register_component(var, config)
    cg.add(var.set_address(location, address))
    cg.add(var.set_hub(hub))
    cg.add(hub.register_entity(var))
    CORE.data.setdefault(DOMAIN, {"entities": 0})["entities"] += 1
    return hub

