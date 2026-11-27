# Free writes

NovaLink can write any value to any address of the stove. Most people never need it: to adapt another board, changing
the address of a known entity is enough.

## Risks

- **Alarm or safety shutdown.** Real case: two writes at RAM 0x58, taken from another board, put an X100 EVO into the
  fumes probe alarm, with the fumes fan at full speed.
- **Uncontrolled operation.** Some addresses may drive the pellet screw, the fans or the igniter: too much fuel, not
  enough air, or an ignition without supervision. This is a fire and carbon monoxide hazard.
- **Permanent change.** An EEPROM write survives a power cut and may change a factory parameter (combustion, probe,
  stove model). Without a dump made before, the original value is lost.
- **Wear.** EEPROM cells stand a limited number of writes: never write in a loop.
- **Warranty.**

## Method

1. Make a full memory dump (button `memory_dump`) and keep it.
2. Read the address with a `custom` sensor over a whole heating cycle, and compare it with the stove display.
3. Try RAM before EEPROM. One value at a time, close to the present one.
4. Stay in front of the stove, and know how to stop it without NovaLink.
5. Restore the original value, then report what you found.

## Configuration

Parts to add to your configuration (see [configuration.md](configuration.md) for every option):

```yaml
novalink:
  port: serial
  allow_custom_writes: true   # without it, the configurations below are refused

number:
  - platform: novalink
    custom:
      - name: "My setting"
        memory_location: ram      # ram or eeprom: an EEPROM write is permanent
        memory_address: 0x40
        min_value: 0
        max_value: 10
        # value = (raw - offset) x multiply
        offset: 0
        multiply: 1

button:
  - platform: novalink
    custom:
      - name: "My command"
        memory_location: ram
        memory_address: 0x41
        value: 0x07
```

Also `custom` switches (`value_on`, `value_off`), and the action `novalink.write: {location: ram, address: 0x41, value: 7}`.

> 🔴 **X100 EVO: never write RAM 0x58.** `board: x100_evo` blocks it. `board: generic` blocks no address.

What still applies: `read_only`, the addresses blocked for the board (X100 EVO: RAM 0x58), the delay and the daily limit
of the EEPROM writes, a reading before each write, and nothing written at boot. Every free write is in the log, at the
warning level. The plausibility check does not apply: only you know what the address holds.
