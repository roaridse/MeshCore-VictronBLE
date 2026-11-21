# Home Assistant

Use the `meshcore-dev/meshcore-ha` integration with a separate Companion radio
connected to Home Assistant over USB, BLE or TCP. Discover the sensor node,
authenticate/add it as a tracked node and keep the telemetry update interval at
7200 seconds for a two-hour polling interval.

The telemetry request and response are peer-encrypted. Cayenne LPP channel
numbers identify the values as follows:

| Device slot | Base channel |
|---|---:|
| 0 | 20 |
| 1 | 28 |
| 2 | 36 |
| 3 | 44 |

## Solar charger layout

For base channel `N`:

| Channel | LPP values |
|---|---|
| N | Battery voltage, current and calculated power |
| N+1 | PV power |
| N+2 | Yield today (energy) |
| N+3 | Device state (generic sensor) |
| N+4 | Charger error code (generic sensor) |

## AC charger layout

| Channel | LPP values |
|---|---|
| N | Output 1 voltage, current and calculated power |
| N+1 | Output 2 voltage, current and calculated power, when available |
| N+2 | Output 3 voltage, current and calculated power, when available |
| N+3 | Charger temperature |
| N+4 | AC input current |
| N+5 | Device state (generic sensor) |
| N+6 | Charger error code (generic sensor) |

The integration creates entities when it first receives each channel/type
combination. Slot numbers are stable in flash until a device is removed and a
new device reuses the free slot.

## RuuviTag layout

Ruuvi slots use base channels 60, 62, 64 and 66:

| Channel | LPP values |
|---|---|
| N | Temperature, relative humidity and barometric pressure |
| N+1 | Battery voltage |

RAWv2 acceleration, movement counter, sequence number, transmit power and RSSI
are decoded for diagnostics but are not included in regular LPP telemetry. This
keeps the radio payload small when several tags are configured. RSSI is present
in optional private-channel status messages.

## Private-channel messages

Configure the same private channel and key on the Home Assistant Companion
radio. Status messages have a compact key/value format such as:

```text
Victron Sensor: STATUS device=MPPT type=solar voltage=13.72 current=2.4 power=33 state=3 error=0 rssi=-61
BLE Sensor: STATUS device=Cabin type=ruuvi temp=21.4 humidity=48.2 pressure=1007.3 battery=2.977 rssi=-57
```

Alarm messages use `ALARM`, and clearing a charger error produces `RECOVERY`.
Both Victron and Ruuvi devices emit a stale-BLE alarm after 15 minutes without
a valid configured-device reading.
These arrive as MeshCore message events. They are deliberately secondary to
native LPP telemetry; an HA automation can notify on them without having to
turn every status line into a sensor entity.
