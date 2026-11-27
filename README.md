# NovaLink firmware

📖 **Documentation (wiki): [English](https://github.com/benbox69/novalink-firmware/wiki) · [Français](https://github.com/benbox69/novalink-firmware/wiki/Accueil)**

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
  the stove is working or in standby, the board **stops the stove**. See [Protection](https://github.com/benbox69/novalink-firmware/wiki/Protection).
- **EEPROM wear.** The setpoint and the power level are written to the stove EEPROM, which only stands a limited number
  of writes. The component limits them (50 per 24 hours by default). The count restarts with the board. Never write
  these values in a loop.
- **X100 EVO: never write RAM 0x58.** It raised an alarm and a safety shutdown. `board: x100_evo` blocks this address;
  `board: generic` does **not**.
- **SERIAL connector: never connect its 20 V line.** See [Wiring](https://github.com/benbox69/novalink-firmware/wiki/Wiring).

## Supported stoves

| `board:` | Meaning |
|---|---|
| `x100_evo` | Micronova X100 EVO, NAVEL INSIDE port, 19200 baud. Measured on a real stove. |
| `generic` (default) | Starts from the addresses of the ESPHome `micronova` component. **Not tested by us** on a real board. You can change every address. |

Two ports are known: **NAVEL INSIDE** (`port: navel_inside`, 19200 baud) and **SERIAL** (`port: serial`, 1200 baud
8N2). The SERIAL port has not been tried on a real stove yet. On another stove, start in
[read-only mode](https://github.com/benbox69/novalink-firmware/wiki/Wiring#first-start-on-a-new-stove).

## Quick start

You need ESPHome **2026.10.0** or newer.

1. [Wire your board](https://github.com/benbox69/novalink-firmware/wiki/Wiring).
2. Copy one example: [esp32-read-only.yaml](examples/esp32-read-only.yaml) to **start with a new stove** (it only
   reads), or [novalink-board.yaml](examples/novalink-board.yaml) for the NovaLink board on an X100 EVO.
3. Add your `secrets.yaml` and run `esphome run` with that file.

The details are in [Getting started](https://github.com/benbox69/novalink-firmware/wiki/Getting-started), also for an existing ESPHome configuration.

## Documentation

| | English | Français |
|---|---|---|
| Start | [Home](https://github.com/benbox69/novalink-firmware/wiki) · [Getting started](https://github.com/benbox69/novalink-firmware/wiki/Getting-started) | [Accueil](https://github.com/benbox69/novalink-firmware/wiki/Accueil) · [Démarrage rapide](https://github.com/benbox69/novalink-firmware/wiki/D%C3%A9marrage-rapide) |
| Wiring | [Wiring](https://github.com/benbox69/novalink-firmware/wiki/Wiring) | [Câblage](https://github.com/benbox69/novalink-firmware/wiki/C%C3%A2blage) |
| Options | [Configuration](https://github.com/benbox69/novalink-firmware/wiki/Configuration) · [Entities](https://github.com/benbox69/novalink-firmware/wiki/Entities) | [Réglages](https://github.com/benbox69/novalink-firmware/wiki/R%C3%A9glages) · [Entités](https://github.com/benbox69/novalink-firmware/wiki/Entit%C3%A9s) |
| Safety net | [Protection](https://github.com/benbox69/novalink-firmware/wiki/Protection) | [Protection du poêle](https://github.com/benbox69/novalink-firmware/wiki/Protection-du-po%C3%AAle) |
| Advanced | [Free writes](https://github.com/benbox69/novalink-firmware/wiki/Free-writes) · [Stove addresses](https://github.com/benbox69/novalink-firmware/wiki/Stove-addresses) | [Écritures libres](https://github.com/benbox69/novalink-firmware/wiki/%C3%89critures-libres) · [Adresses du poêle](https://github.com/benbox69/novalink-firmware/wiki/Adresses-du-po%C3%AAle) |

## Licence

C++ files under the GPLv3, everything else under the MIT licence, as in ESPHome. The component derives from the ESPHome
`micronova` component. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

Not affiliated with or endorsed by Micronova or ESPHome.
