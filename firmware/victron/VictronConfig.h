#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/IdentityStore.h>

#include "VictronDecoder.h"

namespace victron {

constexpr uint8_t kMaxDevices = 4;
constexpr size_t kDeviceNameSize = 16;
constexpr size_t kChannelNameSize = 32;

struct DeviceConfig {
  bool enabled = false;
  uint8_t mac[6] = {};
  uint8_t key[kKeySize] = {};
  char name[kDeviceNameSize] = {};
};

struct ChannelConfig {
  bool status_enabled = false;
  bool alarms_enabled = true;
  uint16_t interval_minutes = 120;
  char name[kChannelNameSize] = {};
  uint8_t key[16] = {};
};

class ConfigStore {
 public:
  bool begin(FILESYSTEM* filesystem);
  bool save();
  void reset();

  int addOrUpdate(const uint8_t mac[6], const uint8_t key[kKeySize],
                  const char* name);
  bool remove(const uint8_t mac[6]);
  int find(const uint8_t mac[6]) const;
  uint8_t count() const;

  DeviceConfig& device(uint8_t index) { return devices_[index]; }
  const DeviceConfig& device(uint8_t index) const { return devices_[index]; }
  ChannelConfig& channel() { return channel_; }
  const ChannelConfig& channel() const { return channel_; }

  bool channelConfigured() const;
  void makeGroupChannel(mesh::GroupChannel& destination) const;

  static bool parseMac(const char* text, uint8_t destination[6]);
  static bool parseHexKey(const char* text, uint8_t destination[kKeySize]);
  static void formatMac(const uint8_t mac[6], char destination[18]);

 private:
  FILESYSTEM* filesystem_ = nullptr;
  DeviceConfig devices_[kMaxDevices];
  ChannelConfig channel_;
};

}  // namespace victron
