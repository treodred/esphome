/*
 * Trill Component for ESPHome
 * This component wraps the Trill library for use in ESPHome
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "trill.h"

namespace esphome {
    namespace trill {

        class TrillComponent : public PollingComponent, public i2c::I2CDevice {
        public:
            TrillComponent() = default;

            // ESPHome lifecycle methods
            void setup() override;
            void update() override;
            void dump_config() override;
            float get_setup_priority() const override { return setup_priority::DATA; }

            // Configuration methods (called from Python code generation)
            void set_device_type(Trill::Device type) { device_type_ = type; }

            // Set sensor for number of touches
            void set_num_touches_sensor(sensor::Sensor* sensor) { num_touches_sensor_ = sensor; }

            // Set sensors for individual touches (position and size)
            void set_touch_position_sensor(uint8_t index, sensor::Sensor* sensor) {
                if (index < 5) touch_position_sensors_[index] = sensor;
            }

            void set_touch_size_sensor(uint8_t index, sensor::Sensor* sensor) {
                if (index < 5) touch_size_sensors_[index] = sensor;
            }

            // For 2D devices (SQUARE, HEX)
            void set_touch_horizontal_position_sensor(uint8_t index, sensor::Sensor* sensor) {
                if (index < 4) touch_horizontal_position_sensors_[index] = sensor;
            }

            void set_touch_horizontal_size_sensor(uint8_t index, sensor::Sensor* sensor) {
                if (index < 4) touch_horizontal_size_sensors_[index] = sensor;
            }

            // For RING device buttons
            void set_button_sensor(uint8_t index, sensor::Sensor* sensor) {
                if (index < 2) button_sensors_[index] = sensor;
            }

            // Advanced configuration methods
            void set_scan_speed(uint8_t speed) { scan_speed_ = speed; }
            void set_scan_resolution(uint8_t bits) { scan_resolution_ = bits; }
            void set_prescaler(uint8_t prescaler) { prescaler_ = prescaler; }
            void set_noise_threshold(uint8_t threshold) { noise_threshold_ = threshold; }
            void set_minimum_touch_size(uint16_t size) { minimum_touch_size_ = size; }

        protected:
            // Trill library instance
            Trill trill_device_;

            // Device configuration
            Trill::Device device_type_{ Trill::TRILL_BAR };

            // Scan settings
            uint8_t scan_speed_{ 0 };           // 0 = ultra fast, 1 = fast, 2 = normal, 3 = slow
            uint8_t scan_resolution_{ 12 };     // 9-16 bits
            uint8_t prescaler_{ 1 };            // 1-8
            uint8_t noise_threshold_{ 0 };      // 0-255
            uint16_t minimum_touch_size_{ 0 };  // Minimum centroid size

            // Sensors for publishing data
            sensor::Sensor* num_touches_sensor_{ nullptr };

            // Touch position and size sensors (1D: up to 5, 2D: up to 4)
            sensor::Sensor* touch_position_sensors_[5]{ nullptr };
            sensor::Sensor* touch_size_sensors_[5]{ nullptr };

            // Horizontal touch sensors (2D only)
            sensor::Sensor* touch_horizontal_position_sensors_[4]{ nullptr };
            sensor::Sensor* touch_horizontal_size_sensors_[4]{ nullptr };

            // Button sensors (RING only)
            sensor::Sensor* button_sensors_[2]{ nullptr };

            // Helper methods
            void publish_touch_data_();
            void publish_button_data_();
        };

    }  // namespace trill

}  // namespace esphome
