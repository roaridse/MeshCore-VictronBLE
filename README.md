# MeshCore Victron, Ruuvi and Shelly BLE Sensor

Firmware for RAK4631, Heltec T114 (without display), and RAK WisMesh Tag that
reads encrypted Victron Instant Readout, Ruuvi RAWv2 and Shelly BLU H&T BTHome
advertisements and exposes the values as MeshCore Cayenne LPP telemetry.
It can optionally publish compact status and alarm messages on a private
MeshCore channel.

The first version supports:

- Victron Solar Charger records (`0x01`), including small SmartSolar MPPTs.
- Victron Battery Monitor records (`0x02`), including BMV-712 and SmartShunt.
- Victron AC Charger records (`0x08`), used by supported Blue Smart chargers.
- Up to four configured Victron devices.
- Up to four RuuviTags using Data Format 5 (RAWv2), with discovery from the CLI.
- Ruuvi temperature, humidity, pressure and battery voltage as Cayenne LPP.
- Up to four Shelly BLU H&T sensors using unencrypted BTHome v2.
- Shelly temperature, humidity and battery percentage as Cayenne LPP.
- Native MeshCore telemetry requests and subscriptions.
- GPS through MeshCore's existing board support where compatible GPS hardware is present.
- Optional periodic private-channel status, charger/battery alarms and stale-BLE alarms.
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

The decoder host tests pass and all three firmware targets compile. RAK4631 has
partial hardware validation; Heltec T114 and RAK WisMesh Tag are build-verified
and still require hardware validation. The Ruuvi path has been validated on
RAK4631 hardware. Shelly BLU H&T, BMV-712, SmartShunt and the exact charger
models still require hardware validation before calling them stable.

Do not commit Victron keys or private-channel keys. Keys are saved in the
internal filesystem and redacted by the CLI, but they are not encrypted
at rest against an attacker with physical/debug access to the device.
