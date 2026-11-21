#include "RuuviDecoder.h"

#include <string.h>

namespace ruuvi {
namespace {

uint16_t readBe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) << 8 | data[1];
}

int16_t readBeS16(const uint8_t* data) {
  return static_cast<int16_t>(readBe16(data));
}

bool allValue(const uint8_t* data, size_t length, uint8_t value) {
  for (size_t i = 0; i < length; ++i) {
    if (data[i] != value) return false;
  }
  return true;
}

}  // namespace

bool decodeRawV2(const uint8_t* data, size_t length, Reading& out) {
  out = Reading{};
  if (!data || length < kRawV2Size || data[0] != kDataFormatRawV2) return false;

  const int16_t temperature = readBeS16(data + 1);
  if (temperature != INT16_MIN) {
    out.temperature_c = temperature * 0.005f;
    out.valid |= Temperature;
  }

  const uint16_t humidity = readBe16(data + 3);
  if (humidity != UINT16_MAX) {
    out.humidity_percent = humidity * 0.0025f;
    out.valid |= Humidity;
  }

  const uint16_t pressure = readBe16(data + 5);
  if (pressure != UINT16_MAX) {
    out.pressure_hpa = (pressure + 50000ul) / 100.0f;
    out.valid |= Pressure;
  }

  const int16_t acceleration[] = {
      readBeS16(data + 7), readBeS16(data + 9), readBeS16(data + 11)};
  float* acceleration_out[] = {
      &out.acceleration_x_g, &out.acceleration_y_g, &out.acceleration_z_g};
  const uint16_t acceleration_bits[] = {
      AccelerationX, AccelerationY, AccelerationZ};
  for (uint8_t i = 0; i < 3; ++i) {
    if (acceleration[i] != INT16_MIN) {
      *acceleration_out[i] = acceleration[i] / 1000.0f;
      out.valid |= acceleration_bits[i];
    }
  }

  const uint16_t power = readBe16(data + 13);
  if (power != UINT16_MAX) {
    out.battery_voltage = ((power >> 5) + 1600) / 1000.0f;
    out.tx_power_dbm = static_cast<int8_t>((power & 0x1F) * 2 - 40);
    out.valid |= PowerInfo;
  }

  if (data[15] != UINT8_MAX) {
    out.movement_counter = data[15];
    out.valid |= Movement;
  }
  out.sequence = readBe16(data + 16);
  if (out.sequence != UINT16_MAX) out.valid |= Sequence;

  memcpy(out.mac, data + 18, sizeof(out.mac));
  if (!allValue(out.mac, sizeof(out.mac), 0xFF) &&
      !allValue(out.mac, sizeof(out.mac), 0x00)) {
    out.valid |= Mac;
  }
  return (out.valid & Mac) != 0;
}

}  // namespace ruuvi
