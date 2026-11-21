#include "ShellyDecoder.h"

namespace shelly {
namespace {

constexpr uint8_t kBTHomeVersion = 2;

int16_t readLeS16(const uint8_t* data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8));
}

}  // namespace

bool decodeBTHomeV2(const uint8_t* data, size_t length, Reading& out) {
  out = Reading{};
  if (!data || length < 4 || data[0] != (kBTHomeServiceUuid & 0xFF) ||
      data[1] != (kBTHomeServiceUuid >> 8)) {
    return false;
  }

  const uint8_t device_info = data[2];
  if ((device_info >> 5) != kBTHomeVersion || (device_info & 0x01) != 0) {
    return false;  // Unsupported version or encrypted advertisement.
  }

  size_t offset = 3;
  while (offset < length) {
    const uint8_t id = data[offset++];
    switch (id) {
      case 0x00:  // Packet id, uint8.
        if (offset + 1 > length) return false;
        out.packet_id = data[offset++];
        out.valid |= PacketId;
        break;
      case 0x01:  // Battery, uint8 percent.
        if (offset + 1 > length) return false;
        out.battery_percent = data[offset++];
        out.valid |= Battery;
        break;
      case 0x2E:  // Humidity, uint8 percent (Shelly BLU H&T).
        if (offset + 1 > length) return false;
        out.humidity_percent = data[offset++];
        out.valid |= Humidity;
        break;
      case 0x3A:  // Button event, uint8.
        if (offset + 1 > length) return false;
        out.button_event = data[offset++];
        out.valid |= Button;
        break;
      case 0x45:  // Temperature, signed little-endian, 0.1 C.
        if (offset + 2 > length) return false;
        out.temperature_c = readLeS16(data + offset) * 0.1f;
        offset += 2;
        out.valid |= Temperature;
        break;
      case 0xF0:  // Shelly device type id.
        if (offset + 2 > length) return false;
        offset += 2;
        break;
      case 0xF1:  // Shelly four-byte firmware version.
        if (offset + 4 > length) return false;
        offset += 4;
        break;
      case 0xF2:  // Shelly three-byte firmware version.
        if (offset + 3 > length) return false;
        offset += 3;
        break;
      default:
        // BTHome has no length byte per object, so an unknown object cannot be
        // skipped safely without a complete object table.
        return false;
    }
  }

  return (out.valid & (Temperature | Humidity | Battery)) != 0;
}

}  // namespace shelly
