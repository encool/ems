import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_NAME, ICON_EMPTY, ENTITY_CATEGORY_CONFIG


# Import the main controller class to link to it
BambuBusController = cg.esphome_ns.class_('BambuBus')

# Define the C++ classes for your specific select entities
_FilamentStateSelect = cg.esphome_ns.class_('FilamentStateSelect', select.Select, cg.Component)
_FilamentMotorSelect = cg.esphome_ns.class_('FilamentMotorSelect', select.Select, cg.Component)

# Configuration keys
CONF_CONTROLLER_SELECT_TYPE = 'controller_select_type'
CONF_PARENT_CONTROLLER_ID = 'parent_controller_id'

# Enum values for select type
TYPE_FILAMENT_STATE = 'filament_state'
TYPE_MOTOR_INDEX = 'motor_index'


# Custom validator to declare the ID with the correct C++ type
# This function will receive the full config for this select entry.
def _declare_id_based_on_type(config):
    if config[CONF_CONTROLLER_SELECT_TYPE] == TYPE_FILAMENT_STATE:
        # Declare the ID with the C++ type _FilamentStateSelect
        # This ensures cg.get_variable() and type checks in __init__.py work correctly
        cv.declare_id(_FilamentStateSelect)(config[CONF_ID])
    elif config[CONF_CONTROLLER_SELECT_TYPE] == TYPE_MOTOR_INDEX:
        # Declare the ID with the C++ type _FilamentMotorSelect
        cv.declare_id(_FilamentMotorSelect)(config[CONF_ID])
    else:
        # This case should ideally not be reached if enum validation is correct
        raise cv.Invalid(f"Unknown controller_select_type for ID declaration: {config[CONF_CONTROLLER_SELECT_TYPE]}")
    return config # Return the (potentially modified) config


CONFIG_SCHEMA = cv.All( # Use cv.All to apply validators sequentially
    select.SELECT_SCHEMA.extend({
        # CONF_ID is already part of select.SELECT_SCHEMA, we'll handle its type declaration later
        cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({
            TYPE_FILAMENT_STATE: None,
            TYPE_MOTOR_INDEX: None,
        }, lower=True),
        cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusController),
        # You can add other common select options here if needed, e.g.:
        # cv.Optional(CONF_ICON, default=ICON_EMPTY): cv.icon,
        # cv.Optional(CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG): cv.entity_category,
    }).extend(cv.COMPONENT_SCHEMA),
    _declare_id_based_on_type # Apply our custom ID declarator AFTER basic schema validation
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_CONTROLLER_ID])
    select_type_str = config[CONF_CONTROLLER_SELECT_TYPE]

    # config[CONF_ID] is the Pvariable ID which now has the correct C++ type
    # thanks to _declare_id_based_on_type.
    # cg.Pvariable(id, ConstructorCall) is the way to generate `id = new ConstructorCall();`
    # The first argument to cg.Pvariable is the ID itself (already typed).
    # The second argument is the C++ `new ...` expression.

    if select_type_str == TYPE_FILAMENT_STATE:
        # Generates: `auto* id_from_yaml = new BambuBus::FilamentStateSelect(parent_ptr);`
        var = cg.Pvariable(config[CONF_ID], _FilamentStateSelect.new(parent))
    elif select_type_str == TYPE_MOTOR_INDEX:
        # Generates: `auto* id_from_yaml = new BambuBus::FilamentMotorSelect(parent_ptr);`
        var = cg.Pvariable(config[CONF_ID], _FilamentMotorSelect.new(parent))
    else:
        # Should not be reached if schema validation is correct
        raise cv.Invalid(f"Unknown controller_select_type in to_code: {select_type_str}")

    # Options are set in C++ setup() method using this->traits.set_options(...)
    await select.register_select(var, config, options=[])
    # Component registration is handled by register_select for select entities