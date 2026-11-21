#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "VictronDecoder.h"
#include "RuuviDecoder.h"
#include "ShellyDecoder.h"

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

void testBatteryMonitorTemperature() {
  uint8_t payload[15] = {};
  putBitsLe(payload, 0, 16, 125);        // 125 minutes remaining
  putBitsLe(payload, 16, 16, 1278);      // 12.78 V
  putBitsLe(payload, 32, 16, 0x0024);    // Alarm reason bitfield
  putBitsLe(payload, 48, 16, 29815);     // 298.15 K = 25.00 C
  putBitsLe(payload, 64, 2, 2);          // Temperature aux mode
  putBitsLe(payload, 66, 22,
            static_cast<uint32_t>(-12345) & 0x3FFFFF);  // -12.345 A
  putBitsLe(payload, 88, 20, 456);       // -45.6 Ah as displayed
  putBitsLe(payload, 108, 10, 873);      // 87.3% SOC

  victron::Reading reading;
  assert(victron::decodePlaintext(victron::RecordType::BatteryMonitor,
                                  0xA389, 19, payload, sizeof(payload),
                                  reading) == victron::DecodeResult::Ok);
  assert(strcmp(victron::recordTypeName(reading.record_type), "battery") == 0);
  assert(reading.time_to_go_minutes == 125);
  assert(near(reading.battery_voltage, 12.78f));
  assert(near(reading.battery_current, -12.345f));
  assert(near(reading.battery_power, -157.769f));
  assert(reading.alarm_reason == 0x0024);
  assert(near(reading.consumed_ah, -45.6f));
  assert(near(reading.state_of_charge_percent, 87.3f));
  assert(reading.aux_input_type == victron::AuxInputType::Temperature);
  assert(near(reading.temperature, 25.0f));
}

void testBatteryMonitorMidpointAndUnavailable() {
  uint8_t payload[15] = {};
  putBitsLe(payload, 0, 16, 0xFFFF);     // Time to go unavailable
  putBitsLe(payload, 16, 16, 2661);      // 26.61 V
  putBitsLe(payload, 48, 16, 1330);      // 13.30 V midpoint
  putBitsLe(payload, 64, 2, 1);          // Midpoint aux mode
  putBitsLe(payload, 66, 22, 0x1FFFFF);  // Current unavailable
  putBitsLe(payload, 88, 20, 0xFFFFF);   // Consumed Ah unavailable
  putBitsLe(payload, 108, 10, 0x3FF);    // SOC unavailable

  victron::Reading reading;
  assert(victron::decodePlaintext(victron::RecordType::BatteryMonitor,
                                  0xC039, 20, payload, sizeof(payload),
                                  reading) == victron::DecodeResult::Ok);
  assert(near(reading.battery_voltage, 26.61f));
  assert(near(reading.midpoint_voltage, 13.30f));
  assert(reading.valid & victron::MidpointVoltage);
  assert(!(reading.valid & victron::BatteryCurrent));
  assert(!(reading.valid & victron::StateOfCharge));
  assert(!(reading.valid & victron::TimeToGo));
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

void testShellyBluHtBTHomeV2() {
  // UUID FCD2, unencrypted BTHome v2, packet 42, battery 95%, humidity
  // 51%, button absent, temperature -4.3 C.
  const uint8_t payload[] = {
      0xD2, 0xFC, 0x40, 0x00, 0x2A, 0x01, 0x5F,
      0x2E, 0x33, 0x45, 0xD5, 0xFF};
  shelly::Reading reading;
  assert(shelly::decodeBTHomeV2(payload, sizeof(payload), reading));
  assert(reading.packet_id == 42);
  assert(reading.battery_percent == 95);
  assert(reading.humidity_percent == 51);
  assert(near(reading.temperature_c, -4.3f));
  assert((reading.valid & shelly::Button) == 0);
}

void testShellyBluHtButtonAndDeviceInfo() {
  const uint8_t payload[] = {
      0xD2, 0xFC, 0x44, 0x01, 0x64, 0x2E, 0x31, 0x3A, 0x01,
      0x45, 0xD7, 0x00, 0xF0, 0x11, 0x00, 0xF2, 0x01, 0x02, 0x03};
  shelly::Reading reading;
  assert(shelly::decodeBTHomeV2(payload, sizeof(payload), reading));
  assert(reading.battery_percent == 100);
  assert(reading.humidity_percent == 49);
  assert(near(reading.temperature_c, 21.5f));
  assert(reading.button_event == 1);
}

void testShellyRejectsEncryptedAndMalformed() {
  const uint8_t encrypted[] = {0xD2, 0xFC, 0x41, 0x00, 0x01};
  shelly::Reading reading;
  assert(!shelly::decodeBTHomeV2(encrypted, sizeof(encrypted), reading));
  const uint8_t truncated[] = {0xD2, 0xFC, 0x40, 0x45, 0xD7};
  assert(!shelly::decodeBTHomeV2(truncated, sizeof(truncated), reading));
}

}  // namespace

int main() {
  testSolarCharger();
  testAcCharger();
  testBatteryMonitorTemperature();
  testBatteryMonitorMidpointAndUnavailable();
  testInvalidLengths();
  testRuuviRawV2();
  testRuuviRejectsOtherFormats();
  testShellyBluHtBTHomeV2();
  testShellyBluHtButtonAndDeviceInfo();
  testShellyRejectsEncryptedAndMalformed();
  puts("Victron, Ruuvi and Shelly decoder tests passed");
  return 0;
}
