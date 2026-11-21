#pragma once

#include <Arduino.h>

#include "ShellyConfig.h"
#include "ShellyDecoder.h"

namespace shelly {

constexpr uint8_t kMaxDiscovered = 8;

struct DeviceState {
  bool has_reading = false;
  Reading reading;
  int8_t rssi = 0;
  uint32_t received_at = 0;
  uint32_t packets = 0;
};

struct DiscoveredDevice {
  bool occupied = false;
  uint8_t mac[6] = {};
  Reading reading;
  int8_t rssi = 0;
  uint32_t received_at = 0;
};

class Scanner {
 public:
  void begin(ConfigStore* config);
  void handleServiceData(const uint8_t mac[6], const uint8_t* data,
                         size_t length, int8_t rssi);
  const DeviceState& state(uint8_t index) const { return states_[index]; }
  const DiscoveredDevice& discovered(uint8_t index) const { return discovered_[index]; }
  uint8_t discoveredCount() const;

 private:
  ConfigStore* config_ = nullptr;
  DeviceState states_[kMaxDevices];
  DiscoveredDevice discovered_[kMaxDiscovered];
};

}  // namespace shelly
