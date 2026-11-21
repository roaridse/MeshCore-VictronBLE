#include "RuuviScanner.h"

namespace ruuvi {

void Scanner::begin(ConfigStore* config) {
  config_ = config;
  memset(states_, 0, sizeof(states_));
  memset(discovered_, 0, sizeof(discovered_));
}

void Scanner::handleManufacturerData(const uint8_t* data, size_t length, int8_t rssi) {
  Reading reading;
  if (!decodeRawV2(data, length, reading)) return;
  const uint32_t now = millis();

  int discovery_index = -1;
  int oldest_index = 0;
  for (uint8_t i = 0; i < kMaxDiscovered; ++i) {
    if (discovered_[i].occupied &&
        memcmp(discovered_[i].reading.mac, reading.mac, sizeof(reading.mac)) == 0) {
      discovery_index = i;
      break;
    }
    if (!discovered_[i].occupied && discovery_index < 0) discovery_index = i;
    if (discovered_[i].received_at < discovered_[oldest_index].received_at) oldest_index = i;
  }
  if (discovery_index < 0) discovery_index = oldest_index;
  DiscoveredDevice& found = discovered_[discovery_index];
  found.occupied = true;
  found.reading = reading;
  found.rssi = rssi;
  found.received_at = now;

  if (!config_) return;
  const int configured_index = config_->find(reading.mac);
  if (configured_index < 0) return;
  DeviceState& state = states_[configured_index];
  state.has_reading = true;
  state.reading = reading;
  state.rssi = rssi;
  state.received_at = now;
  ++state.packets;
}

uint8_t Scanner::discoveredCount() const {
  uint8_t count = 0;
  for (const auto& item : discovered_) if (item.occupied) ++count;
  return count;
}

}  // namespace ruuvi
