#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "VictronDecoder.h"
#include "RuuviDecoder.h"

namespace {

void putLe16(uint8_t* destination, uint16_t value) {
  destination[0] = value & 0xFF;
  destination[1] = value >> 8;
}

void putBitsLe(uint8_t* destination, size_t bit_offset, uint8_t bit_count,
               uint32_t value) {
  for (uint8_t bit = 0; bit < bit_count; ++bit) {
    const size_t target = bit_offset + bit;
    if (value & (1ul << bit)) destination[target / 8] |= 1u << (target % 8);
  }
}

bool near(float left, float right) { return fabsf(left - right) < 0.011f; }

void testSolarCharger() {
  uint8_t payload[12] = {};
  payload[0] = 3;  // Bulk
  payload[1] = 0;  // No error
  putLe16(payload + 2, 1450);  // 14.50 V
  putLe16(payload + 4, 42);    // 4.2 A
  putLe16(payload + 6, 123);   // 1.23 kWh
  putLe16(payload + 8, 87);    // 87 W PV
  putBitsLe(payload, 80, 9, 0x1FF);  // unavailable load current

  victron::Reading reading;
  assert(victron::decodePlaintext(victron::RecordType::SolarCharger,
                                  0xA054, 17, payload, sizeof(payload),
                                  reading) == victron::DecodeResult::Ok);
  assert(reading.product_id == 0xA054);
  assert(reading.device_state == 3);
  assert(reading.charger_error == 0);
  assert(near(reading.battery_voltage, 14.50f));
  assert(near(reading.battery_current, 4.2f));
  assert(near(reading.battery_power, 60.9f));
  assert(near(reading.yield_today_kwh, 1.23f));
  assert(near(reading.pv_power, 87.0f));
}

void testAcCharger() {
  uint8_t payload[13] = {};
  payload[0] = 5;  // Float
  payload[1] = 0;
  putBitsLe(payload, 16, 13, 1425);  // output 1: 14.25 V
  putBitsLe(payload, 29, 11, 80);    // output 1: 8.0 A
  putBitsLe(payload, 40, 13, 1310);  // output 2: 13.10 V
  putBitsLe(payload, 53, 11, 15);    // output 2: 1.5 A
  putBitsLe(payload, 64, 13, 0x1FFF);
  putBitsLe(payload, 77, 11, 0x7FF);
  putBitsLe(payload, 88, 7, 65);     // 25 C
  putBitsLe(payload, 95, 9, 12);     // 1.2 A AC

  victron::Reading reading;
  assert(victron::decodePlaintext(victron::RecordType::AcCharger,
                                  0xA340, 18, payload, sizeof(payload),
                                  reading) == victron::DecodeResult::Ok);
  assert(reading.device_state == 5);
  assert(near(reading.battery_voltage, 14.25f));
  assert(near(reading.battery_current, 8.0f));
  assert(near(reading.battery_power, 114.0f));
  assert(reading.valid & victron::Output2);
  assert(!(reading.valid & victron::Output3));
  assert(near(reading.output2_voltage, 13.10f));
  assert(near(reading.output2_current, 1.5f));
  assert(near(reading.temperature, 25.0f));
  assert(near(reading.ac_current, 1.2f));
}

void testInvalidLengths() {
  uint8_t payload[4] = {};
  victron::Reading reading;
  assert(victron::decodePlaintext(victron::RecordType::SolarCharger,
                                  0, 0, payload, sizeof(payload), reading) ==
         victron::DecodeResult::InvalidPayload);
}

void testRuuviRawV2() {
  const uint8_t payload[] = {
      0x05, 0x12, 0xFC, 0x53, 0x94, 0xC3, 0x7C, 0x00,
      0x04, 0xFF, 0xFC, 0x04, 0x0C, 0xAC, 0x36, 0x42,
      0x00, 0xCD, 0xCB, 0xB8, 0x33, 0x4C, 0x88, 0x4F};
  ruuvi::Reading reading;
  assert(ruuvi::decodeRawV2(payload, sizeof(payload), reading));
  assert(near(reading.temperature_c, 24.3f));
  assert(near(reading.humidity_percent, 53.49f));
  assert(near(reading.pressure_hpa, 1000.44f));
  assert(near(reading.acceleration_x_g, 0.004f));
  assert(near(reading.acceleration_y_g, -0.004f));
  assert(near(reading.acceleration_z_g, 1.036f));
  assert(near(reading.battery_voltage, 2.977f));
  assert(reading.tx_power_dbm == 4);
  assert(reading.movement_counter == 66);
  assert(reading.sequence == 205);
  const uint8_t expected_mac[] = {0xCB, 0xB8, 0x33, 0x4C, 0x88, 0x4F};
  assert(memcmp(reading.mac, expected_mac, sizeof(expected_mac)) == 0);
}

void testRuuviRejectsOtherFormats() {
  uint8_t payload[ruuvi::kRawV2Size] = {};
  payload[0] = 3;
  ruuvi::Reading reading;
  assert(!ruuvi::decodeRawV2(payload, sizeof(payload), reading));
  payload[0] = 5;
  assert(!ruuvi::decodeRawV2(payload, sizeof(payload) - 1, reading));
}

}  // namespace

int main() {
  testSolarCharger();
  testAcCharger();
  testInvalidLengths();
  testRuuviRawV2();
  testRuuviRejectsOtherFormats();
  puts("Victron and Ruuvi decoder tests passed");
  return 0;
}
