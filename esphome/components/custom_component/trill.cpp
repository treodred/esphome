#include "trill.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace trill {

static const char *const TAG = "trill";

void TrillComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Trill sensor...");
  
  // Identifier le capteur
  if (!this->identify_()) {
    ESP_LOGE(TAG, "Failed to identify Trill sensor");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "Trill sensor identified: type=%d, firmware=%d", 
                device_type_, firmware_version_);
  
  // Configurer le mode
  uint8_t mode_data = (uint8_t)mode_;
  this->send_command_(CMD_MODE, &mode_data, 1);
  delay(20);
  
  // Configurer les paramètres de scan
  uint8_t scan_data[2] = {scan_speed_, num_bits_};
  this->send_command_(CMD_SCAN_SETTINGS, scan_data, 2);
  delay(20);
  
  // Configurer le prescaler si défini
  if (prescaler_ > 0) {
    this->send_command_(CMD_PRESCALER, &prescaler_, 1);
    delay(20);
  }
  
  // Configurer le seuil de bruit si défini
  if (threshold_ > 0) {
    this->send_command_(CMD_NOISE_THRESHOLD, &threshold_, 1);
    delay(20);
  }
  
  // Configurer la taille minimum de toucher
  if (min_touch_size_ > 0) {
    uint8_t size_data[2] = {(uint8_t)(min_touch_size_ >> 8), 
                            (uint8_t)(min_touch_size_ & 0xFF)};
    this->send_command_(CMD_MINIMUM_SIZE, size_data, 2);
    delay(20);
  }
  
  // Mettre à jour la baseline
  this->update_baseline_();
  delay(50);
  
  ESP_LOGCONFIG(TAG, "Trill sensor setup complete");
}

void TrillComponent::update() {
  if (mode_ != MODE_CENTROID) {
    return;
  }
  
  if (!this->read_centroids_()) {
    ESP_LOGW(TAG, "Failed to read centroid data");
    return;
  }
  
  // Publier les données des touches
  for (uint8_t i = 0; i < num_touches_ && i < 5; i++) {
    uint16_t location = buffer_[i];
    uint16_t size = buffer_[i + 5];
    
    // Position normalisée (0-1)
    float normalized_location = location / 65535.0f;
    
    if (touch_sensors_[i] != nullptr) {
      touch_sensors_[i]->publish_state(normalized_location);
    }
    
    if (touch_size_sensors_[i] != nullptr) {
      touch_size_sensors_[i]->publish_state(size);
    }
    
    if (touch_binary_sensors_[i] != nullptr) {
      touch_binary_sensors_[i]->publish_state(true);
    }
  }
  
  // Marquer les touches non actives
  for (uint8_t i = num_touches_; i < 5; i++) {
    if (touch_binary_sensors_[i] != nullptr) {
      touch_binary_sensors_[i]->publish_state(false);
    }
  }
  
  // Pour les capteurs 2D
  if (is_2d_()) {
    uint8_t num_horizontal = 0;
    // Compter les touches horizontales
    for (uint8_t i = 10; i < 14; i++) {
      if (buffer_[i] != 0xFFFF) {
        num_horizontal++;
      } else {
        break;
      }
    }
    
    for (uint8_t i = 0; i < num_horizontal && i < 4; i++) {
      if (horizontal_sensors_[i] != nullptr) {
        float h_location = buffer_[10 + i] / 65535.0f;
        horizontal_sensors_[i]->publish_state(h_location);
      }
    }
  }
  
  // Pour Ring - boutons
  if (device_type_ == TRILL_RING) {
    for (uint8_t i = 0; i < 2; i++) {
      if (button_sensors_[i] != nullptr) {
        button_sensors_[i]->publish_state(buffer_[10 + i]);
      }
    }
  }
}

void TrillComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Trill Sensor:");
  LOG_I2C_DEVICE(this);
  
  const char *device_name = "Unknown";
  switch (device_type_) {
    case TRILL_BAR: device_name = "Bar"; break;
    case TRILL_SQUARE: device_name = "Square"; break;
    case TRILL_RING: device_name = "Ring"; break;
    case TRILL_HEX: device_name = "Hex"; break;
    case TRILL_FLEX: device_name = "Flex"; break;
    case TRILL_CRAFT: device_name = "Craft"; break;
    default: break;
  }
  
  ESP_LOGCONFIG(TAG, "  Device Type: %s", device_name);
  ESP_LOGCONFIG(TAG, "  Firmware Version: %d", firmware_version_);
  ESP_LOGCONFIG(TAG, "  Mode: %d", mode_);
  ESP_LOGCONFIG(TAG, "  Scan Speed: %d", scan_speed_);
  ESP_LOGCONFIG(TAG, "  Resolution: %d bits", num_bits_);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with Trill failed!");
  }
}

bool TrillComponent::identify_() {
  uint8_t cmd_data[2] = {CMD_OFFSET, CMD_IDENTIFY};
  
  if (!this->write_bytes_raw(cmd_data, 2)) {
    return false;
  }
  
  delay(25);
  
  uint8_t response[3];
  if (!this->read_bytes_raw(response, 3)) {
    return false;
  }
  
  device_type_ = (TrillDevice)response[1];
  firmware_version_ = response[2];
  last_read_loc_ = CMD_OFFSET;
  
  return device_type_ != TRILL_NONE;
}

bool TrillComponent::read_centroids_() {
  this->prepare_for_data_read_();
  
  uint8_t length = this->get_centroid_length_();
  
  if (!this->read_bytes_raw((uint8_t*)buffer_, length)) {
    return false;
  }
  
  // Convertir big-endian en little-endian
  for (uint8_t i = 0; i < length / 2; i++) {
    uint8_t *bytes = (uint8_t*)&buffer_[i];
    uint8_t temp = bytes[0];
    bytes[0] = bytes[1];
    bytes[1] = temp;
  }
  
  // Compter le nombre de touches
  num_touches_ = 0;
  uint8_t max_touches = is_2d_() ? 4 : 5;
  for (uint8_t i = 0; i < max_touches; i++) {
    if (buffer_[i] == 0xFFFF) {
      break;
    }
    num_touches_++;
  }
  
  return true;
}

void TrillComponent::update_baseline_() {
  uint8_t cmd_data[2] = {CMD_OFFSET, CMD_BASELINE_UPDATE};
  this->write_bytes_raw(cmd_data, 2);
  last_read_loc_ = CMD_OFFSET;
}

void TrillComponent::send_command_(uint8_t command, const uint8_t *data, uint8_t len) {
  uint8_t buffer[10];
  buffer[0] = CMD_OFFSET;
  buffer[1] = command;
  
  for (uint8_t i = 0; i < len; i++) {
    buffer[2 + i] = data[i];
  }
  
  this->write_bytes_raw(buffer, 2 + len);
  last_read_loc_ = CMD_OFFSET;
}

void TrillComponent::prepare_for_data_read_() {
  if (last_read_loc_ != DATA_OFFSET) {
    this->write_bytes_raw(&DATA_OFFSET, 1);
    last_read_loc_ = DATA_OFFSET;
  }
}

uint8_t TrillComponent::get_centroid_length_() {
  if (device_type_ == TRILL_SQUARE || device_type_ == TRILL_HEX) {
    return 32;  // 2D: 16 words
  } else if (device_type_ == TRILL_RING) {
    return 24;  // Ring avec boutons: 12 words
  } else {
    return 20;  // 1D standard: 10 words
  }
}

bool TrillComponent::is_2d_() const {
  return device_type_ == TRILL_SQUARE || device_type_ == TRILL_HEX;
}

}  // namespace trill
}  // namespace esphome