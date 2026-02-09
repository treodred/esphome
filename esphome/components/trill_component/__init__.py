import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ['i2c']
CODEOWNERS = ['@yourusername']

# Namespace
trill_ns = cg.esphome_ns.namespace('trill_component')
TrillComponent = trill_ns.class_('TrillComponent', cg.PollingComponent, i2c.I2CDevice)

# Enums
TrillDevice = trill_ns.enum('TrillDevice')
TRILL_DEVICES = {
    'BAR': TrillDevice.TRILL_BAR,
    'SQUARE': TrillDevice.TRILL_SQUARE,
    'CRAFT': TrillDevice.TRILL_CRAFT,
    'RING': TrillDevice.TRILL_RING,
    'HEX': TrillDevice.TRILL_HEX,
    'FLEX': TrillDevice.TRILL_FLEX,
}

TrillMode = trill_ns.enum('TrillMode')
TRILL_MODES = {
    'CENTROID': TrillMode.MODE_CENTROID,
    'RAW': TrillMode.MODE_RAW,
    'BASELINE': TrillMode.MODE_BASELINE,
    'DIFF': TrillMode.MODE_DIFF,
}

# Configuration keys
CONF_DEVICE_TYPE = 'device_type'
CONF_MODE = 'mode'
CONF_SCAN_SPEED = 'scan_speed'
CONF_NUM_BITS = 'num_bits'
CONF_THRESHOLD = 'threshold'
CONF_PRESCALER = 'prescaler'
CONF_MINIMUM_TOUCH_SIZE = 'minimum_touch_size'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TrillComponent),
    cv.Required(CONF_DEVICE_TYPE): cv.enum(TRILL_DEVICES, upper=True),
    cv.Optional(CONF_MODE, default='CENTROID'): cv.enum(TRILL_MODES, upper=True),
    cv.Optional(CONF_SCAN_SPEED, default=1): cv.int_range(min=0, max=3),
    cv.Optional(CONF_NUM_BITS, default=12): cv.int_range(min=9, max=16),
    cv.Optional(CONF_THRESHOLD, default=0): cv.uint8_t,
    cv.Optional(CONF_PRESCALER, default=1): cv.int_range(min=1, max=8),
    cv.Optional(CONF_MINIMUM_TOUCH_SIZE, default=0): cv.uint16_t,
}).extend(cv.polling_component_schema('60ms')).extend(i2c.i2c_device_schema(0x18))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    
    cg.add(var.set_device_type(config[CONF_DEVICE_TYPE]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_scan_speed(config[CONF_SCAN_SPEED]))
    cg.add(var.set_num_bits(config[CONF_NUM_BITS]))
    
    if CONF_THRESHOLD in config:
        cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    if CONF_PRESCALER in config:
        cg.add(var.set_prescaler(config[CONF_PRESCALER]))
    if CONF_MINIMUM_TOUCH_SIZE in config:
        cg.add(var.set_minimum_touch_size(config[CONF_MINIMUM_TOUCH_SIZE]))
