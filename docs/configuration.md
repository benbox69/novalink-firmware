# Configuration

Every option of the `novalink` component. It needs ESPHome 2026.10.0 or newer.

- [Loading the component](#loading-the-component)
- [UART](#uart)
- [`novalink:`](#novalink)
- [Protection](#protection)
- [Addresses](#addresses)
- [Sensor](#sensor) · [Binary sensor](#binary-sensor) · [Text sensor](#text-sensor) · [Number](#number) ·
  [Switch](#switch) · [Button](#button) · [`novalink.write` action](#novalinkwrite-action)

## Loading the component

```yaml
external_components:
  - source: github://benbox69/novalink-firmware
    components: [novalink]
```

This follows the `main` branch. To stay on one version, add `@` and a tag or a branch name after the repository name.
See the ESPHome page [External Components](https://esphome.io/components/external_components/).

## UART

`novalink` uses the ESPHome [UART bus](https://esphome.io/components/uart/). Both `tx_pin` and `rx_pin` are needed.
The settings must match the `port` option. If they do not, ESPHome refuses the configuration and says what it expects.

| `port` | Stove connector | Speed | Format |
|---|---|---|---|
| `navel_inside` | NAVEL INSIDE | 19200 | 8 data bits, no parity, 1 or 2 stop bits |
| `serial` | SERIAL | 1200 | 8 data bits, no parity, 2 stop bits |
| `custom` | any other | your choice | not checked |

```yaml
uart:
  tx_pin: GPIO5
  rx_pin: GPIO20
  baud_rate: 19200   # for the SERIAL port: 1200, with stop_bits: 2
```

## `novalink:`

```yaml
novalink:
  port: navel_inside
  board: x100_evo
```

| Option | Default | Description |
|---|---|---|
| `port` | **required** | `navel_inside`, `serial` or `custom`, see [UART](#uart). |
| `board` | `generic` | `generic` or `x100_evo`, see [Addresses](#addresses). |
| `id` | automatic | Needed only if you refer to the component yourself. |
| `uart_id` | automatic | Needed only with several UART buses. |
| `read_only` | `false` | `true`: nothing is ever written to the stove. Good for a first test on an unknown stove. The protection cannot stop the stove then. The TERM contact still opens. |
| `allow_custom_writes` | `false` | Allows writes to any address. Read [custom-writes.md](custom-writes.md) first. |
| `eeprom_write_interval` | `5s` | Minimum time between two EEPROM writes. A write asked too early waits. Only the last value asked is written. |
| `eeprom_daily_limit` | `50` | Maximum number of EEPROM writes in 24 hours, `0` to `65535`. `0` means no limit. |
| `reply_timeout` | `200ms` | How long to wait for an answer of the stove. |
| `echo` | `true` | The board reads back what it sends, and checks it. This is how a one-wire interface behaves. The default is `false` when `enable_rx_pin` is set. |
| `enable_rx_pin` | none | Output pin for interfaces that can switch reception off. The pin is high while the board sends, then low to listen. |
| `enable_rx_idle` | `true` | Only with `enable_rx_pin`. `true`: reception stays on between two exchanges. `false`: it is switched off between them. |
| `protection` | on | See [Protection](#protection). |

### EEPROM writes

The stove keeps the setpoint and the power level in its **EEPROM**. An EEPROM only stands a limited number of writes.
So the component protects it:

- nothing is written when the stove already holds the value;
- two writes are at least `eeprom_write_interval` apart;
- there are at most `eeprom_daily_limit` writes in 24 hours, all addresses together, retries and free writes included.
  Above the limit, the write is refused and a warning goes to the log. The 24 hours start with the first write. The
  count starts again when the board restarts.

On and off commands go to the RAM of the stove. They are not limited.

**Do not write the setpoint or the power level in a loop**, for example from an automation that runs every few seconds.

### How a write works

1. The board reads the address first. If it cannot, nothing is written. A command is never kept to be sent later.
2. It checks that the value read makes sense: a setpoint or a power level inside the limits of the entity, a stove state
   that accepts the command. If not, nothing is written. (Free writes skip this check.)
3. It writes. For a number or a free switch, it reads the value back. For a setpoint or a power level it may try once
   more. An on or off command is never sent again by itself.

The entity then shows what the stove really holds. A refused or failed write is explained in the log.

Only the addresses of your `number` and `switch` entities can be written, plus the stove state address, used by the
protection to stop the stove. Free writes are possible only with `allow_custom_writes: true`.
Nothing is written when the board starts.

## Protection

The board watches the stove by itself. It works without Home Assistant. It **never lights the stove**, and never stops
it during an ignition or a cleaning.

> ⚠️ This is a software safety net. It does **not** replace the safety devices of the stove or a carbon monoxide
> detector.

It is **on by default**. Every 15 seconds it reads the stove state and the temperature of the stove's own room probe.
It does this even if you did not add these entities. A value older than 60 seconds counts as unknown.

### Rule 1: maximum temperature (always active)

If the stove probe reaches the maximum temperature (30 °C by default) while the stove is **working** or in
**standby**, the board stops the stove. This rule works from the first day, even without Home Assistant.

Two stops are at least 3 minutes apart.

### Rule 2: the integration is lost

This rule is for people who use the NovaLink Home Assistant integration (coming). The integration sends a
**heartbeat**: it sets the number `novalink_heartbeat` regularly. Any value set on this number counts as a heartbeat.

- **Arming.** The rule does nothing until the board has received one heartbeat. From then on it stays armed, even after
  a power cut. Without a heartbeat, the board never applies this rule. Do not set this number by hand to "try it".
- **Lost.** After `heartbeat_timeout` without a heartbeat (10 minutes by default), the integration is lost. After a
  restart, the time is counted from the start of the board.

What happens then depends on `fallback`:

| `fallback` | What the board does |
|---|---|
| `setpoint` (default) | It lowers the stove setpoint to the fallback setpoint (22 °C). The stove keeps running and regulates on its own probe. The setpoint is **never raised**. This is an EEPROM write: at most 3 attempts, 3 minutes apart, for each loss. Once done, you can change the setpoint on the stove: the board leaves it alone until the next loss. |
| `stop` | It stops the stove when its probe reaches `fallback_stop` (24 °C), while working or in standby. |

`fallback: setpoint` needs the number `thermostat_setpoint`. Without it, ESPHome refuses the configuration.

In both modes:

- **Probe lost too.** If the integration is lost and the stove probe has been unknown for `probe_timeout` (10 minutes),
  the board stops the stove, while working or in standby.
- **TERM contact.** If you give `term_switch`, the board turns this switch off (contact open).
  It does not turn it back on: that is the job of the integration.
- **2 minutes of grace.** When the stove or its probe comes back after an outage, and the integration was lost in the
  meantime, the board waits 2 minutes before it applies rule 2. The maximum temperature never waits.

When the switch `protection_without_novalink` is off, rule 2 is off, TERM included. Rule 1 stays active.

A short loss of the Home Assistant connection does not stop the stove. By default, ESPHome restarts a board that has
lost its Home Assistant connection for a while. You can give it more time:

```yaml
api:
  reboot_timeout: 1h
```

### Options

```yaml
novalink:
  port: navel_inside
  board: x100_evo
  protection:
    fallback: setpoint
    fallback_setpoint: 22
    heartbeat_timeout: 10min
```

| Option | Default | Limits | Description |
|---|---|---|---|
| `enabled` | `true` | | `false` turns the whole protection off, rule 1 included. |
| `max_temperature` | `30` | 24 to 38 °C | Rule 1. |
| `fallback` | `setpoint` | `setpoint`, `stop` | What to do when the integration is lost. |
| `fallback_setpoint` | `22` | 6 to 30, whole degrees | Setpoint given to the stove with `fallback: setpoint`. |
| `fallback_stop` | `24` | 18 to 32 °C | Stop temperature with `fallback: stop`. |
| `heartbeat_timeout` | `10min` | 2 min to 24 h | Time without heartbeat before the integration is lost. |
| `probe_timeout` | `10min` | 2 min to 24 h | Time without probe value before the "probe lost" stop. |
| `term_switch` | none | the `id` of a switch | Switch that drives the TERM contact. |

The three temperatures can also be changed from Home Assistant with the [protection numbers](#protection-numbers).
A value changed there is kept in flash, and **replaces the YAML value**.

### TERM switch

The TERM input is the external thermostat contact of the stove. The NovaLink board drives it with GPIO3:

```yaml
switch:
  - platform: gpio
    pin: GPIO3
    id: term_demand
    name: "Thermostat demand"
    restore_mode: ALWAYS_OFF

novalink:
  port: navel_inside
  board: x100_evo
  protection:
    term_switch: term_demand
```

> ⚠️ The effect of the TERM contact has not been checked on the X100 EVO.

### What was tested

Everything above is tested on a computer with a fake stove. On a real stove, the board stopped the stove once on purpose
(integration lost); the arming kept after a power cut and the TERM contact have **not** been tried yet.

## Addresses

A stove stores its values in numbered places of its memory. The `board` option picks the table of addresses:

- `x100_evo`: measured on a Micronova X100 EVO.
- `generic`: the addresses of the ESPHome `micronova` component. They have not been tested by us on a real board.

The full table is in [addresses.md](addresses.md). Two options change the address of an entity:

| Option | Values |
|---|---|
| `memory_location` | `ram` or `eeprom`. For an entity that only reads, a number from `0x00` to `0x7F` also works. For an entity that writes, only `ram` or `eeprom`. |
| `memory_address` | `0x00` to `0xFF`, except `0x80` and `0xA0` |

If your board has no known address for an entity, ESPHome refuses the configuration. Remove the entity, or give both
options.

The protection uses the same addresses as the entities `room_temperature` and `stove_state` when you change their
`memory_address`. It always reads the room temperature as the raw value divided by 2.

> 🔴 **X100 EVO: never write RAM 0x58.** On 14 November 2026, writing there raised the "fumes probe" alarm and a safety
> shutdown. With `board: x100_evo` the component blocks this address. With `board: generic` it does **not**: always set
> `board: x100_evo` on this stove.

## Sensor

```yaml
sensor:
  - platform: novalink
    room_temperature:
      name: "Room temperature"
    fumes_temperature:
      name: "Fumes temperature"
```

Add at least one key. Each key takes the usual [sensor options](https://esphome.io/components/sensor/). Except
`eeprom_writes`, each one also takes `memory_location`, `memory_address` and `update_interval` (default `10s`).

| Key | Unit | Value | `generic` | `x100_evo` |
|---|---|---|---|---|
| `room_temperature` | °C | raw ÷ 2 | yes | yes |
| `fumes_temperature` | °C | raw | yes | yes |
| `stove_power` | | raw | yes | yes |
| `state_code` | | stove state number | yes | yes |
| `fumes_fan_raw` | | raw | yes | yes |
| `fumes_fan_speed` | rpm | raw × 10 + `rpm_offset`; 0 stays 0 | yes | no |
| `thermostat_setpoint` | °C | raw (read only) | yes | yes |
| `power_level` | | raw (read only) | yes | yes |
| `water_temperature` | °C | raw ÷ 2 | yes | no |
| `water_pressure` | bar | raw ÷ 10 | yes | no |
| `brazier_cleaning` | min | time before the next brazier cleaning | no | yes |
| `alarm_code` | | raw alarm register | no | yes |
| `eeprom_writes` | | EEPROM writes sent since the board started | yes | yes |

"no" means that the board has no known address: you must give one.

- `rpm_offset` (only `fumes_fan_speed`): `0` to `255`, default `0`.
- A state code above 10 is usually a communication glitch. It is shown only when read twice in a row.

### Custom sensors

Read any address. `memory_location` and `memory_address` are required. The value is the raw byte; use ESPHome
[filters](https://esphome.io/components/sensor/#sensor-filters) to convert it.

```yaml
sensor:
  - platform: novalink
    custom:
      - name: "Fumes fan target"
        memory_location: ram
        memory_address: 0x36
        update_interval: 30s
```

## Binary sensor

```yaml
binary_sensor:
  - platform: novalink
    link:
      name: "Stove link"
    alarm:
      name: "Stove alarm"
```

Add at least one key. Each key takes the usual [binary sensor options](https://esphome.io/components/binary_sensor/).

| Key | On means | Other options |
|---|---|---|
| `link` | The stove answers. | none |
| `novalink` | The Home Assistant integration sends its heartbeat. | none |
| `alarm` | The stove state is 8, 9 or 10. | `memory_location`, `memory_address`, `update_interval` (`10s`) |

## Text sensor

```yaml
text_sensor:
  - platform: novalink
    stove_state:
      name: "Stove state"
    last_protection:
      name: "Last protection"
```

Add at least one key. Each key takes the usual [text sensor options](https://esphome.io/components/text_sensor/).

| Key | Content | `generic` | `x100_evo` |
|---|---|---|---|
| `stove_state` | The state in words | yes | yes |
| `alarm` | `None`, `Fumes probe`, `Out of pellets`, or `Code 0x..` for an unknown code | no | yes |
| `display_text` | The text on the stove screen | no | yes |
| `last_protection` | What the protection did last | yes | yes |

`stove_state`, `alarm` and `display_text` take `memory_location`, `memory_address` and `update_interval` (`10s`,
but `30s` for `display_text`).

**State texts:** `Off`, `Start`, `Pellets loading`, `Ignition`, `Working`, `Brazier cleaning`, `Final cleaning`,
`Standby`, `No pellets alarm` (8), `No ignition alarm` (9), `Undefined alarm`, `Unknown (N)`. With `x100_evo`, codes 8
and 9 both show `Alarm`: on this stove they do not tell the cause. To translate the texts, use the `map`
[filter](https://esphome.io/components/text_sensor/#text-sensor-filters).

**Display text:** the stove shows several messages in turn. The component reads the screen for a while, and joins the
messages with ` / `. The option `window` (default `5s`) is how long it reads. With the `stove_state` text sensor, the
display is also read each time the state changes.

**Last protection:** `Stopped: maximum temperature`, `Stopped: NovaLink lost`, `Stopped: NovaLink and probe lost` or
`Fallback setpoint: NovaLink lost`. It is published when it happens.

When the stove no longer answers, `stove_state`, `alarm` and `display_text` show `unknown`. Sensors and numbers become
unknown too.

## Number

```yaml
number:
  - platform: novalink
    thermostat_setpoint:
      name: "Thermostat setpoint"
    power_level:
      name: "Power level"
```

Add at least one key. Each key takes the usual [number options](https://esphome.io/components/number/).

### Stove settings

These **write to the EEPROM** of the stove, see [EEPROM writes](#eeprom-writes).

| Option | `thermostat_setpoint` | `power_level` |
|---|---|---|
| `min_value` | `6` | `1` |
| `max_value` | `40` | `5` |
| `offset` (stored value = shown value + offset) | `0` | `0` |
| `memory_location`, `memory_address` | from the board | from the board |
| `update_interval` | `10s` | `10s` |
| `mirror` | | none |

- `min_value`, `max_value` and `offset` are whole numbers from `0` to `255`. `max_value` must be greater than
  `min_value`. The step is 1.
- If the value read on the stove is outside `min_value` and `max_value`, the board does not write. The address may mean
  something else on your stove.
- `mirror` (only `power_level`): some boards apply the level only when a second address gets it too. After a successful
  write, the same value is copied there. Options: `memory_address` (required), `memory_location` (`ram` by default,
  or `eeprom`).

### Heartbeat

`novalink_heartbeat` is for the Home Assistant integration: each value set on it is a heartbeat. Range -20 to 60,
step 0.1. The value itself is not used by the protection. It does not write to the stove.
**The first heartbeat arms rule 2 for good**, see [Protection](#protection).

### Protection numbers

They change the protection from Home Assistant. The start value is the one of `protection:`. A changed value is kept in
flash.

| Key | Range | Step | Only with |
|---|---|---|---|
| `maximum_temperature` | 24 to 38 °C | 0.5 | |
| `fallback_setpoint` | 6 to 30 °C | 1 | `fallback: setpoint` |
| `stop_without_novalink` | 18 to 32 °C | 0.5 | `fallback: stop` |

The heartbeat and the protection numbers use `mode: box` by default (`mode` accepts the usual ESPHome values).

### Custom numbers (free writes)

They need `allow_custom_writes: true`. **Read [custom-writes.md](custom-writes.md) first.**

| Option | Default | Description |
|---|---|---|
| `memory_location` | required | `ram` or `eeprom` |
| `memory_address` | required | address |
| `min_value`, `max_value` | required | limits of the shown value |
| `step` | `1` | |
| `offset` | `0` | `0` to `255` |
| `multiply` | `1` | greater than 0 |
| `update_interval` | `10s` | |

Shown value = (raw − `offset`) × `multiply`.

## Switch

```yaml
switch:
  - platform: novalink
    stove:
      name: "Stove on/off"
    protection_without_novalink:
      name: "Protection without NovaLink"
```

Add at least one key. Each key takes the usual [switch options](https://esphome.io/components/switch/), except
`inverted`.

### `stove`

> ⚠️ This switch lights a real fire. Test it only while you are next to the stove.

- The switch shows what the stove reports, not what you asked. It is on in states 1 to 5 and 7.
- A start is sent only when the stove is off (state 0). A stop is sent only in states 1 to 5 and 7. In other states,
  nothing is sent.
- The board never writes it at startup, and does not restore it.

| Option | Default | Description |
|---|---|---|
| `value_on` | `0x01` | value written to start |
| `value_off` | `0x06` | value written to stop, also used by the protection |
| `memory_location`, `memory_address` | from the board | address of the stove state |
| `update_interval` | `10s` | |

Without this switch, the protection stops the stove with `0x06`.

### `protection_without_novalink`

Turns rule 2 of the [protection](#protection) on or off. It does not touch the maximum temperature. On by default; the
board remembers your choice (`restore_mode: RESTORE_DEFAULT_ON`). It does not write to the stove.

When the stove does not answer, the stove switch keeps its last state: check the `link` binary sensor.

### Custom switches (free writes)

They need `allow_custom_writes: true`. Required: `name`, `memory_location` (`ram` or `eeprom`), `memory_address`,
`value_on`, `value_off`. Optional: `update_interval` (`10s`). The switch is on when the address holds `value_on`.
Never restored at startup.

## Button

```yaml
button:
  - platform: novalink
    memory_dump:
      name: "Memory dump"
```

Add at least one key. Each key takes the usual [button options](https://esphome.io/components/button/).

- `memory_dump`: reads the whole RAM and EEPROM of the stove and prints them in the log. It writes nothing.
  `0x80` and `0xA0` are shown as `--`.
- `custom`: a list of free write buttons. Needs `allow_custom_writes: true`. Required: `name`, `memory_location`
  (`ram` or `eeprom`), `memory_address`, `value` (`0x00` to `0xFF`). The value is sent once and not read back.

## `novalink.write` action

One free write from an automation. Needs `allow_custom_writes: true`. The value is not read back.

```yaml
on_...:
  then:
    - novalink.write:
        location: ram     # ram or eeprom
        address: 0x41
        value: 7          # 0 to 255, or a lambda
```

Add `id:` only if you have given one to `novalink:` and need to name it.

