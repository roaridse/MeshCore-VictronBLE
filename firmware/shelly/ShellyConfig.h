#pragma once

#include <Arduino.h>
#include <helpers/IdentityStore.h>

namespace shelly {

constexpr uint8_t kMaxDevices = 4;
constexpr size_t kDeviceNameSize = 16;

struct DeviceConfig {
  bool enabled = false;
  uint8_t mac[6] = {};
  char name[kDeviceNameSize] = {};
};

class ConfigStore {
 public:
  bool begin(FILESYSTEM* filesystem);
  bool save();
  void reset();
  int addOrUpdate(const uint8_t mac[6], const char* name);
  bool remove(const uint8_t mac[6]);
  int find(const uint8_t mac[6]) const;
  uint8_t count() const;
  DeviceConfig& device(uint8_t index) { return devices_[index]; }
  const DeviceConfig& device(uint8_t index) const { return devices_[index]; }

 private:
  FILESYSTEM* filesystem_ = nullptr;
  DeviceConfig devices_[kMaxDevices];
};

}  // namespace shelly
