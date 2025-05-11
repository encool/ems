import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_NAME # CONF_ICON, CONF_ENTITY_CATEGORY etc.

BambuBusComponent = cg.esphome_ns.class_('BambuBus', cg.Component, uart.UARTDevice)

# Forward declare select C++ classes (will be fully defined in select.py)
FilamentStateSelect = cg.esphome_ns.class_('FilamentStateSelect')
FilamentMotorSelect = cg.esphome_ns.class_('FilamentMotorSelect')

# Key to tell which type of select this is
CONF_CONTROLLER_SELECT_TYPE = 'controller_select_type'
TYPE_FILAMENT_STATE = 'filament_state'
TYPE_MOTOR_INDEX = 'motor_index'

# ID of the parent MyFilamentController component
CONF_PARENT_CONTROLLER_ID = 'parent_controller_id'


CONFIG_SCHEMA = select.SELECT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(select.Select), # Base class for ID
    cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({
        TYPE_FILAMENT_STATE: None,
        TYPE_MOTOR_INDEX: None,
    }, lower=True),
    cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusComponent),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_CONTROLLER_ID])
    select_type = config[CONF_CONTROLLER_SELECT_TYPE]

    if select_type == TYPE_FILAMENT_STATE:
        # Pass parent to constructor
        var = cg.new_Pvariable(config[CONF_ID], FilamentStateSelect(parent))
    elif select_type == TYPE_MOTOR_INDEX:
        var = cg.new_Pvariable(config[CONF_ID], FilamentMotorSelect(parent))
    else:
        # Should not happen due to schema validation
        raise cv.Invalid(f"Unknown controller_select_type: {select_type}")

    await select.register_select(var, config, options=[]) # Options are set in C++
    # Component registration is handled by register_select for select entities