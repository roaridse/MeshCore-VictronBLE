#pragma once

#include <Arduino.h>

#include "VictronConfig.h"
#include "VictronDecoder.h"
#include "RuuviScanner.h"
#include "ShellyScanner.h"

#ifdef NRF52_PLATFORM
#include <bluefruit.h>
#endif

namespace victron {

struct DeviceState {
  bool has_reading = false;
  Reading reading;
  int8_t rssi = 0;
  uint32_t received_at = 0;
  uint32_t packets = 0;
};

class Scanner {
 public:
  bool begin(ConfigStore* config, ruuvi::Scanner* ruuvi_scanner,
             shelly::Scanner* shelly_scanner);
  void loop() {}

  const DeviceState& state(uint8_t index) const { return states_[index]; }
  DeviceState& state(uint8_t index) { return states_[index]; }

 private:
  ConfigStore* config_ = nullptr;
  ruuvi::Scanner* ruuvi_scanner_ = nullptr;
  shelly::Scanner* shelly_scanner_ = nullptr;
  DeviceState states_[kMaxDevices];

#ifdef NRF52_PLATFORM
  static Scanner* instance_;
  static void scanCallback(ble_gap_evt_adv_report_t* report);
  void handleReport(ble_gap_evt_adv_report_t* report);
#endif
};

}  // namespace victron
