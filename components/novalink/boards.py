"""Addresses of the stove values, per board. See https://github.com/benbox69/novalink-firmware/wiki/Stove-addresses for what proves them."""

RAM = 0x00
EEPROM = 0x20

BOARD_GENERIC = "generic"
BOARD_X100_EVO = "x100_evo"

# Addresses of the ESPHome micronova component
GENERIC = {
    "room_temperature": (RAM, 0x01),
    "fumes_temperature": (RAM, 0x5A),
    "stove_state": (RAM, 0x21),
    "stove_power": (RAM, 0x34),
    "fumes_fan_speed": (RAM, 0x37),
    "fumes_fan_raw": (RAM, 0x37),
    "water_temperature": (RAM, 0x3B),
    "water_pressure": (RAM, 0x3C),
    "thermostat_setpoint": (EEPROM, 0x7D),
    "power_level": (EEPROM, 0x7F),
}

# Measured on a Micronova X100 EVO
X100_EVO = {
    "room_temperature": (RAM, 0x01),
    "fumes_temperature": (RAM, 0x59),
    "stove_state": (RAM, 0x21),
    "stove_power": (RAM, 0x34),
    "fumes_fan_raw": (RAM, 0x37),
    "thermostat_setpoint": (EEPROM, 0x7D),
    "power_level": (EEPROM, 0x7F),
    "brazier_cleaning": (RAM, 0x32),
    "display_text": (RAM, 0x88),
    "alarm_code": (RAM, 0x33),
}

BOARDS = {BOARD_GENERIC: GENERIC, BOARD_X100_EVO: X100_EVO}

# Never written, whatever the configuration says. X100 EVO: RAM 0x58 raised the fumes probe alarm on 2026-11-14
BLOCKED_WRITES = {BOARD_GENERIC: (), BOARD_X100_EVO: ((RAM, 0x58),)}
