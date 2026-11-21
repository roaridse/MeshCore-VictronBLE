#include "ShellyConfig.h"

#include <helpers/TxtDataHelpers.h>

namespace shelly {
namespace {

constexpr uint32_t kMagic = 0x53485431;  // SHT1
constexpr uint8_t kVersion = 1;
constexpr char kFilename[] = "/shelly.cfg";

struct __attribute__((packed)) DiskDevice {
  uint8_t enabled;
  uint8_t mac[6];
  char name[kDeviceNameSize];
};

struct __attribute__((packed)) DiskConfig {
  uint32_t magic;
  uint8_t version;
  DiskDevice devices[kMaxDevices];
};

}  // namespace

void ConfigStore::reset() { memset(devices_, 0, sizeof(devices_)); }

bool ConfigStore::begin(FILESYSTEM* filesystem) {
  filesystem_ = filesystem;
  reset();
  if (!filesystem_ || !filesystem_->exists(kFilename)) return true;
  File file = filesystem_->open(kFilename);
  if (!file) return false;
  DiskConfig disk = {};
  const size_t bytes = file.read(reinterpret_cast<uint8_t*>(&disk), sizeof(disk));
  file.close();
  if (bytes != sizeof(disk) || disk.magic != kMagic || disk.version != kVersion) return false;
  for (uint8_t i = 0; i < kMaxDevices; ++i) {
    devices_[i].enabled = disk.devices[i].enabled == 1;
    memcpy(devices_[i].mac, disk.devices[i].mac, sizeof(devices_[i].mac));
    memcpy(devices_[i].name, disk.devices[i].name, sizeof(devices_[i].name));
    devices_[i].name[sizeof(devices_[i].name) - 1] = 0;
  }
  return true;
}

bool ConfigStore::save() {
  if (!filesystem_) return false;
  DiskConfig disk = {};
  disk.magic = kMagic;
  disk.version = kVersion;
  for (uint8_t i = 0; i < kMaxDevices; ++i) {
    disk.devices[i].enabled = devices_[i].enabled ? 1 : 0;
    memcpy(disk.devices[i].mac, devices_[i].mac, sizeof(disk.devices[i].mac));
    memcpy(disk.devices[i].name, devices_[i].name, sizeof(disk.devices[i].name));
  }
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

int ConfigStore::addOrUpdate(const uint8_t mac[6], const char* name) {
  int index = find(mac);
  if (index < 0) {
    for (uint8_t i = 0; i < kMaxDevices; ++i) {
      if (!devices_[i].enabled) { index = i; break; }
    }
  }
  if (index < 0) return -1;
  DeviceConfig& item = devices_[index];
  item.enabled = true;
  memcpy(item.mac, mac, sizeof(item.mac));
  StrHelper::strncpy(item.name, (name && name[0]) ? name : "Shelly", sizeof(item.name));
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

}  // namespace shelly
