#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace trill {

enum TrillDevice {
  TRILL_NONE = -1,
  TRILL_UNKNOWN = 0,
  TRILL_BAR = 1,
  TRILL_SQUARE = 2,
  TRILL_CRAFT = 3,
  TRILL_RING = 4,
  TRILL_HEX = 5,
  TRILL_FLEX = 6,
};

enum TrillMode {
  MODE_CENTROID = 0,
  MODE_RAW = 1,
  MODE_BASELINE = 2,
  MODE_DIFF = 3,
};

class TrillComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration
  void set_device_type(TrillDevice device) { device_type_ = device; }
  void set_mode(TrillMode mode) { mode_ = mode; }
  void set_scan_speed(uint8_t speed) { scan_speed_ = speed; }
  void set_num_bits(uint8_t bits) { num_bits_ = bits; }
  void set_threshold(uint8_t threshold) { threshold_ = threshold; }
  void set_prescaler(uint8_t prescaler) { prescaler_ = prescaler; }
  void set_minimum_touch_size(uint16_t size) { min_touch_size_ = size; }
  
  // Sensors pour les touches (jusqu'à 5 touches simultanées)
  void set_touch_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 5) touch_sensors_[index] = sensor;
  }
  void set_touch_size_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 5) touch_size_sensors_[index] = sensor;
  }
  
  // Capteurs binaires pour détecter la présence de touches
  void set_touch_binary_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < 5) touch_binary_sensors_[index] = sensor;
  }
  
  // Pour les capteurs 2D (Square, Hex)
  void set_horizontal_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 4) horizontal_sensors_[index] = sensor;
  }
  
  // Pour Ring - boutons
  void set_button_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 2) button_sensors_[index] = sensor;
  }

 protected:
  // Commandes I2C
  static const uint8_t CMD_OFFSET = 0;
  static const uint8_t DATA_OFFSET = 4;
  
  static const uint8_t CMD_MODE = 1;
  static const uint8_t CMD_SCAN_SETTINGS = 2;
  static const uint8_t CMD_PRESCALER = 3;
  static const uint8_t CMD_NOISE_THRESHOLD = 4;
  static const uint8_t CMD_IDAC = 5;
  static const uint8_t CMD_BASELINE_UPDATE = 6;
  static const uint8_t CMD_MINIMUM_SIZE = 7;
  static const uint8_t CMD_AUTO_SCAN = 16;
  static const uint8_t CMD_IDENTIFY = 255;
  
  // Méthodes internes
  bool identify_();
  bool read_centroids_();
  void update_baseline_();
  void send_command_(uint8_t command, const uint8_t *data = nullptr, uint8_t len = 0);
  void prepare_for_data_read_();
  uint8_t get_centroid_length_();
  bool is_2d_() const;
  
  // Configuration
  TrillDevice device_type_{TRILL_NONE};
  TrillMode mode_{MODE_CENTROID};
  uint8_t scan_speed_{1};
  uint8_t num_bits_{12};
  uint8_t threshold_{0};
  uint8_t prescaler_{1};
  uint16_t min_touch_size_{0};
  
  // État
  uint8_t firmware_version_{0};
  uint8_t last_read_loc_{0xFF};
  uint16_t buffer_[32];  // Buffer pour les données de centroid
  uint8_t num_touches_{0};
  
  // Sensors
  sensor::Sensor *touch_sensors_[5] = {nullptr};
  sensor::Sensor *touch_size_sensors_[5] = {nullptr};
  binary_sensor::BinarySensor *touch_binary_sensors_[5] = {nullptr};
  sensor::Sensor *horizontal_sensors_[4] = {nullptr};
  sensor::Sensor *button_sensors_[2] = {nullptr};
};

}  // namespace trill
}  // namespace esphome
