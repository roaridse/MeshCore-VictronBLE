# MeshCore Victron and Ruuvi BLE Sensor

Firmware for a RAK4631 that reads encrypted Victron Instant Readout and Ruuvi
RAWv2 BLE advertisements and exposes the values as MeshCore Cayenne LPP telemetry.
It can optionally publish compact status and alarm messages on a private
MeshCore channel.

The first version supports:

- Victron Solar Charger records (`0x01`), including small SmartSolar MPPTs.
- Victron AC Charger records (`0x08`), used by supported Blue Smart chargers.
- Up to four configured Victron devices.
- Up to four RuuviTags using Data Format 5 (RAWv2), with discovery from the CLI.
- Ruuvi temperature, humidity, pressure and battery voltage as Cayenne LPP.
- Native MeshCore telemetry requests and subscriptions.
- GPS through MeshCore's existing RAK4631 sensor support.
- Optional periodic private-channel status, charger-error alarms and stale-BLE alarms.
- Reproducible GitHub Actions firmware builds.
- Tagged GitHub Releases with UF2, Nordic DFU ZIP and HEX downloads.

## Privacy

Cayenne LPP is only a binary sensor-data format. It does not decide whether a
packet is public or private.

In this firmware, LPP telemetry is returned in MeshCore's encrypted peer
request/response packets. A repeater can route the packet but cannot read its
payload. The optional channel messages are encrypted with the configured
private-channel key. The node name/public key and radio routing metadata can
still be observable, as with other MeshCore nodes.

GPS is included in encrypted LPP telemetry. The build leaves advertised
latitude and longitude at zero and disables periodic flood advertisements by
default. Do not configure a real advertised location if it must remain private.

## Repository model

This is an overlay, not a fork of MeshCore. The build checks out the pinned
MeshCore commit `75b04ef06def54b348400cb1b138afb6749ffd4f`, applies one small
sensor hook patch, copies in this firmware and builds it with PlatformIO.

See [BUILDING.md](docs/BUILDING.md), [CONFIGURATION.md](docs/CONFIGURATION.md)
and [HOME_ASSISTANT.md](docs/HOME_ASSISTANT.md).

## Current status

The decoder host tests pass and the `RAK_4631_victron_sensor` target compiles.
The resulting v0.2.0 build uses about 36 KB RAM and 441 KB flash. Hardware
validation against a real RuuviTag and the exact charger models is still
required before calling it stable.

Do not commit Victron keys or private-channel keys. Keys are saved in the
RAK4631 internal filesystem and redacted by the CLI, but they are not encrypted
at rest against an attacker with physical/debug access to the device.
