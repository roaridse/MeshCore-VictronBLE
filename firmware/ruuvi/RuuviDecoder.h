#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ruuvi {

constexpr uint16_t kCompanyId = 0x0499;
constexpr uint8_t kDataFormatRawV2 = 5;
constexpr size_t kRawV2Size = 24;

enum ValidField : uint16_t {
  Temperature = 1 << 0,
  Humidity = 1 << 1,
  Pressure = 1 << 2,
  AccelerationX = 1 << 3,
  AccelerationY = 1 << 4,
  AccelerationZ = 1 << 5,
  PowerInfo = 1 << 6,
  Movement = 1 << 7,
  Sequence = 1 << 8,
  Mac = 1 << 9,
};

struct Reading {
  uint16_t valid = 0;
  float temperature_c = 0;
  float humidity_percent = 0;
  float pressure_hpa = 0;
  float acceleration_x_g = 0;
  float acceleration_y_g = 0;
  float acceleration_z_g = 0;
  float battery_voltage = 0;
  int8_t tx_power_dbm = 0;
  uint8_t movement_counter = 0;
  uint16_t sequence = 0;
  uint8_t mac[6] = {};
};

bool decodeRawV2(const uint8_t* data, size_t length, Reading& destination);

}  // namespace ruuvi
