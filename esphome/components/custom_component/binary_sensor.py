import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import TrillComponent, trill_ns

DEPENDENCIES = ['trill']

CONF_TRILL_ID = 'trill_id'
CONF_TOUCH_INDEX = 'touch_index'

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({
    cv.GenerateID(CONF_TRILL_ID): cv.use_id(TrillComponent),
    cv.Required(CONF_TOUCH_INDEX): cv.int_range(min=0, max=4),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TRILL_ID])
    var = await binary_sensor.new_binary_sensor(config)
    
    index = config[CONF_TOUCH_INDEX]
    cg.add(parent.set_touch_binary_sensor(index, var))