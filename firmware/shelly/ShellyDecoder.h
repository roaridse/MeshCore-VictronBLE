#pragma once

#include <stddef.h>
#include <stdint.h>

namespace shelly {

constexpr uint16_t kBTHomeServiceUuid = 0xFCD2;

enum ValidField : uint8_t {
  PacketId = 1 << 0,
  Battery = 1 << 1,
  Humidity = 1 << 2,
  Temperature = 1 << 3,
  Button = 1 << 4,
};

struct Reading {
  uint8_t valid = 0;
  uint8_t packet_id = 0;
  uint8_t battery_percent = 0;
  uint8_t humidity_percent = 0;
  float temperature_c = 0;
  uint8_t button_event = 0;
};

// Decodes unencrypted BTHome v2 service data, including the little-endian
// 0xFCD2 UUID at the start of the buffer.
bool decodeBTHomeV2(const uint8_t* data, size_t length, Reading& destination);

}  // namespace shelly
