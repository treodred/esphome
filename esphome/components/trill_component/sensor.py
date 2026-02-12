import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@yourusername"]

trill_ns = cg.esphome_ns.namespace("trill")
TrillComponent = trill_ns.class_("TrillComponent", cg.PollingComponent, i2c.I2CDevice)

# Trill device types enum
//TrillDevice = trill_ns.enum("device_type_", is_class=True)
//TrillDevice = trill_ns.enum("TrillDevice", is_class=True)
TRILL_DEVICES = {
    "BAR": TrillDevice.TRILL_BAR,
    "SQUARE": TrillDevice.TRILL_SQUARE,
    "CRAFT": TrillDevice.TRILL_CRAFT,
    "RING": TrillDevice.TRILL_RING,
    "HEX": TrillDevice.TRILL_HEX,
    "FLEX": TrillDevice.TRILL_FLEX,
}

# Configuration keys
CONF_DEVICE_TYPE = "device_type"
CONF_NUM_TOUCHES = "num_touches"
CONF_TOUCHES = "touches"
CONF_BUTTONS = "buttons"
CONF_SCAN_SPEED = "scan_speed"
CONF_SCAN_RESOLUTION = "scan_resolution"
CONF_PRESCALER = "prescaler"
CONF_NOISE_THRESHOLD = "noise_threshold"
CONF_MINIMUM_TOUCH_SIZE = "minimum_touch_size"

# Touch configuration
CONF_POSITION = "position"
CONF_SIZE = "size"
CONF_HORIZONTAL_POSITION = "horizontal_position"
CONF_HORIZONTAL_SIZE = "horizontal_size"

# Default I2C addresses for each device type
DEFAULT_ADDRESSES = {
    "BAR": 0x20,
    "SQUARE": 0x28,
    "CRAFT": 0x30,
    "RING": 0x38,
    "HEX": 0x40,
    "FLEX": 0x48,
}

# Scan speed values
SCAN_SPEEDS = {
    "ULTRA_FAST": 0,
    "FAST": 1,
    "NORMAL": 2,
    "SLOW": 3,
}

def validate_device_config(config):
    """Validate that the configuration matches the device type."""
    device_type = config[CONF_DEVICE_TYPE]
    
    # 2D devices (SQUARE, HEX) support horizontal sensors
    is_2d = device_type in ["SQUARE", "HEX"]
    
    # RING device supports buttons
    is_ring = device_type == "RING"
    
    # Check if horizontal sensors are configured for non-2D devices
    if not is_2d and CONF_TOUCHES in config:
        for touch_config in config[CONF_TOUCHES]:
            if CONF_HORIZONTAL_POSITION in touch_config or CONF_HORIZONTAL_SIZE in touch_config:
                raise cv.Invalid(
                    f"horizontal_position and horizontal_size are only available for SQUARE and HEX devices, not {device_type}"
                )
    
    # Check if buttons are configured for non-RING devices
    if not is_ring and CONF_BUTTONS in config:
        raise cv.Invalid(
            f"buttons are only available for RING devices, not {device_type}"
        )
    
    return config

# Touch sensor schema
TOUCH_SCHEMA = cv.Schema({
    cv.Optional(CONF_POSITION): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
       
    ),
    cv.Optional(CONF_SIZE): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
       
    ),
    cv.Optional(CONF_HORIZONTAL_POSITION): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
       
    ),
    cv.Optional(CONF_HORIZONTAL_SIZE): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
       
    ),
})

# Button sensor schema
BUTTON_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
   
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(TrillComponent),
        cv.Required(CONF_DEVICE_TYPE): cv.enum(TRILL_DEVICES, upper=True),
        cv.Optional(CONF_NUM_TOUCHES): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
           
        ),
        cv.Optional(CONF_TOUCHES): cv.ensure_list(TOUCH_SCHEMA),
        cv.Optional(CONF_BUTTONS): cv.ensure_list(BUTTON_SCHEMA),
        cv.Optional(CONF_SCAN_SPEED, default="ULTRA_FAST"): cv.enum(SCAN_SPEEDS, upper=True),
        cv.Optional(CONF_SCAN_RESOLUTION, default=12): cv.int_range(min=9, max=16),
        cv.Optional(CONF_PRESCALER, default=1): cv.int_range(min=1, max=8),
        cv.Optional(CONF_NOISE_THRESHOLD, default=0): cv.int_range(min=0, max=255),
        cv.Optional(CONF_MINIMUM_TOUCH_SIZE, default=0): cv.uint16_t,
    })
    .extend(cv.polling_component_schema("50ms"))
    .extend(i2c.i2c_device_schema(None)),  # Address will be set based on device type
    validate_device_config,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Set default I2C address based on device type if not specified
    device_type_str = config[CONF_DEVICE_TYPE]
    if i2c.CONF_ADDRESS not in config:
        config[i2c.CONF_ADDRESS] = DEFAULT_ADDRESSES[device_type_str]
    
    await i2c.register_i2c_device(var, config)
    
    # Set device type
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
    
    # Set scan settings
    cg.add(var.set_scan_speed(config[CONF_SCAN_SPEED]))
    cg.add(var.set_scan_resolution(config[CONF_SCAN_RESOLUTION]))
    cg.add(var.set_prescaler(config[CONF_PRESCALER]))
    
    if config[CONF_NOISE_THRESHOLD] != 0:
        cg.add(var.set_noise_threshold(config[CONF_NOISE_THRESHOLD]))
    
    if config[CONF_MINIMUM_TOUCH_SIZE] != 0:
        cg.add(var.set_minimum_touch_size(config[CONF_MINIMUM_TOUCH_SIZE]))
    
    # Configure num_touches sensor
    if CONF_NUM_TOUCHES in config:
        sens = await sensor.new_sensor(config[CONF_NUM_TOUCHES])
        cg.add(var.set_num_touches_sensor(sens))
    
    # Configure individual touch sensors
    if CONF_TOUCHES in config:
        for i, touch_config in enumerate(config[CONF_TOUCHES]):
            if i >= 5:  # Max 5 touches for 1D devices
                break
            
            if CONF_POSITION in touch_config:
                sens = await sensor.new_sensor(touch_config[CONF_POSITION])
                cg.add(var.set_touch_position_sensor(i, sens))
            
            if CONF_SIZE in touch_config:
                sens = await sensor.new_sensor(touch_config[CONF_SIZE])
                cg.add(var.set_touch_size_sensor(i, sens))
            
            # 2D sensors (only for SQUARE and HEX)
            if CONF_HORIZONTAL_POSITION in touch_config:
                if i >= 4:  # Max 4 touches for 2D devices
                    break
                sens = await sensor.new_sensor(touch_config[CONF_HORIZONTAL_POSITION])
                cg.add(var.set_touch_horizontal_position_sensor(i, sens))
            
            if CONF_HORIZONTAL_SIZE in touch_config:
                if i >= 4:
                    break
                sens = await sensor.new_sensor(touch_config[CONF_HORIZONTAL_SIZE])
                cg.add(var.set_touch_horizontal_size_sensor(i, sens))
    
    # Configure button sensors (RING only)
    if CONF_BUTTONS in config:
        for i, button_config in enumerate(config[CONF_BUTTONS]):
            if i >= 2:  # Max 2 buttons
                break
            sens = await sensor.new_sensor(button_config)

            cg.add(var.set_button_sensor(i, sens))

