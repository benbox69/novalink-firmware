# Wiring

> 🔴 Unplug the stove before you open it. Connecting to the stove controller is at your own risk and may void the
> warranty.

## Which connector?

| Connector on the stove board | Setting | Speed |
|---|---|---|
| **NAVEL INSIDE** (3 pins: +5 V, SER, GND) | `port: navel_inside` | 19200 |
| **SERIAL** | `port: serial` | 1200, 8N2 |

On the X100 EVO we tested, only NAVEL INSIDE answers. The SERIAL port stays silent. The component has not been tried
on the SERIAL port of any real stove yet.

> 🔴 **Never connect the 20 V line of the SERIAL connector.** This connector has an extra supply line (about 15 to
> 20 V) next to +5 V, SER and GND. It can destroy your interface and damage the stove board. The pin order differs
> between boards: **identify every pin with a multimeter** before you connect anything.

## NovaLink board

The [NovaLink board](https://github.com/benbox69/novalink) is an ESP32-C3 SuperMini with an isolator between the ESP
and the stove. Its schematic, pinout and connectors are described in its repository.

> 🚧 One V1.1 board runs on an X100 EVO since 2026-11-26: reads checked, writes not tried yet. The earlier stove tests
> were done with a hand-made interface.

| Function | Pin |
|---|---|
| TX to stove | GPIO5 |
| RX from stove | GPIO20 |
| TERM contact (J4) | GPIO3, high = contact closed |
| Status LED | GPIO10 |
| Distance sensor (I²C SDA, SCL, XSHUT) | GPIO6, GPIO7, GPIO4 |

Stove connector J1: pin 1 = +5 V, pin 2 = SER, pin 3 = GND. The stove side is powered by the stove, the ESP by USB-C.

**No pin inversion** is needed.

```yaml
uart:
  tx_pin: GPIO5
  rx_pin: GPIO20
  baud_rate: 19200

novalink:
  port: navel_inside
  board: x100_evo   # on another stove: generic
```

For the SERIAL port, you need an adapter cable to J1, and `port: serial` with `baud_rate: 1200` and `stop_bits: 2`.
This has not been tested.

## Your own ESP32 interface

The stove uses **one wire (SER)** for both directions. Your interface must join the TX and RX pins of the ESP to this
wire, and adapt the voltage: the connector carries 5 V, an ESP32 works at 3.3 V. **Do not wire SER straight to an ESP
pin.**

What follows from the one wire:

- **Echo.** The ESP hears what it sends. Keep `echo: true` (the default).
- **Inversion.** If your circuit inverts the signal, say so in the pin settings. For example, an NPN transistor on TX
  inverts it:

  ```yaml
  uart:
    tx_pin: {number: GPIO17, inverted: true}   # NPN transistor
    rx_pin: GPIO16                             # voltage divider
    baud_rate: 19200
  ```

  The tests on the X100 EVO were done with such a hand-made circuit, with a voltage divider on RX. This repository does
  not give a tested schematic: for a ready design, see the NovaLink board.
- **Interfaces that switch reception off** (often with optocouplers, as used with the ESPHome `micronova` component):
  give `enable_rx_pin`. The pin is high while the board sends, and low to listen. `echo` is then `false` by default.
  If your optocoupler loads the stove line, add `enable_rx_idle: false`. See
  [configuration.md](configuration.md#novalink).

## Logger

If your UART uses the pins of the ESP's first serial port (for example GPIO1 and GPIO3 on an ESP8266), turn off serial
logging:

```yaml
logger:
  baud_rate: 0
```

## First start on a new stove

1. Add `read_only: true` under `novalink:`. Nothing can be written.
2. Check that the `link` binary sensor turns on.
3. Press the `memory_dump` button and keep the log.
4. Compare every value with the stove screen.
5. Only then remove `read_only`. Test the setpoint and the power level first. Test on and off last, next to the stove.
