# Configuration

Connect to the RAK4631 USB serial port at 115200 baud after flashing. Commands
are terminated by carriage return/newline.

## Initial MeshCore setup

Change the compiled placeholder password immediately:

```text
password A-LONG-UNIQUE-PASSWORD
set name Cabin-Victron
```

Use the normal MeshCore CLI commands to set the radio parameters used by your
local mesh. Run `help` and compare frequency, bandwidth, spreading factor and
coding rate with the Companion node connected to Home Assistant.

The firmware does not expose the Companion BLE service. Configuration is
therefore performed over USB or through the authenticated remote sensor CLI.
This avoids contention between mobile BLE connections and continuous Victron
advertisement scanning.

Remote CLI uses encrypted direct MeshCore messages and is accepted only from a
contact authenticated as an administrator. The exact button or menu depends on
the MeshCore client, but the protocol is the same remote CLI mechanism used by
repeaters. The node must first be added and authenticated with its admin
password. A remote scan examines BLE devices near the sensor node, not near the
phone or Home Assistant Companion.

## Discover and add a RuuviTag

No encryption key or prior knowledge of a RAWv2 tag is required. Leave the tag
near the node for a few seconds and run:

```text
ruuvi scan
```

The compact response contains scan slot, embedded Ruuvi MAC and RSSI, for
example `0=CB:B8:33:4C:88:4F/-57`. Inspect one result and then configure it:

```text
ruuvi scan 0
ruuvi add CB:B8:33:4C:88:4F Cabin
ruuvi list
```

Names are limited to 15 characters without spaces. Up to four tags can be
configured. Remove one with:

```text
ruuvi remove CB:B8:33:4C:88:4F
```

`ruuvi scan` retains the eight most recently observed RAWv2 tags since boot.
The MAC address comes from the RAWv2 payload, so a changing BLE advertiser
address does not affect identification. Ruuvi BLE broadcasts themselves are
unencrypted; only their onward MeshCore transport is private.

## Add a Victron device

In VictronConnect, open the charger, enable **Instant readout via Bluetooth**,
then open **Product info → Instant readout details → Show**. Copy the MAC
address and 32-hex-character encryption key.

```text
victron add AA:BB:CC:DD:EE:FF 00112233445566778899AABBCCDDEEFF MPPT
victron list
```

Names are currently limited to 15 characters without spaces. Add up to four
devices. Remove one with:

```text
victron remove AA:BB:CC:DD:EE:FF
```

`victron list` never prints a stored key. Be aware that a serial terminal may
retain the original command in scrollback or history.

## Optional private channel

Create or select a private channel in the MeshCore app on the receiving
Companion node. Enter the same name and 16-byte key in hexadecimal here:

```text
channel set VictronSensors 0123456789ABCDEFFEDCBA9876543210
channel status on
channel alarms on
channel interval 120
channel info
status now
```

The interval is in minutes and accepts 15 through 10080. Alarms are enabled by
default, but nothing is transmitted until a private channel has been set.

Clear the private-channel configuration with:

```text
channel clear
```

Repeaters do not need the channel key to forward the encrypted flood packet.
Only receivers that need to read it require the key. Channel messages are not
delivery-acknowledged and do not provide the same sender authentication as a
direct peer message; use the LPP peer path for authoritative telemetry.

## GPS

GPS uses the standard MeshCore RAK4631 sensor settings and a compatible GPS
module such as the RAK1910. Enable it through the MeshCore sensor CLI. Position
is placed in the encrypted telemetry response rather than the private text
status in this release.
