"""Names shared by the hub and the platforms."""

import esphome.codegen as cg
from esphome.components import uart

DOMAIN = "novalink"

novalink_ns = cg.esphome_ns.namespace(DOMAIN)
NovaLink = novalink_ns.class_("NovaLink", cg.Component, uart.UARTDevice)
StoveEntity = novalink_ns.class_("StoveEntity", cg.PollingComponent)

# Hub
CONF_ALLOW_CUSTOM_WRITES = "allow_custom_writes"
CONF_PROTECTION = "protection"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_FALLBACK = "fallback"
CONF_FALLBACK_SETPOINT = "fallback_setpoint"
CONF_FALLBACK_STOP = "fallback_stop"
FALLBACK_SETPOINT = "setpoint"
FALLBACK_STOP = "stop"

# Entities
CONF_NOVALINK_ID = "novalink_id"
CONF_MEMORY_LOCATION = "memory_location"
CONF_MEMORY_ADDRESS = "memory_address"
CONF_CUSTOM = "custom"
