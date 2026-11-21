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

## BMV-712 and SmartShunt layout

Battery monitors use Victron record type `0x02` and the same slot/base-channel
table as the chargers:

| Channel | LPP values |
|---|---|
| N | Battery voltage, current and calculated power |
| N+1 | State of charge (percentage, rounded to a whole percent) |
| N+2 | Consumed Ah (generic sensor, negative as displayed by VictronConnect) |
| N+3 | Time to go in minutes (generic sensor) |
| N+4 | Alarm-reason bitfield (generic sensor) |
| N+5 | Auxiliary starter/midpoint voltage or battery temperature |
| N+6 | Auxiliary input type: 0 starter, 1 midpoint, 2 temperature, 3 none |

Unavailable values, such as SOC before the monitor has been synchronised, are
left out of the LPP response. The auxiliary value on `N+5` changes LPP type
according to how the BMV/SmartShunt input is configured.

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

## Shelly BLU H&T layout

Shelly slots use base channels 70, 72, 74 and 76:

| Channel | LPP values |
|---|---|
| N | Temperature and relative humidity |
| N+1 | Battery percentage |

The packet counter, button event and RSSI are not included in regular LPP
telemetry. RSSI is included in the optional private-channel status message.

## Private-channel messages

Configure the same private channel and key on the Home Assistant Companion
radio. Status messages have a compact key/value format such as:

```text
Victron Sensor: STATUS device=MPPT type=solar voltage=13.72 current=2.4 power=33 state=3 error=0 rssi=-61
Victron Sensor: STATUS device=Shunt type=battery voltage=13.28 current=-1.245 soc=86.4 consumed=-42.7 tgo=680 alarm=0 rssi=-58
BLE Sensor: STATUS device=Cabin type=ruuvi temp=21.4 humidity=48.2 pressure=1007.3 battery=2.977 rssi=-57
BLE Sensor: STATUS device=Galley type=shelly temp=20.8 humidity=47 battery=96 rssi=-63
```

Alarm messages use `ALARM`, and clearing a charger error or battery-monitor
alarm produces `RECOVERY`.
Victron, Ruuvi and Shelly devices emit a stale-BLE alarm after 15 minutes
without a valid configured-device reading.
These arrive as MeshCore message events. They are deliberately secondary to
native LPP telemetry; an HA automation can notify on them without having to
turn every status line into a sensor entity.
