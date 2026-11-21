#include "VictronDecoder.h"

#include <math.h>
#include <string.h>

#ifdef ARDUINO
#include <AES.h>
#endif

namespace victron {
namespace {

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readBitsLe(const uint8_t* data, size_t bit_offset, uint8_t bit_count) {
  uint32_t value = 0;
  for (uint8_t bit = 0; bit < bit_count; ++bit) {
    const size_t source_bit = bit_offset + bit;
    if (data[source_bit / 8] & (1u << (source_bit % 8))) {
      value |= 1ul << bit;
    }
  }
  return value;
}

int32_t signExtend(uint32_t value, uint8_t bit_count) {
  const uint32_t sign_bit = 1ul << (bit_count - 1);
  if (value & sign_bit) value |= ~((1ul << bit_count) - 1);
  return static_cast<int32_t>(value);
}

bool decodeSigned16(int16_t raw, float scale, float& value) {
  if (raw == 0x7FFF) return false;
  value = static_cast<float>(raw) * scale;
  return true;
}

bool decodeUnsigned(uint32_t raw, uint32_t unavailable, float scale, float& value) {
  if (raw == unavailable) return false;
  value = static_cast<float>(raw) * scale;
  return true;
}

#ifdef ARDUINO
void decryptCtr(const uint8_t* ciphertext, size_t length,
                const uint8_t key[kKeySize], uint16_t counter,
                uint8_t* plaintext) {
  AES128 aes;
  uint8_t nonce[16] = {};
  uint8_t stream[16] = {};
  nonce[0] = static_cast<uint8_t>(counter & 0xFF);
  nonce[1] = static_cast<uint8_t>(counter >> 8);
  aes.setKey(key, kKeySize);
  aes.encryptBlock(stream, nonce);
  for (size_t i = 0; i < length; ++i) plaintext[i] = ciphertext[i] ^ stream[i];
}
#endif

DecodeResult decodeSolar(const uint8_t* data, size_t length, Reading& out) {
  if (length < 12) return DecodeResult::InvalidPayload;
  out.device_state = data[0];
  out.charger_error = data[1];

  float value;
  if (decodeSigned16(static_cast<int16_t>(readLe16(data + 2)), 0.01f, value)) {
    out.battery_voltage = value;
    out.valid |= BatteryVoltage;
  }
  if (decodeSigned16(static_cast<int16_t>(readLe16(data + 4)), 0.1f, value)) {
    out.battery_current = value;
    out.valid |= BatteryCurrent;
  }
  if (decodeUnsigned(readLe16(data + 6), 0xFFFF, 0.01f, value)) {
    out.yield_today_kwh = value;
    out.valid |= YieldToday;
  }
  if (decodeUnsigned(readLe16(data + 8), 0xFFFF, 1.0f, value)) {
    out.pv_power = value;
    out.valid |= PvPower;
  }
  if ((out.valid & (BatteryVoltage | BatteryCurrent)) ==
      (BatteryVoltage | BatteryCurrent)) {
    out.battery_power = out.battery_voltage * out.battery_current;
    out.valid |= BatteryPower;
  }
  return DecodeResult::Ok;
}

DecodeResult decodeAcCharger(const uint8_t* data, size_t length, Reading& out) {
  if (length < 13) return DecodeResult::InvalidPayload;
  out.device_state = data[0];
  out.charger_error = data[1];

  float value;
  const uint32_t voltage1 = readBitsLe(data, 16, 13);
  const uint32_t current1 = readBitsLe(data, 29, 11);
  const uint32_t voltage2 = readBitsLe(data, 40, 13);
  const uint32_t current2 = readBitsLe(data, 53, 11);
  const uint32_t voltage3 = readBitsLe(data, 64, 13);
  const uint32_t current3 = readBitsLe(data, 77, 11);
  const uint32_t temperature = readBitsLe(data, 88, 7);
  const uint32_t ac_current = readBitsLe(data, 95, 9);

  if (decodeUnsigned(voltage1, 0x1FFF, 0.01f, value)) {
    out.battery_voltage = value;
    out.valid |= BatteryVoltage;
  }
  if (decodeUnsigned(current1, 0x7FF, 0.1f, value)) {
    out.battery_current = value;
    out.valid |= BatteryCurrent;
  }
  if ((out.valid & (BatteryVoltage | BatteryCurrent)) ==
      (BatteryVoltage | BatteryCurrent)) {
    out.battery_power = out.battery_voltage * out.battery_current;
    out.valid |= BatteryPower;
  }
  if (voltage2 != 0x1FFF && current2 != 0x7FF) {
    out.output2_voltage = voltage2 * 0.01f;
    out.output2_current = current2 * 0.1f;
    out.valid |= Output2;
  }
  if (voltage3 != 0x1FFF && current3 != 0x7FF) {
    out.output3_voltage = voltage3 * 0.01f;
    out.output3_current = current3 * 0.1f;
    out.valid |= Output3;
  }
  if (temperature != 0x7F) {
    out.temperature = static_cast<float>(temperature) - 40.0f;
    out.valid |= Temperature;
  }
  if (decodeUnsigned(ac_current, 0x1FF, 0.1f, value)) {
    out.ac_current = value;
    out.valid |= AcCurrent;
  }
  return DecodeResult::Ok;
}

DecodeResult decodeBatteryMonitor(const uint8_t* data, size_t length,
                                  Reading& out) {
  if (length < 15) return DecodeResult::InvalidPayload;

  const uint32_t time_to_go = readBitsLe(data, 0, 16);
  const int16_t voltage = static_cast<int16_t>(readBitsLe(data, 16, 16));
  out.alarm_reason = static_cast<uint16_t>(readBitsLe(data, 32, 16));
  const uint32_t aux = readBitsLe(data, 48, 16);
  out.aux_input_type = static_cast<AuxInputType>(readBitsLe(data, 64, 2));
  const uint32_t current = readBitsLe(data, 66, 22);
  const uint32_t consumed_ah = readBitsLe(data, 88, 20);
  const uint32_t state_of_charge = readBitsLe(data, 108, 10);

  out.valid |= AlarmReason | AuxInput;
  if (time_to_go != 0xFFFF) {
    out.time_to_go_minutes = static_cast<uint16_t>(time_to_go);
    out.valid |= TimeToGo;
  }

  float value;
  if (decodeSigned16(voltage, 0.01f, value)) {
    out.battery_voltage = value;
    out.valid |= BatteryVoltage;
  }
  if (current != 0x1FFFFF) {
    out.battery_current = signExtend(current, 22) * 0.001f;
    out.valid |= BatteryCurrent;
  }
  if ((out.valid & (BatteryVoltage | BatteryCurrent)) ==
      (BatteryVoltage | BatteryCurrent)) {
    out.battery_power = out.battery_voltage * out.battery_current;
    out.valid |= BatteryPower;
  }
  if (consumed_ah != 0xFFFFF) {
    // VictronConnect displays consumed charge as a negative value.
    out.consumed_ah = -static_cast<float>(consumed_ah) * 0.1f;
    out.valid |= ConsumedAh;
  }
  if (state_of_charge != 0x3FF) {
    out.state_of_charge_percent = state_of_charge * 0.1f;
    out.valid |= StateOfCharge;
  }

  switch (out.aux_input_type) {
    case AuxInputType::StarterVoltage:
      if (decodeSigned16(static_cast<int16_t>(aux), 0.01f, value)) {
        out.starter_voltage = value;
        out.valid |= StarterVoltage;
      }
      break;
    case AuxInputType::MidpointVoltage:
      if (decodeSigned16(static_cast<int16_t>(aux), 0.01f, value)) {
        out.midpoint_voltage = value;
        out.valid |= MidpointVoltage;
      }
      break;
    case AuxInputType::Temperature:
      if (aux != 0xFFFF) {
        out.temperature = aux * 0.01f - 273.15f;
        out.valid |= Temperature;
      }
      break;
    case AuxInputType::None:
      break;
  }
  return DecodeResult::Ok;
}

}  // namespace

DecodeResult decodePlaintext(RecordType type, uint16_t product_id,
                             uint16_t data_counter, const uint8_t* plaintext,
                             size_t length, Reading& out) {
  if (!plaintext) return DecodeResult::InvalidPayload;
  out = Reading{};
  out.record_type = type;
  out.product_id = product_id;
  out.data_counter = data_counter;
  switch (type) {
    case RecordType::SolarCharger:
      return decodeSolar(plaintext, length, out);
    case RecordType::BatteryMonitor:
      return decodeBatteryMonitor(plaintext, length, out);
    case RecordType::AcCharger:
      return decodeAcCharger(plaintext, length, out);
    default:
      return DecodeResult::UnsupportedRecord;
  }
}

DecodeResult decodeAdvertisement(const uint8_t* data, size_t length,
                                 const uint8_t key[kKeySize], Reading& out) {
  if (!data || !key || length < 9) return DecodeResult::TooShort;
  if (data[0] != kProductAdvertisement) {
    return DecodeResult::NotProductAdvertisement;
  }
  if (data[7] != key[0]) return DecodeResult::WrongKey;

  const RecordType type = static_cast<RecordType>(data[4]);
  if (type != RecordType::SolarCharger &&
      type != RecordType::BatteryMonitor && type != RecordType::AcCharger) {
    return DecodeResult::UnsupportedRecord;
  }
  const size_t encrypted_length = length - 8;
  if (encrypted_length == 0 || encrypted_length > kMaxEncryptedPayload) {
    return DecodeResult::InvalidPayload;
  }
#ifdef ARDUINO
  uint8_t plaintext[kMaxEncryptedPayload] = {};
  const uint16_t counter = readLe16(data + 5);
  decryptCtr(data + 8, encrypted_length, key, counter, plaintext);
  return decodePlaintext(type, readLe16(data + 2), counter, plaintext,
                         encrypted_length, out);
#else
  (void)out;
  return DecodeResult::InvalidPayload;
#endif
}

const char* recordTypeName(RecordType type) {
  switch (type) {
    case RecordType::SolarCharger: return "solar";
    case RecordType::BatteryMonitor: return "battery";
    case RecordType::AcCharger: return "ac";
    default: return "unsupported";
  }
}

}  // namespace victron
