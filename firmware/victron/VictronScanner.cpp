#include "VictronScanner.h"

#ifdef NRF52_PLATFORM
#include <bluefruit.h>
#endif

namespace victron {

#ifdef NRF52_PLATFORM
Scanner* Scanner::instance_ = nullptr;

bool Scanner::begin(ConfigStore* config, ruuvi::Scanner* ruuvi_scanner) {
  config_ = config;
  ruuvi_scanner_ = ruuvi_scanner;
  memset(states_, 0, sizeof(states_));
  instance_ = this;

  // Observer/central capacity is needed for scanning. No GATT connection is made.
  Bluefruit.configCentralBandwidth(BANDWIDTH_LOW);
  if (!Bluefruit.begin(0, 1)) return false;
  Bluefruit.autoConnLed(false);
  Bluefruit.setName("Victron Mesh Sensor");
  Bluefruit.Scanner.setRxCallback(scanCallback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.setInterval(160, 80);  // 100 ms interval, 50 ms window
  return Bluefruit.Scanner.start(0);
}

void Scanner::scanCallback(ble_gap_evt_adv_report_t* report) {
  if (instance_) instance_->handleReport(report);
  Bluefruit.Scanner.resume();
}

void Scanner::handleReport(ble_gap_evt_adv_report_t* report) {
  if (!config_ || !report || !report->data.p_data) return;

  uint8_t mac[6];
  // Nordic stores BLE addresses least-significant byte first.
  for (uint8_t i = 0; i < 6; ++i) mac[i] = report->peer_addr.addr[5 - i];
  const int device_index = config_->find(mac);

  const uint8_t* bytes = report->data.p_data;
  size_t remaining = report->data.len;
  while (remaining >= 2) {
    const uint8_t field_length = bytes[0];
    if (field_length == 0 || static_cast<size_t>(field_length + 1) > remaining) break;
    const uint8_t type = bytes[1];
    if (type == BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA && field_length >= 4) {
      const uint8_t* manufacturer = bytes + 2;
      const size_t manufacturer_length = field_length - 1;
      const uint16_t company = static_cast<uint16_t>(manufacturer[0]) |
                               (static_cast<uint16_t>(manufacturer[1]) << 8);
      if (company == ruuvi::kCompanyId && ruuvi_scanner_) {
        ruuvi_scanner_->handleManufacturerData(manufacturer + 2,
                                                manufacturer_length - 2,
                                                report->rssi);
      } else if (company == kCompanyId && device_index >= 0) {
        Reading reading;
        const DeviceConfig& device = config_->device(device_index);
        if (decodeAdvertisement(manufacturer + 2, manufacturer_length - 2,
                                device.key, reading) == DecodeResult::Ok) {
          DeviceState& state = states_[device_index];
          if (!state.has_reading || state.reading.data_counter != reading.data_counter) {
            state.reading = reading;
            state.has_reading = true;
            state.rssi = report->rssi;
            state.received_at = millis();
            ++state.packets;
          }
        }
        return;
      }
    }
    bytes += field_length + 1;
    remaining -= field_length + 1;
  }
}

#else

bool Scanner::begin(ConfigStore* config, ruuvi::Scanner* ruuvi_scanner) {
  config_ = config;
  ruuvi_scanner_ = ruuvi_scanner;
  memset(states_, 0, sizeof(states_));
  return false;
}

#endif

}  // namespace victron
