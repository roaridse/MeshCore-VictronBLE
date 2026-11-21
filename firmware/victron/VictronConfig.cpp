#include "VictronConfig.h"

#include <Utils.h>
#include <helpers/TxtDataHelpers.h>

namespace victron {
namespace {

constexpr uint32_t kMagic = 0x56494331;  // VIC1
constexpr uint8_t kVersion = 1;
constexpr char kFilename[] = "/victron.cfg";

struct __attribute__((packed)) DiskDevice {
  uint8_t enabled;
  uint8_t mac[6];
  uint8_t key[kKeySize];
  char name[kDeviceNameSize];
};

struct __attribute__((packed)) DiskConfig {
  uint32_t magic;
  uint8_t version;
  DiskDevice devices[kMaxDevices];
  uint8_t status_enabled;
  uint8_t alarms_enabled;
  uint16_t interval_minutes;
  char channel_name[kChannelNameSize];
  uint8_t channel_key[16];
};

bool allZero(const uint8_t* data, size_t length) {
  uint8_t value = 0;
  for (size_t i = 0; i < length; ++i) value |= data[i];
  return value == 0;
}

}  // namespace

void ConfigStore::reset() {
  memset(devices_, 0, sizeof(devices_));
  channel_ = ChannelConfig{};
}

bool ConfigStore::begin(FILESYSTEM* filesystem) {
  filesystem_ = filesystem;
  reset();
  if (!filesystem_ || !filesystem_->exists(kFilename)) return true;
  File file = filesystem_->open(kFilename);
  if (!file) return false;
  DiskConfig disk = {};
  const size_t bytes = file.read(reinterpret_cast<uint8_t*>(&disk), sizeof(disk));
  file.close();
  if (bytes != sizeof(disk) || disk.magic != kMagic || disk.version != kVersion) {
    return false;
  }
  for (uint8_t i = 0; i < kMaxDevices; ++i) {
    devices_[i].enabled = disk.devices[i].enabled == 1;
    memcpy(devices_[i].mac, disk.devices[i].mac, sizeof(devices_[i].mac));
    memcpy(devices_[i].key, disk.devices[i].key, sizeof(devices_[i].key));
    memcpy(devices_[i].name, disk.devices[i].name, sizeof(devices_[i].name));
    devices_[i].name[sizeof(devices_[i].name) - 1] = 0;
  }
  channel_.status_enabled = disk.status_enabled == 1;
  channel_.alarms_enabled = disk.alarms_enabled == 1;
  channel_.interval_minutes = disk.interval_minutes;
  if (channel_.interval_minutes < 15) channel_.interval_minutes = 120;
  memcpy(channel_.name, disk.channel_name, sizeof(channel_.name));
  channel_.name[sizeof(channel_.name) - 1] = 0;
  memcpy(channel_.key, disk.channel_key, sizeof(channel_.key));
  return true;
}

bool ConfigStore::save() {
  if (!filesystem_) return false;
  DiskConfig disk = {};
  disk.magic = kMagic;
  disk.version = kVersion;
  for (uint8_t i = 0; i < kMaxDevices; ++i) {
    disk.devices[i].enabled = devices_[i].enabled ? 1 : 0;
    memcpy(disk.devices[i].mac, devices_[i].mac, sizeof(devices_[i].mac));
    memcpy(disk.devices[i].key, devices_[i].key, sizeof(devices_[i].key));
    memcpy(disk.devices[i].name, devices_[i].name, sizeof(devices_[i].name));
  }
  disk.status_enabled = channel_.status_enabled ? 1 : 0;
  disk.alarms_enabled = channel_.alarms_enabled ? 1 : 0;
  disk.interval_minutes = channel_.interval_minutes;
  memcpy(disk.channel_name, channel_.name, sizeof(disk.channel_name));
  memcpy(disk.channel_key, channel_.key, sizeof(disk.channel_key));

  filesystem_->remove(kFilename);
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  File file = filesystem_->open(kFilename, FILE_O_WRITE);
#else
  File file = filesystem_->open(kFilename, "w");
#endif
  if (!file) return false;
  const size_t bytes = file.write(reinterpret_cast<const uint8_t*>(&disk), sizeof(disk));
  file.close();
  return bytes == sizeof(disk);
}

int ConfigStore::find(const uint8_t mac[6]) const {
  for (uint8_t i = 0; i < kMaxDevices; ++i) {
    if (devices_[i].enabled && memcmp(devices_[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

int ConfigStore::addOrUpdate(const uint8_t mac[6], const uint8_t key[kKeySize],
                             const char* name) {
  int index = find(mac);
  if (index < 0) {
    for (uint8_t i = 0; i < kMaxDevices; ++i) {
      if (!devices_[i].enabled) {
        index = i;
        break;
      }
    }
  }
  if (index < 0) return -1;
  DeviceConfig& item = devices_[index];
  item.enabled = true;
  memcpy(item.mac, mac, sizeof(item.mac));
  memcpy(item.key, key, sizeof(item.key));
  StrHelper::strncpy(item.name, (name && name[0]) ? name : "Victron", sizeof(item.name));
  return index;
}

bool ConfigStore::remove(const uint8_t mac[6]) {
  const int index = find(mac);
  if (index < 0) return false;
  memset(&devices_[index], 0, sizeof(devices_[index]));
  return true;
}

uint8_t ConfigStore::count() const {
  uint8_t result = 0;
  for (const auto& item : devices_) if (item.enabled) ++result;
  return result;
}

bool ConfigStore::channelConfigured() const {
  return channel_.name[0] != 0 && !allZero(channel_.key, sizeof(channel_.key));
}

void ConfigStore::makeGroupChannel(mesh::GroupChannel& destination) const {
  memset(&destination, 0, sizeof(destination));
  memcpy(destination.secret, channel_.key, sizeof(channel_.key));
  mesh::Utils::sha256(destination.hash, sizeof(destination.hash),
                      channel_.key, sizeof(channel_.key));
}

bool ConfigStore::parseMac(const char* text, uint8_t destination[6]) {
  if (!text || !destination) return false;
  unsigned int parts[6];
  char tail;
  const int count = sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%c",
                           &parts[0], &parts[1], &parts[2], &parts[3],
                           &parts[4], &parts[5], &tail);
  if (count != 6) return false;
  for (uint8_t i = 0; i < 6; ++i) destination[i] = static_cast<uint8_t>(parts[i]);
  return true;
}

bool ConfigStore::parseHexKey(const char* text, uint8_t destination[kKeySize]) {
  return text && strlen(text) == kKeySize * 2 &&
         mesh::Utils::fromHex(destination, kKeySize, text);
}

void ConfigStore::formatMac(const uint8_t mac[6], char destination[18]) {
  snprintf(destination, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

}  // namespace victron
