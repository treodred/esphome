import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
)
from . import TrillComponent, trill_ns, CONF_DEVICE_TYPE

DEPENDENCIES = ['trill']

CONF_TRILL_ID = 'trill_id'
CONF_TOUCH_INDEX = 'touch_index'
CONF_TYPE = 'type'

# Types de capteurs
TYPE_LOCATION = 'location'
TYPE_SIZE = 'size'
TYPE_HORIZONTAL = 'horizontal'
TYPE_BUTTON = 'button'

CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=3,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({
    cv.GenerateID(CONF_TRILL_ID): cv.use_id(TrillComponent),
    cv.Required(CONF_TYPE): cv.enum({
        TYPE_LOCATION: TYPE_LOCATION,
        TYPE_SIZE: TYPE_SIZE,
        TYPE_HORIZONTAL: TYPE_HORIZONTAL,
        TYPE_BUTTON: TYPE_BUTTON,
    }, lower=True),
    cv.Required(CONF_TOUCH_INDEX): cv.int_range(min=0, max=4),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TRILL_ID])
    var = await sensor.new_sensor(config)
    
    touch_type = config[CONF_TYPE]
    index = config[CONF_TOUCH_INDEX]
    
    if touch_type == TYPE_LOCATION:
        cg.add(parent.set_touch_sensor(index, var))
    elif touch_type == TYPE_SIZE:
        cg.add(parent.set_touch_size_sensor(index, var))
    elif touch_type == TYPE_HORIZONTAL:
        cg.add(parent.set_horizontal_sensor(index, var))
    elif touch_type == TYPE_BUTTON:
        cg.add(parent.set_button_sensor(index, var))
