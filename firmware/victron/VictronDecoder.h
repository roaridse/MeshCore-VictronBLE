#pragma once

#include <stddef.h>
#include <stdint.h>

namespace victron {

constexpr uint16_t kCompanyId = 0x02E1;
constexpr uint8_t kProductAdvertisement = 0x10;
constexpr size_t kKeySize = 16;
constexpr size_t kMaxEncryptedPayload = 16;

enum class RecordType : uint8_t {
  SolarCharger = 0x01,
  AcCharger = 0x08,
};

enum ValidField : uint16_t {
  BatteryVoltage = 1u << 0,
  BatteryCurrent = 1u << 1,
  BatteryPower = 1u << 2,
  YieldToday = 1u << 3,
  PvPower = 1u << 4,
  Temperature = 1u << 5,
  AcCurrent = 1u << 6,
  Output2 = 1u << 7,
  Output3 = 1u << 8,
};

struct Reading {
  RecordType record_type = RecordType::SolarCharger;
  uint16_t product_id = 0;
  uint16_t data_counter = 0;
  uint16_t valid = 0;
  uint8_t device_state = 0xFF;
  uint8_t charger_error = 0xFF;
  float battery_voltage = 0;
  float battery_current = 0;
  float battery_power = 0;
  float yield_today_kwh = 0;
  float pv_power = 0;
  float temperature = 0;
  float ac_current = 0;
  float output2_voltage = 0;
  float output2_current = 0;
  float output3_voltage = 0;
  float output3_current = 0;
};

enum class DecodeResult : uint8_t {
  Ok,
  TooShort,
  NotProductAdvertisement,
  WrongKey,
  UnsupportedRecord,
  InvalidPayload,
};

// manufacturer_data starts after the two-byte Bluetooth company identifier.
DecodeResult decodeAdvertisement(const uint8_t* manufacturer_data, size_t length,
                                 const uint8_t key[kKeySize], Reading& out);

// Exposed for deterministic host tests and for adding packet fixtures.
DecodeResult decodePlaintext(RecordType type, uint16_t product_id,
                             uint16_t data_counter, const uint8_t* plaintext,
                             size_t length, Reading& out);

const char* recordTypeName(RecordType type);

}  // namespace victron
