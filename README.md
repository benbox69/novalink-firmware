# NovaLink firmware

An [ESPHome](https://esphome.io/) external component for **Micronova pellet stoves**. It reads the stove and controls
it (on/off, setpoint, power level), with a safety net that runs in the board itself.

It is made for the [NovaLink board](https://github.com/benbox69/novalink), and can be used with other ESP boards and
stove interfaces. A Home Assistant integration (thermostat and more) is coming. The component works without it.

> 🚧 **Work in progress.** Tested on a computer, and on **one stove** (Micronova X100 EVO, NAVEL INSIDE port) with a
> hand-made interface, then the NovaLink V1.1 board (reads only so far). The protection stopped the stove once on
> purpose (integration lost); its other cases have not happened on a real stove.

## ⚠️ Read this first

- **A pellet stove is a fire.** Use this at your own risk. Opening the stove may void the warranty. This software does
  not replace the safety devices of the stove or a carbon monoxide detector.
- **The protection is on by default**, even without Home Assistant. If the room probe of the stove reaches 30 °C while
  the stove is working or in standby, the board **stops the stove**. See [Protection](docs/configuration.md#protection).
- **EEPROM wear.** The setpoint and the power level are written to the stove EEPROM, which only stands a limited number
  of writes. The component limits them (50 per 24 hours by default). The count restarts with the board. Never write
  these values in a loop.
- **X100 EVO: never write RAM 0x58.** It raised an alarm and a safety shutdown. `board: x100_evo` blocks this address;
  `board: generic` does **not**.
- **SERIAL connector: never connect its 20 V line.** See [Wiring](docs/wiring.md).

## Supported stoves

| `board:` | Meaning |
|---|---|
| `x100_evo` | Micronova X100 EVO, NAVEL INSIDE port, 19200 baud. Measured on a real stove. |
| `generic` (default) | Starts from the addresses of the ESPHome `micronova` component. **Not tested by us** on a real board. You can change every address. |

Two ports are known: **NAVEL INSIDE** (`port: navel_inside`, 19200 baud) and **SERIAL** (`port: serial`, 1200 baud
8N2). The SERIAL port has not been tried on a real stove yet. On another stove, start in
[read-only mode](docs/wiring.md#first-start-on-a-new-stove).

## Quick start

You need ESPHome **2026.10.0** or newer.

1. [Wire your board](docs/wiring.md).
2. Copy one example:

   | Example | For |
   |---|---|
   | [examples/esp32-read-only.yaml](examples/esp32-read-only.yaml) | Any ESP32 with your own interface. It only reads: **start here** with a new stove. Set the pins to match your interface. |
   | [examples/novalink-board.yaml](examples/novalink-board.yaml) | The NovaLink board on an X100 EVO, with everything: controls, protection, TERM relay, pellet level sensor. |

3. Next to it, create a file `secrets.yaml` with your own values for each `!secret` name used in the example. API keys
   must be [ESPHome encryption keys](https://esphome.io/components/api/).
4. Install, for example with the NovaLink board:

   ```bash
   esphome run novalink-board.yaml
   ```

About the NovaLink example:

- No pellet level sensor? Remove the `vl53l0x` sensor and the `i2c:` block.
- Not an X100 EVO? Do not use it as it is. Start with the read-only example. With `board: generic`, `brazier_cleaning`,
  `alarm_code`, `alarm` (text) and `display_text` have no known address: remove them.

### Already have an ESPHome configuration?

Add these blocks. This version only reads.

```yaml
external_components:
  - source: github://benbox69/novalink-firmware
    components: [novalink]

uart:
  tx_pin: GPIO5        # NovaLink board pins
  rx_pin: GPIO20
  baud_rate: 19200

novalink:
  port: navel_inside
  board: x100_evo      # another stove: generic
  read_only: true
  protection:
    enabled: false     # the protection needs writes

sensor:
  - platform: novalink
    room_temperature:
      name: "Room temperature"

text_sensor:
  - platform: novalink
    stove_state:
      name: "Stove state"

binary_sensor:
  - platform: novalink
    link:
      name: "Stove link"
```

To allow writes, remove `read_only` and `protection:`, and add the setpoint number (the protection needs it):

```yaml
number:
  - platform: novalink
    thermostat_setpoint:
      name: "Thermostat setpoint"
```

## Documentation

- [Configuration](docs/configuration.md): every option and entity, and the protection.
- [Wiring](docs/wiring.md): NovaLink board or your own interface.
- [Stove addresses](docs/addresses.md) and [free writes](docs/custom-writes.md).

## Licence

C++ files under the GPLv3, everything else under the MIT licence, as in ESPHome. The component derives from the ESPHome
`micronova` component. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

Not affiliated with or endorsed by Micronova or ESPHome.
