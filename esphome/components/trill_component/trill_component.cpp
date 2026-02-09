/*
 * Trill Component for ESPHome
 * Implementation
 */

#include "trill_component.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace trill {

        static const char* TAG = "trill";

        void TrillComponent::setup() {
            ESP_LOGCONFIG(TAG, "Setting up Trill sensor...");

            // Initialize the Trill device with this I2CDevice
            int result = trill_device_.begin(device_type_, this);

            if (result != 0) {
                ESP_LOGE(TAG, "Failed to initialize Trill (error code: %d)", result);
                switch (result) {
                case -3:
                    ESP_LOGE(TAG, "Wrong device type detected");
                    break;
                case -2:
                    ESP_LOGE(TAG, "Invalid I2C device");
                    break;
                case -1:
                    ESP_LOGE(TAG, "Invalid mode configuration");
                    break;
                case 2:
                    ESP_LOGE(TAG, "Unable to identify device - check I2C connection");
                    break;
                default:
                    ESP_LOGE(TAG, "Unknown error");
                    break;
                }
                this->mark_failed();
                return;
            }

            ESP_LOGCONFIG(TAG, "Trill %s initialized successfully",
                Trill::getNameFromDevice(device_type_));
            ESP_LOGCONFIG(TAG, "Firmware version: %d", trill_device_.firmwareVersion());
            ESP_LOGCONFIG(TAG, "Number of channels: %d", trill_device_.getNumChannels());

            // Apply advanced settings if configured
            if (scan_speed_ != 0 || scan_resolution_ != 12) {
                trill_device_.setScanSettings(scan_speed_, scan_resolution_);
                delay(Trill::interCommandDelay);
            }

            if (prescaler_ != 1) {
                trill_device_.setPrescaler(prescaler_);
                delay(Trill::interCommandDelay);
            }

            if (noise_threshold_ != 0) {
                trill_device_.setNoiseThreshold(noise_threshold_);
                delay(Trill::interCommandDelay);
            }

            if (minimum_touch_size_ != 0) {
                trill_device_.setMinimumTouchSize(minimum_touch_size_);
                delay(Trill::interCommandDelay);
            }

            ESP_LOGCONFIG(TAG, "Trill setup complete");
        }

        void TrillComponent::update() {
            // Read data from the sensor
            if (!trill_device_.read()) {
                ESP_LOGW(TAG, "Failed to read from Trill sensor");
                this->status_set_warning();
                return;
            }

            this->status_clear_warning();

            // Publish touch data
            publish_touch_data_();

            // Publish button data if RING device
            if (device_type_ == Trill::TRILL_RING) {
                publish_button_data_();
            }
        }

        void TrillComponent::publish_touch_data_() {
            uint8_t num_touches = trill_device_.getNumTouches();

            // Publish number of touches
            if (num_touches_sensor_ != nullptr) {
                num_touches_sensor_->publish_state(num_touches);
            }

            // Determine max touches based on device type
            uint8_t max_touches = (device_type_ == Trill::TRILL_SQUARE ||
                device_type_ == Trill::TRILL_HEX) ? 4 : 5;

            // Publish individual touch data
            for (uint8_t i = 0; i < max_touches; i++) {
                if (i < num_touches) {
                    // Touch is active
                    int position = trill_device_.touchLocation(i);
                    int size = trill_device_.touchSize(i);

                    if (touch_position_sensors_[i] != nullptr) {
                        touch_position_sensors_[i]->publish_state(position);
                    }

                    if (touch_size_sensors_[i] != nullptr) {
                        touch_size_sensors_[i]->publish_state(size);
                    }
                }
                else {
                    // No touch - publish NaN to indicate no data
                    if (touch_position_sensors_[i] != nullptr) {
                        touch_position_sensors_[i]->publish_state(NAN);
                    }

                    if (touch_size_sensors_[i] != nullptr) {
                        touch_size_sensors_[i]->publish_state(NAN);
                    }
                }
            }

            // For 2D devices, publish horizontal touch data
            if (trill_device_.is2D()) {
                uint8_t num_horizontal_touches = trill_device_.getNumHorizontalTouches();

                for (uint8_t i = 0; i < 4; i++) {
                    if (i < num_horizontal_touches) {
                        int h_position = trill_device_.touchHorizontalLocation(i);
                        int h_size = trill_device_.touchHorizontalSize(i);

                        if (touch_horizontal_position_sensors_[i] != nullptr) {
                            touch_horizontal_position_sensors_[i]->publish_state(h_position);
                        }

                        if (touch_horizontal_size_sensors_[i] != nullptr) {
                            touch_horizontal_size_sensors_[i]->publish_state(h_size);
                        }
                    }
                    else {
                        if (touch_horizontal_position_sensors_[i] != nullptr) {
                            touch_horizontal_position_sensors_[i]->publish_state(NAN);
                        }

                        if (touch_horizontal_size_sensors_[i] != nullptr) {
                            touch_horizontal_size_sensors_[i]->publish_state(NAN);
                        }
                    }
                }
            }
        }

        void TrillComponent::publish_button_data_() {
            // Only RING device has buttons
            if (device_type_ != Trill::TRILL_RING) {
                return;
            }

            for (uint8_t i = 0; i < 2; i++) {
                if (button_sensors_[i] != nullptr) {
                    int button_value = trill_device_.getButtonValue(i);
                    if (button_value >= 0) {
                        button_sensors_[i]->publish_state(button_value);
                    }
                }
            }
        }

        void TrillComponent::dump_config() {
            ESP_LOGCONFIG(TAG, "Trill Sensor:");
            LOG_I2C_DEVICE(this);
            LOG_UPDATE_INTERVAL(this);

            ESP_LOGCONFIG(TAG, "  Device Type: %s",
                Trill::getNameFromDevice(device_type_));
            ESP_LOGCONFIG(TAG, "  Firmware Version: %d", trill_device_.firmwareVersion());
            ESP_LOGCONFIG(TAG, "  Number of Channels: %d", trill_device_.getNumChannels());
            ESP_LOGCONFIG(TAG, "  Mode: %s",
                trill_device_.getMode() == Trill::CENTROID ? "CENTROID" :
                trill_device_.getMode() == Trill::RAW ? "RAW" :
                trill_device_.getMode() == Trill::BASELINE ? "BASELINE" :
                trill_device_.getMode() == Trill::DIFF ? "DIFF" : "UNKNOWN");

            ESP_LOGCONFIG(TAG, "  Scan Speed: %d", scan_speed_);
            ESP_LOGCONFIG(TAG, "  Scan Resolution: %d bits", scan_resolution_);
            ESP_LOGCONFIG(TAG, "  Prescaler: %d", prescaler_);

            if (noise_threshold_ != 0) {
                ESP_LOGCONFIG(TAG, "  Noise Threshold: %d", noise_threshold_);
            }

            if (minimum_touch_size_ != 0) {
                ESP_LOGCONFIG(TAG, "  Minimum Touch Size: %d", minimum_touch_size_);
            }

            // Log configured sensors
            if (num_touches_sensor_ != nullptr) {
                LOG_SENSOR("  ", "Number of Touches", num_touches_sensor_);
            }

            for (uint8_t i = 0; i < 5; i++) {
                if (touch_position_sensors_[i] != nullptr) {
                    LOG_SENSOR("  ", "Touch Position", touch_position_sensors_[i]);
                }
                if (touch_size_sensors_[i] != nullptr) {
                    LOG_SENSOR("  ", "Touch Size", touch_size_sensors_[i]);
                }
            }

            if (trill_device_.is2D()) {
                for (uint8_t i = 0; i < 4; i++) {
                    if (touch_horizontal_position_sensors_[i] != nullptr) {
                        LOG_SENSOR("  ", "Horizontal Position", touch_horizontal_position_sensors_[i]);
                    }
                    if (touch_horizontal_size_sensors_[i] != nullptr) {
                        LOG_SENSOR("  ", "Horizontal Size", touch_horizontal_size_sensors_[i]);
                    }
                }
            }

            if (device_type_ == Trill::TRILL_RING) {
                for (uint8_t i = 0; i < 2; i++) {
                    if (button_sensors_[i] != nullptr) {
                        LOG_SENSOR("  ", "Button", button_sensors_[i]);
                    }
                }
            }

            if (this->is_failed()) {
                ESP_LOGE(TAG, "Communication with Trill failed!");
            }
        }

    }  // namespace trill
}  // namespace esphome