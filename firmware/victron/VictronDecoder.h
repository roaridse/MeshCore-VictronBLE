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
  BatteryMonitor = 0x02,
  AcCharger = 0x08,
};

enum ValidField : uint32_t {
  BatteryVoltage = 1u << 0,
  BatteryCurrent = 1u << 1,
  BatteryPower = 1u << 2,
  YieldToday = 1u << 3,
  PvPower = 1u << 4,
  Temperature = 1u << 5,
  AcCurrent = 1u << 6,
  Output2 = 1u << 7,
  Output3 = 1u << 8,
  TimeToGo = 1u << 9,
  AlarmReason = 1u << 10,
  AuxInput = 1u << 11,
  ConsumedAh = 1u << 12,
  StateOfCharge = 1u << 13,
  StarterVoltage = 1u << 14,
  MidpointVoltage = 1u << 15,
};

enum class AuxInputType : uint8_t {
  StarterVoltage = 0,
  MidpointVoltage = 1,
  Temperature = 2,
  None = 3,
};

struct Reading {
  RecordType record_type = RecordType::SolarCharger;
  uint16_t product_id = 0;
  uint16_t data_counter = 0;
  uint32_t valid = 0;
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
  uint16_t time_to_go_minutes = 0;
  uint16_t alarm_reason = 0;
  AuxInputType aux_input_type = AuxInputType::None;
  float consumed_ah = 0;
  float state_of_charge_percent = 0;
  float starter_voltage = 0;
  float midpoint_voltage = 0;
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
