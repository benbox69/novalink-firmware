# Address table

Every address found, with what proves it. Stoves differ: **the same address can mean something else on another board.**

- **Default**: `board: generic`, the addresses of the ESPHome `micronova` component. Not checked by us on a board that
  uses them.
- **X100 EVO**: `board: x100_evo`, measured on 13-15 November 2026 on an X100 EVO (cod. ARIA, Siberia New 15 kW), NAVEL
  INSIDE port, 19200 baud.

Evidence: **M** measured on the stove, **E** external source only, **?** not identified, **–** not used.

## Default and X100 EVO compared

| Entity | Default | X100 EVO | Difference | Evidence (X100 EVO) |
|---|---|---|---|---|
| Serial link | SERIAL port, 1200 baud 8N2 | NAVEL INSIDE port, 19200 baud 8N1 or 8N2 | ⚠️ **different** (its SERIAL port does not answer) | M |
| Room temperature | RAM 0x01, ÷ 2 °C | RAM 0x01, ÷ 2 °C | same | M |
| Fumes temperature | RAM 0x5A, °C | RAM **0x59**, °C | ⚠️ **different** (0x5A reads 0) | M |
| Stove state | RAM 0x21 | RAM 0x21 | same | M |
| Stove power | RAM 0x34 | RAM 0x34, 1 to 5 | same | M |
| Fumes fan | RAM 0x37, × 10 + offset rpm | RAM 0x37, raw value | ⚠️ **conversion unknown** (98 at work, about 245 at most) | M (raw) |
| Water temperature | RAM 0x3B, ÷ 2 °C | – | ⚠️ air stove, not used | – |
| Water pressure | RAM 0x3C, ÷ 10 bar | – | ⚠️ **0x3C is a countdown here** (one unit per minute) | M |
| Setpoint | EEPROM 0x7D, °C | EEPROM 0x7D, °C | same | M (read, write) |
| Power level | EEPROM 0x7F | EEPROM 0x7F, 1 to 5 | same | M (read; write accepted) |
| On command | RAM 0x21 = 0x01 | RAM 0x21 = 0x01 | same | M (daily use) |
| Off command | RAM 0x21 = 0x06 | RAM 0x21 = 0x06 | same | M (daily use) |
| Brazier cleaning in | – | RAM 0x32, minutes | X100 EVO only | M |
| Display text | – | RAM 0x88-0x97, 16 ASCII characters | X100 EVO only | M |

## State codes (RAM 0x21)

| Code | Default label | Seen on the X100 EVO |
|---|---|---|
| 0 | Off | yes |
| 1 | Start | yes, once |
| 2 | Pellets loading | yes (display: CHARGE GRANULES, then ATTENTE FLAMME) |
| 3 | Ignition | yes (FEU PRESENT) |
| 4 | Working | yes |
| 5 | Brazier cleaning | yes |
| 6 | Final cleaning | yes |
| 7 | Standby | no (E) |
| 8 | No pellets alarm | yes, as a **general alarm** (SONDE FUMEE on 2026-11-14; out of pellets, says the owner, on 2026-11-24) |
| 9 | No ignition alarm | yes: alarm in memory, to be acknowledged ("MEMOIRE ALARM"), **not an ignition failure** (2026-11-24) |
| 10 | Undefined alarm | no (E) |
| other | – | 223 is read now and then on this stove: communication glitch, must be ignored |

Alarm of 2026-11-24 (out of pellets according to the owner, display not read): fumes going down from 16:20 (107 °C), state
4 until 17:11:36 (46-49 °C), then 8, then 9 at 17:15:49, still 9 an hour later. The code alone does not give the cause.
Possible reading, not checked: 8 = alarm and shutdown in progress, 9 = alarm to be acknowledged.

## Addresses never to write

| Board | Address | What happened |
|---|---|---|
| X100 EVO | RAM 0x58 | Used as an infrared remote code on other boards. Here, writing 0x52 and 0x58 raised the "fumes probe" alarm and a safety shutdown (2026-11-14) |

## Other X100 EVO addresses

| Zone | Address | Meaning | Evidence | Access |
|---|---|---|---|---|
| RAM | 0x02 | 67-74, goes down with the fumes; maybe a tank safety probe | ? | read |
| RAM | 0x0D | 8 to 15, changes during the brazier cleaning | ? | read |
| RAM | 0x19 | copy of the setpoint, °C | M | read |
| RAM | 0x27 | as 0x02 | ? | read |
| RAM | 0x2F | 10 to 15, changes during the brazier cleaning | ? | read |
| RAM | 0x36 | fumes fan target, raw | M | read |
| RAM | 0x3C | countdown, one unit per minute | ? | read |
| RAM | 0x63 to 0x69 | clock: seconds, day of the week, hour, minutes, day, month, year | M | read |
| RAM | 0xBA | follows the fumes temperature, to be confirmed | ? | read |
| EEPROM | 0x7E | 5; maybe the maximum power or a fan setting | ? | read |
| EEPROM | 0x82, 0xA0 | "NORVEGIA", configured model name | M | read (0xA0 is skipped by this firmware) |
| EEPROM | 0xF8 to 0xFD | clock in BCD: day of the week, hour, minutes, day, month, year | M | read |

## Other boards

Reports and external sources, not checked by us.

| Board or stove | Difference | Source |
|---|---|---|
| Extraflame Comfort Idro | power level at EEPROM 0x60 with offset 136, RAM copy at 0x37 | [esphome/esphome#22915](https://github.com/esphome/esphome/pull/22915) |
| Extraflame Preziosa | fumes temperature at RAM 0x02 | [esphome/esphome#12470](https://github.com/esphome/esphome/pull/12470) |

## Alarms on the X100 EVO (2026-11-24, stove in state 9, out of pellets)

Dumps made during the alarm (twice), and after the owner acknowledged it (final cleaning), compared with the one of
2026-11-14 at work (0x80 and 0xA0 skipped).

- **Display text** (RAM 0x88-0x97): the two dumps gave "MANQUE  GRANM" and "MEMOIRE GRANULES", one character at a time
  while the messages alternate. Deduced: **"MANQUE GRANULES"** (out of pellets) and **"MEMOIRE ALARM"** (alarm memory).
- **States**: 8 = alarm in progress, **9 = alarm in memory, to be acknowledged**.
- **Alarm register: RAM 0x33** (M). 0 at work on 2026-11-14, 0x10 in both alarm dumps, 0 once acknowledged.
- **Current alarm kept in EEPROM: 0xE5** (M), same values: 0, 0x10, 0x10, 0.
- **Alarm history: EEPROM 0xE0 to 0xE4** (M), latest first. 2026-11-14: FF 10 10 10 10. Now: **10 01 FF 10 10**, that is the
  alarm of 2026-11-24 (0x10), the "SONDE FUMEE" one of 2026-11-14 (0x01), then the older ones moved by two places. The
  meaning of 0xFF is not known.
- RAM 0x1F (0x10 too) does not change at the acknowledgement: not an alarm register.
- **Bits**: 0x01 = fumes probe, 0x10 = out of pellets here. The original Extraflame firmware has a register of one bit
  per cause ([OpenXtraflame](https://github.com/Shad107/OpenXtraflame), MIT, which guesses RAM 0x40): 0x01 fumes probe,
  0x02 hot fumes, 0x04 probe short-circuit, 0x08 extractor, 0x10 no ignition, 0x20 flame lost, 0x40 no draught, 0x80
  auger. 0x01 agrees with what the stove displayed; 0x10
  does not quite (out of pellets here, no ignition there); the others are not checked.
- **Counters, candidates**: EEPROM 0xEA/0xEB (low, high) 344 then 439, and 0xEC/0xED the same; 0xEE/0xEF 45 then 74.
  They look like hours of work and number of ignitions.
- EEPROM 0xF0 is a copy of the state (4, then 9).
- **Not explained**: EEPROM 0x85, the "V" of the first "NORVEGIA", reads 0 in the three dumps of 2026-11-24.

Your board is missing? Take a memory dump (button `memory_dump`, see [First start on a new stove](wiring.md#first-start-on-a-new-stove))
and report it in a GitHub issue, with what the stove display showed.
