#include "SensorMesh.h"

#include "VictronConfig.h"
#include "VictronScanner.h"
#include "RuuviConfig.h"
#include "RuuviScanner.h"
#include "ShellyConfig.h"
#include "ShellyScanner.h"

namespace {

constexpr uint8_t kFirstVictronChannel = 20;
constexpr uint8_t kChannelsPerDevice = 8;
constexpr uint8_t kFirstRuuviChannel = 60;
constexpr uint8_t kRuuviChannelsPerDevice = 2;
constexpr uint8_t kFirstShellyChannel = 70;
constexpr uint8_t kShellyChannelsPerDevice = 2;
constexpr uint32_t kReadingMaxAgeMs = 15ul * 60ul * 1000ul;

victron::ConfigStore victron_config;
victron::Scanner victron_scanner;
ruuvi::ConfigStore ruuvi_config;
ruuvi::Scanner ruuvi_scanner;
shelly::ConfigStore shelly_config;
shelly::Scanner shelly_scanner;

bool readingIsFresh(const victron::DeviceState& state) {
  return state.has_reading && static_cast<uint32_t>(millis() - state.received_at) <= kReadingMaxAgeMs;
}

bool readingIsFresh(const ruuvi::DeviceState& state) {
  return state.has_reading && static_cast<uint32_t>(millis() - state.received_at) <= kReadingMaxAgeMs;
}

bool readingIsFresh(const shelly::DeviceState& state) {
  return state.has_reading && static_cast<uint32_t>(millis() - state.received_at) <= kReadingMaxAgeMs;
}

void addPowerTuple(CayenneLPP& lpp, uint8_t channel, float voltage,
                   float current, float power) {
  lpp.addVoltage(channel, voltage);
  lpp.addCurrent(channel, current);
  lpp.addPower(channel, power);
}

class VictronSensorMesh : public SensorMesh {
 public:
  VictronSensorMesh(mesh::MainBoard& board, mesh::Radio& radio,
                    mesh::MillisecondClock& ms, mesh::RNG& rng,
                    mesh::RTCClock& rtc, mesh::MeshTables& tables)
      : SensorMesh(board, radio, ms, rng, rtc, tables) {
    memset(previous_alarm_, 0xFF, sizeof(previous_alarm_));
    memset(stale_alarm_sent_, 0, sizeof(stale_alarm_sent_));
    memset(ruuvi_stale_alarm_sent_, 0, sizeof(ruuvi_stale_alarm_sent_));
    memset(shelly_stale_alarm_sent_, 0, sizeof(shelly_stale_alarm_sent_));
  }

 protected:
  void appendCustomTelemetry(CayenneLPP& lpp) override {
    for (uint8_t i = 0; i < victron::kMaxDevices; ++i) {
      if (!victron_config.device(i).enabled) continue;
      const auto& state = victron_scanner.state(i);
      if (!readingIsFresh(state)) continue;
      const auto& value = state.reading;
      const uint8_t base = kFirstVictronChannel + i * kChannelsPerDevice;

      if ((value.valid & (victron::BatteryVoltage | victron::BatteryCurrent |
                          victron::BatteryPower)) ==
          (victron::BatteryVoltage | victron::BatteryCurrent |
           victron::BatteryPower)) {
        addPowerTuple(lpp, base, value.battery_voltage, value.battery_current,
                      value.battery_power);
      }
      if (value.record_type == victron::RecordType::SolarCharger) {
        if (value.valid & victron::PvPower) lpp.addPower(base + 1, value.pv_power);
        if (value.valid & victron::YieldToday) lpp.addEnergy(base + 2, value.yield_today_kwh);
        lpp.addGenericSensor(base + 3, value.device_state);
        lpp.addGenericSensor(base + 4, value.charger_error);
      } else if (value.record_type == victron::RecordType::AcCharger) {
        if (value.valid & victron::Output2) {
          addPowerTuple(lpp, base + 1, value.output2_voltage,
                        value.output2_current,
                        value.output2_voltage * value.output2_current);
        }
        if (value.valid & victron::Output3) {
          addPowerTuple(lpp, base + 2, value.output3_voltage,
                        value.output3_current,
                        value.output3_voltage * value.output3_current);
        }
        if (value.valid & victron::Temperature) lpp.addTemperature(base + 3, value.temperature);
        if (value.valid & victron::AcCurrent) lpp.addCurrent(base + 4, value.ac_current);
        lpp.addGenericSensor(base + 5, value.device_state);
        lpp.addGenericSensor(base + 6, value.charger_error);
      } else if (value.record_type == victron::RecordType::BatteryMonitor) {
        if (value.valid & victron::StateOfCharge) {
          lpp.addPercentage(base + 1,
                            static_cast<uint32_t>(value.state_of_charge_percent + 0.5f));
        }
        if (value.valid & victron::ConsumedAh) {
          lpp.addGenericSensor(base + 2, value.consumed_ah);
        }
        if (value.valid & victron::TimeToGo) {
          lpp.addGenericSensor(base + 3, value.time_to_go_minutes);
        }
        lpp.addGenericSensor(base + 4, value.alarm_reason);
        if (value.valid & victron::StarterVoltage) {
          lpp.addVoltage(base + 5, value.starter_voltage);
        } else if (value.valid & victron::MidpointVoltage) {
          lpp.addVoltage(base + 5, value.midpoint_voltage);
        } else if (value.valid & victron::Temperature) {
          lpp.addTemperature(base + 5, value.temperature);
        }
        lpp.addGenericSensor(base + 6,
                             static_cast<uint8_t>(value.aux_input_type));
      }
    }

    for (uint8_t i = 0; i < ruuvi::kMaxDevices; ++i) {
      if (!ruuvi_config.device(i).enabled) continue;
      const auto& state = ruuvi_scanner.state(i);
      if (!readingIsFresh(state)) continue;
      const auto& value = state.reading;
      const uint8_t base = kFirstRuuviChannel + i * kRuuviChannelsPerDevice;
      if (value.valid & ruuvi::Temperature) lpp.addTemperature(base, value.temperature_c);
      if (value.valid & ruuvi::Humidity) lpp.addRelativeHumidity(base, value.humidity_percent);
      if (value.valid & ruuvi::Pressure) lpp.addBarometricPressure(base, value.pressure_hpa);
      if (value.valid & ruuvi::PowerInfo) lpp.addVoltage(base + 1, value.battery_voltage);
    }

    for (uint8_t i = 0; i < shelly::kMaxDevices; ++i) {
      if (!shelly_config.device(i).enabled) continue;
      const auto& state = shelly_scanner.state(i);
      if (!readingIsFresh(state)) continue;
      const auto& value = state.reading;
      const uint8_t base = kFirstShellyChannel + i * kShellyChannelsPerDevice;
      if (value.valid & shelly::Temperature) lpp.addTemperature(base, value.temperature_c);
      if (value.valid & shelly::Humidity) lpp.addRelativeHumidity(base, value.humidity_percent);
      if (value.valid & shelly::Battery) lpp.addPercentage(base + 1, value.battery_percent);
    }
  }

  void onSensorDataRead() override {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < victron::kMaxDevices; ++i) {
      if (!victron_config.device(i).enabled) continue;
      const auto& state = victron_scanner.state(i);
      if (readingIsFresh(state)) {
        stale_alarm_sent_[i] = false;
        const uint16_t alarm = state.reading.record_type ==
                                       victron::RecordType::BatteryMonitor
                                   ? state.reading.alarm_reason
                                   : state.reading.charger_error;
        if (previous_alarm_[i] != alarm) {
          if (previous_alarm_[i] != 0xFFFF &&
              victron_config.channel().alarms_enabled) {
            sendAlarmChange(i, alarm);
          } else if (previous_alarm_[i] == 0xFFFF && alarm != 0 &&
                     victron_config.channel().alarms_enabled) {
            sendAlarmChange(i, alarm);
          }
          previous_alarm_[i] = alarm;
        }
      } else if (state.has_reading && !stale_alarm_sent_[i] &&
                 victron_config.channel().alarms_enabled) {
        char message[96];
        snprintf(message, sizeof(message), "ALARM device=%s reason=ble_stale age_min=%lu",
                 victron_config.device(i).name,
                 static_cast<unsigned long>((now - state.received_at) / 60000ul));
        sendConfiguredChannel(message);
        stale_alarm_sent_[i] = true;
      }
    }

    for (uint8_t i = 0; i < ruuvi::kMaxDevices; ++i) {
      if (!ruuvi_config.device(i).enabled) continue;
      const auto& state = ruuvi_scanner.state(i);
      if (readingIsFresh(state)) {
        ruuvi_stale_alarm_sent_[i] = false;
      } else if (state.has_reading && !ruuvi_stale_alarm_sent_[i] &&
                 victron_config.channel().alarms_enabled) {
        char message[96];
        snprintf(message, sizeof(message), "ALARM device=%s reason=ble_stale age_min=%lu",
                 ruuvi_config.device(i).name,
                 static_cast<unsigned long>((now - state.received_at) / 60000ul));
        sendConfiguredChannel(message);
        ruuvi_stale_alarm_sent_[i] = true;
      }
    }

    for (uint8_t i = 0; i < shelly::kMaxDevices; ++i) {
      if (!shelly_config.device(i).enabled) continue;
      const auto& state = shelly_scanner.state(i);
      if (readingIsFresh(state)) {
        shelly_stale_alarm_sent_[i] = false;
      } else if (state.has_reading && !shelly_stale_alarm_sent_[i] &&
                 victron_config.channel().alarms_enabled) {
        char message[96];
        snprintf(message, sizeof(message), "ALARM device=%s reason=ble_stale age_min=%lu",
                 shelly_config.device(i).name,
                 static_cast<unsigned long>((now - state.received_at) / 60000ul));
        sendConfiguredChannel(message);
        shelly_stale_alarm_sent_[i] = true;
      }
    }

    if (send_status_now_ || statusDue(now)) {
      send_status_now_ = false;
      sendStatus();
      last_status_at_ = now;
    }
  }

  int querySeriesData(uint32_t, uint32_t, MinMaxAvg[], int) override { return 0; }

  bool handleCustomCommand(uint32_t, char* command, char* reply) override {
    if (strcmp(command, "victron list") == 0) {
      Serial.printf("Configured Victron devices (%u/%u):\n", victron_config.count(),
                    victron::kMaxDevices);
      for (uint8_t i = 0; i < victron::kMaxDevices; ++i) {
        if (!victron_config.device(i).enabled) continue;
        char mac[18];
        victron::ConfigStore::formatMac(victron_config.device(i).mac, mac);
        const auto& state = victron_scanner.state(i);
        Serial.printf("%u: %s %s key=<redacted> seen=%s packets=%lu\n", i,
                      mac, victron_config.device(i).name,
                      state.has_reading ? "yes" : "no",
                      static_cast<unsigned long>(state.packets));
      }
      strcpy(reply, "OK - details on serial console");
      return true;
    }
    if (strncmp(command, "victron add ", 12) == 0) {
      return addDevice(command + 12, reply);
    }
    if (strncmp(command, "victron remove ", 15) == 0) {
      uint8_t mac[6];
      if (!victron::ConfigStore::parseMac(command + 15, mac)) {
        strcpy(reply, "Err - use MAC AA:BB:CC:DD:EE:FF");
      } else if (!victron_config.remove(mac)) {
        strcpy(reply, "Err - device not found");
      } else {
        strcpy(reply, victron_config.save() ? "OK" : "Err - save failed");
      }
      return true;
    }
    if (strcmp(command, "ruuvi scan") == 0) {
      return listRuuviScan(reply);
    }
    if (strncmp(command, "ruuvi scan ", 11) == 0) {
      return showRuuviScan(command + 11, reply);
    }
    if (strcmp(command, "ruuvi list") == 0) {
      return listRuuvi(reply);
    }
    if (strncmp(command, "ruuvi add ", 10) == 0) {
      return addRuuvi(command + 10, reply);
    }
    if (strncmp(command, "ruuvi remove ", 13) == 0) {
      uint8_t mac[6];
      if (!victron::ConfigStore::parseMac(command + 13, mac)) {
        strcpy(reply, "Err - use MAC AA:BB:CC:DD:EE:FF");
      } else if (!ruuvi_config.remove(mac)) {
        strcpy(reply, "Err - device not found");
      } else {
        strcpy(reply, ruuvi_config.save() ? "OK" : "Err - save failed");
      }
      return true;
    }
    if (strcmp(command, "shelly scan") == 0) {
      return listShellyScan(reply);
    }
    if (strncmp(command, "shelly scan ", 12) == 0) {
      return showShellyScan(command + 12, reply);
    }
    if (strcmp(command, "shelly list") == 0) {
      return listShelly(reply);
    }
    if (strncmp(command, "shelly add ", 11) == 0) {
      return addShelly(command + 11, reply);
    }
    if (strncmp(command, "shelly remove ", 14) == 0) {
      uint8_t mac[6];
      if (!victron::ConfigStore::parseMac(command + 14, mac)) {
        strcpy(reply, "Err - use MAC AA:BB:CC:DD:EE:FF");
      } else if (!shelly_config.remove(mac)) {
        strcpy(reply, "Err - device not found");
      } else {
        strcpy(reply, shelly_config.save() ? "OK" : "Err - save failed");
      }
      return true;
    }
    if (strncmp(command, "channel set ", 12) == 0) {
      return setChannel(command + 12, reply);
    }
    if (strcmp(command, "channel clear") == 0) {
      victron_config.channel() = victron::ChannelConfig{};
      strcpy(reply, victron_config.save() ? "OK" : "Err - save failed");
      return true;
    }
    if (strncmp(command, "channel status ", 15) == 0) {
      return setBoolean(command + 15, victron_config.channel().status_enabled, reply);
    }
    if (strncmp(command, "channel alarms ", 15) == 0) {
      return setBoolean(command + 15, victron_config.channel().alarms_enabled, reply);
    }
    if (strncmp(command, "channel interval ", 17) == 0) {
      const int minutes = atoi(command + 17);
      if (minutes < 15 || minutes > 10080) {
        strcpy(reply, "Err - interval must be 15..10080 minutes");
      } else {
        victron_config.channel().interval_minutes = minutes;
        strcpy(reply, victron_config.save() ? "OK" : "Err - save failed");
      }
      return true;
    }
    if (strcmp(command, "channel info") == 0) {
      snprintf(reply, 160, "name=%s key=%s status=%s alarms=%s interval=%u",
               victron_config.channel().name,
               victron_config.channelConfigured() ? "<redacted>" : "unset",
               victron_config.channel().status_enabled ? "on" : "off",
               victron_config.channel().alarms_enabled ? "on" : "off",
               victron_config.channel().interval_minutes);
      return true;
    }
    if (strcmp(command, "status now") == 0) {
      send_status_now_ = true;
      strcpy(reply, "OK - queued");
      return true;
    }
    if (strcmp(command, "victron help") == 0) {
      strcpy(reply, "victron add/remove/list; ruuvi or shelly scan/add/remove/list; channel set/clear/info/status/alarms/interval; status now");
      return true;
    }
    if (strcmp(command, "ruuvi help") == 0) {
      strcpy(reply, "ruuvi scan [slot]; ruuvi add MAC NAME; ruuvi remove MAC; ruuvi list");
      return true;
    }
    if (strcmp(command, "shelly help") == 0) {
      strcpy(reply, "shelly scan [slot]; shelly add MAC NAME; shelly remove MAC; shelly list");
      return true;
    }
    return false;
  }

 private:
  uint16_t previous_alarm_[victron::kMaxDevices];
  bool stale_alarm_sent_[victron::kMaxDevices];
  bool ruuvi_stale_alarm_sent_[ruuvi::kMaxDevices];
  bool shelly_stale_alarm_sent_[shelly::kMaxDevices];
  bool send_status_now_ = false;
  uint32_t last_status_at_ = 0;

  bool statusDue(uint32_t now) const {
    if (!victron_config.channel().status_enabled || !victron_config.channelConfigured()) return false;
    const uint32_t interval = static_cast<uint32_t>(victron_config.channel().interval_minutes) * 60000ul;
    return last_status_at_ == 0 || static_cast<uint32_t>(now - last_status_at_) >= interval;
  }

  bool sendConfiguredChannel(const char* text) {
    if (!victron_config.channelConfigured()) return false;
    mesh::GroupChannel channel;
    victron_config.makeGroupChannel(channel);
    return sendChannelMessage(channel, text);
  }

  void sendStatus() {
    if (!victron_config.channel().status_enabled || !victron_config.channelConfigured()) return;
    for (uint8_t i = 0; i < victron::kMaxDevices; ++i) {
      if (!victron_config.device(i).enabled) continue;
      const auto& state = victron_scanner.state(i);
      char message[160];
      if (!readingIsFresh(state)) {
        snprintf(message, sizeof(message), "STATUS device=%s available=0",
                 victron_config.device(i).name);
      } else if (state.reading.record_type ==
                 victron::RecordType::BatteryMonitor) {
        const auto& v = state.reading;
        if (v.valid & victron::StateOfCharge) {
          const int time_to_go = (v.valid & victron::TimeToGo)
                                     ? v.time_to_go_minutes
                                     : -1;
          snprintf(message, sizeof(message),
                   "STATUS device=%s type=battery voltage=%.2f current=%.3f soc=%.1f consumed=%.1f tgo=%d alarm=%u rssi=%d",
                   victron_config.device(i).name, v.battery_voltage,
                   v.battery_current, v.state_of_charge_percent, v.consumed_ah,
                   time_to_go, v.alarm_reason, state.rssi);
        } else {
          snprintf(message, sizeof(message),
                   "STATUS device=%s type=battery voltage=%.2f current=%.3f soc=na alarm=%u rssi=%d",
                   victron_config.device(i).name, v.battery_voltage,
                   v.battery_current, v.alarm_reason, state.rssi);
        }
      } else {
        const auto& v = state.reading;
        snprintf(message, sizeof(message),
                 "STATUS device=%s type=%s voltage=%.2f current=%.1f power=%.0f state=%u error=%u rssi=%d",
                 victron_config.device(i).name, victron::recordTypeName(v.record_type),
                 v.battery_voltage, v.battery_current, v.battery_power,
                 v.device_state, v.charger_error, state.rssi);
      }
      sendConfiguredChannel(message);
    }
    for (uint8_t i = 0; i < ruuvi::kMaxDevices; ++i) {
      if (!ruuvi_config.device(i).enabled) continue;
      const auto& state = ruuvi_scanner.state(i);
      char message[144];
      if (!readingIsFresh(state)) {
        snprintf(message, sizeof(message), "STATUS device=%s type=ruuvi available=0",
                 ruuvi_config.device(i).name);
      } else {
        const auto& v = state.reading;
        snprintf(message, sizeof(message),
                 "STATUS device=%s type=ruuvi temp=%.1f humidity=%.1f pressure=%.1f battery=%.3f rssi=%d",
                 ruuvi_config.device(i).name, v.temperature_c,
                 v.humidity_percent, v.pressure_hpa, v.battery_voltage,
                 state.rssi);
      }
      sendConfiguredChannel(message);
    }
    for (uint8_t i = 0; i < shelly::kMaxDevices; ++i) {
      if (!shelly_config.device(i).enabled) continue;
      const auto& state = shelly_scanner.state(i);
      char message[128];
      if (!readingIsFresh(state)) {
        snprintf(message, sizeof(message), "STATUS device=%s type=shelly available=0",
                 shelly_config.device(i).name);
      } else {
        const auto& v = state.reading;
        snprintf(message, sizeof(message),
                 "STATUS device=%s type=shelly temp=%.1f humidity=%u battery=%u rssi=%d",
                 shelly_config.device(i).name, v.temperature_c,
                 v.humidity_percent, v.battery_percent, state.rssi);
      }
      sendConfiguredChannel(message);
    }
  }

  void sendAlarmChange(uint8_t index, uint16_t alarm) {
    char message[96];
    const auto& reading = victron_scanner.state(index).reading;
    if (reading.record_type == victron::RecordType::BatteryMonitor) {
      snprintf(message, sizeof(message), "%s device=%s alarm_reason=%u",
               alarm == 0 ? "RECOVERY" : "ALARM",
               victron_config.device(index).name, alarm);
    } else {
      snprintf(message, sizeof(message), "%s device=%s charger_error=%u",
               alarm == 0 ? "RECOVERY" : "ALARM",
               victron_config.device(index).name, alarm);
    }
    sendConfiguredChannel(message);
  }

  bool addDevice(char* parameters, char* reply) {
    char* mac_text = strtok(parameters, " ");
    char* key_text = strtok(nullptr, " ");
    char* name = strtok(nullptr, " ");
    uint8_t mac[6];
    uint8_t key[victron::kKeySize];
    if (!mac_text || !key_text ||
        !victron::ConfigStore::parseMac(mac_text, mac) ||
        !victron::ConfigStore::parseHexKey(key_text, key)) {
      strcpy(reply, "Err - victron add MAC 32_HEX_KEY NAME");
      return true;
    }
    const int index = victron_config.addOrUpdate(mac, key, name);
    if (index < 0) {
      strcpy(reply, "Err - device table full");
    } else if (!victron_config.save()) {
      strcpy(reply, "Err - save failed");
    } else {
      snprintf(reply, 160, "OK - device slot %d (key redacted)", index);
    }
    return true;
  }

  bool listRuuviScan(char* reply) {
    if (ruuvi_scanner.discoveredCount() == 0) {
      strcpy(reply, "No RAWv2 tags seen yet");
      return true;
    }
    size_t used = 0;
    for (uint8_t i = 0; i < ruuvi::kMaxDiscovered; ++i) {
      const auto& found = ruuvi_scanner.discovered(i);
      if (!found.occupied) continue;
      char mac[18];
      victron::ConfigStore::formatMac(found.reading.mac, mac);
      const int written = snprintf(reply + used, 160 - used, "%s%u=%s/%d",
                                   used ? " " : "", i, mac, found.rssi);
      if (written < 0 || static_cast<size_t>(written) >= 160 - used) break;
      used += written;
    }
    return true;
  }

  bool showRuuviScan(const char* slot_text, char* reply) {
    char* end = nullptr;
    const long slot = strtol(slot_text, &end, 10);
    if (!slot_text[0] || *end || slot < 0 || slot >= ruuvi::kMaxDiscovered ||
        !ruuvi_scanner.discovered(slot).occupied) {
      strcpy(reply, "Err - scan slot not found; use ruuvi scan");
      return true;
    }
    const auto& found = ruuvi_scanner.discovered(slot);
    char mac[18];
    victron::ConfigStore::formatMac(found.reading.mac, mac);
    snprintf(reply, 160,
             "slot=%ld mac=%s temp=%.2f humidity=%.2f pressure=%.1f battery=%.3f rssi=%d configured=%s",
             slot, mac, found.reading.temperature_c, found.reading.humidity_percent,
             found.reading.pressure_hpa, found.reading.battery_voltage, found.rssi,
             ruuvi_config.find(found.reading.mac) >= 0 ? "yes" : "no");
    return true;
  }

  bool listRuuvi(char* reply) {
    if (ruuvi_config.count() == 0) {
      strcpy(reply, "No Ruuvi tags configured");
      return true;
    }
    size_t used = 0;
    for (uint8_t i = 0; i < ruuvi::kMaxDevices; ++i) {
      if (!ruuvi_config.device(i).enabled) continue;
      char mac[18];
      victron::ConfigStore::formatMac(ruuvi_config.device(i).mac, mac);
      const int written = snprintf(reply + used, 160 - used, "%s%u=%s/%s/%s",
                                   used ? " " : "", i, mac,
                                   ruuvi_config.device(i).name,
                                   ruuvi_scanner.state(i).has_reading ? "seen" : "unseen");
      if (written < 0 || static_cast<size_t>(written) >= 160 - used) break;
      used += written;
    }
    return true;
  }

  bool addRuuvi(char* parameters, char* reply) {
    char* mac_text = strtok(parameters, " ");
    char* name = strtok(nullptr, " ");
    uint8_t mac[6];
    if (!mac_text || !victron::ConfigStore::parseMac(mac_text, mac)) {
      strcpy(reply, "Err - ruuvi add MAC NAME");
      return true;
    }
    const int index = ruuvi_config.addOrUpdate(mac, name);
    if (index < 0) strcpy(reply, "Err - Ruuvi table full");
    else if (!ruuvi_config.save()) strcpy(reply, "Err - save failed");
    else snprintf(reply, 160, "OK - Ruuvi slot %d", index);
    return true;
  }

  bool listShellyScan(char* reply) {
    if (shelly_scanner.discoveredCount() == 0) {
      strcpy(reply, "No unencrypted Shelly/BTHome sensors seen yet");
      return true;
    }
    size_t used = 0;
    for (uint8_t i = 0; i < shelly::kMaxDiscovered; ++i) {
      const auto& found = shelly_scanner.discovered(i);
      if (!found.occupied) continue;
      char mac[18];
      victron::ConfigStore::formatMac(found.mac, mac);
      const int written = snprintf(reply + used, 160 - used, "%s%u=%s/%d",
                                   used ? " " : "", i, mac, found.rssi);
      if (written < 0 || static_cast<size_t>(written) >= 160 - used) break;
      used += written;
    }
    return true;
  }

  bool showShellyScan(const char* slot_text, char* reply) {
    char* end = nullptr;
    const long slot = strtol(slot_text, &end, 10);
    if (!slot_text[0] || *end || slot < 0 || slot >= shelly::kMaxDiscovered ||
        !shelly_scanner.discovered(slot).occupied) {
      strcpy(reply, "Err - scan slot not found; use shelly scan");
      return true;
    }
    const auto& found = shelly_scanner.discovered(slot);
    char mac[18];
    victron::ConfigStore::formatMac(found.mac, mac);
    snprintf(reply, 160,
             "slot=%ld mac=%s temp=%.1f humidity=%u battery=%u rssi=%d configured=%s",
             slot, mac, found.reading.temperature_c,
             found.reading.humidity_percent, found.reading.battery_percent,
             found.rssi, shelly_config.find(found.mac) >= 0 ? "yes" : "no");
    return true;
  }

  bool listShelly(char* reply) {
    if (shelly_config.count() == 0) {
      strcpy(reply, "No Shelly sensors configured");
      return true;
    }
    size_t used = 0;
    for (uint8_t i = 0; i < shelly::kMaxDevices; ++i) {
      if (!shelly_config.device(i).enabled) continue;
      char mac[18];
      victron::ConfigStore::formatMac(shelly_config.device(i).mac, mac);
      const int written = snprintf(reply + used, 160 - used, "%s%u=%s/%s/%s",
                                   used ? " " : "", i, mac,
                                   shelly_config.device(i).name,
                                   shelly_scanner.state(i).has_reading ? "seen" : "unseen");
      if (written < 0 || static_cast<size_t>(written) >= 160 - used) break;
      used += written;
    }
    return true;
  }

  bool addShelly(char* parameters, char* reply) {
    char* mac_text = strtok(parameters, " ");
    char* name = strtok(nullptr, " ");
    uint8_t mac[6];
    if (!mac_text || !victron::ConfigStore::parseMac(mac_text, mac)) {
      strcpy(reply, "Err - shelly add MAC NAME");
      return true;
    }
    const int index = shelly_config.addOrUpdate(mac, name);
    if (index < 0) strcpy(reply, "Err - Shelly table full");
    else if (!shelly_config.save()) strcpy(reply, "Err - save failed");
    else snprintf(reply, 160, "OK - Shelly slot %d", index);
    return true;
  }

  bool setChannel(char* parameters, char* reply) {
    char* name = strtok(parameters, " ");
    char* key_text = strtok(nullptr, " ");
    uint8_t key[16];
    if (!name || !key_text || !victron::ConfigStore::parseHexKey(key_text, key)) {
      strcpy(reply, "Err - channel set NAME 32_HEX_KEY");
      return true;
    }
    StrHelper::strncpy(victron_config.channel().name, name,
                       sizeof(victron_config.channel().name));
    memcpy(victron_config.channel().key, key, sizeof(key));
    strcpy(reply, victron_config.save() ? "OK - key redacted" : "Err - save failed");
    return true;
  }

  bool setBoolean(const char* value, bool& destination, char* reply) {
    if (strcmp(value, "on") == 0) destination = true;
    else if (strcmp(value, "off") == 0) destination = false;
    else {
      strcpy(reply, "Err - use on or off");
      return true;
    }
    strcpy(reply, victron_config.save() ? "OK" : "Err - save failed");
    return true;
  }
};

StdRNG fast_rng;
SimpleMeshTables tables;
VictronSensorMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng,
                           rtc_clock, tables);

char command[180];

void halt() {
  while (true) delay(1000);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  board.begin();
  if (!radio_init()) halt();
  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* filesystem;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  filesystem = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  filesystem = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  filesystem = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
#error "Unsupported filesystem"
#endif

  if (!store.load("_main", the_mesh.self_id)) {
    the_mesh.self_id = radio_new_identity();
    store.save("_main", the_mesh.self_id);
  }

  command[0] = 0;
  sensors.begin();
  the_mesh.begin(filesystem);
  if (!victron_config.begin(filesystem)) {
    Serial.println("Victron config invalid; defaults loaded");
  }
  if (!ruuvi_config.begin(filesystem)) {
    Serial.println("Ruuvi config invalid; defaults loaded");
  }
  if (!shelly_config.begin(filesystem)) {
    Serial.println("Shelly config invalid; defaults loaded");
  }
  ruuvi_scanner.begin(&ruuvi_config);
  shelly_scanner.begin(&shelly_config);
  if (!victron_scanner.begin(&victron_config, &ruuvi_scanner, &shelly_scanner)) {
    Serial.println("Victron BLE scanner failed to start");
  }

#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif
  Serial.println("Victron/Ruuvi/Shelly Mesh sensor ready; type 'shelly help'");
}

void loop() {
  size_t length = strlen(command);
  while (Serial.available() && length < sizeof(command) - 1) {
    const char c = Serial.read();
    if (c != '\n') command[length++] = c;
    command[length] = 0;
    // Echo every received byte, as the upstream MeshCore sensor CLI does.
    // Besides making an interactive terminal easier to use, this gives the
    // web configurator an unambiguous indication that USB RX is working even
    // if a command itself produces no textual reply.
    Serial.print(c);
  }
  if (length == sizeof(command) - 1) command[length - 1] = '\r';
  if (length > 0 && command[length - 1] == '\r') {
    command[length - 1] = 0;
    char reply[180] = {};
    the_mesh.handleCommand(nullptr, 0, command, reply);
    if (reply[0]) Serial.println(reply);
    command[0] = 0;
  }
  // Keep board-specific watchdog and power-management housekeeping running.
  // This is also present in MeshCore's upstream simple_sensor loop.
  board.loop();
  the_mesh.loop();
  sensors.loop();
  victron_scanner.loop();
  rtc_clock.tick();
}
