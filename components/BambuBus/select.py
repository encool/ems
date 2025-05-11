# my_components/BambuBus/select.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_NAME

BambuBusController = cg.esphome_ns.class_('BambuBus')

FilamentStateSelect = cg.esphome_ns.class_('FilamentStateSelect', select.Select, cg.Component)
FilamentMotorSelect = cg.esphome_ns.class_('FilamentMotorSelect', select.Select, cg.Component)

CONF_CONTROLLER_SELECT_TYPE = 'controller_select_type'
TYPE_FILAMENT_STATE = 'filament_state'
TYPE_MOTOR_INDEX = 'motor_index'
CONF_PARENT_CONTROLLER_ID = 'parent_controller_id'

# 这个函数在配置验证阶段被调用，用来决定ID的真实类型
def validate_select_type(value):
    if value[CONF_CONTROLLER_SELECT_TYPE] == TYPE_FILAMENT_STATE:
        return cv.declare_id(FilamentStateSelect)(value[CONF_ID]) # 声明ID为FilamentStateSelect
    if value[CONF_CONTROLLER_SELECT_TYPE] == TYPE_MOTOR_INDEX:
        return cv.declare_id(FilamentMotorSelect)(value[CONF_ID]) # 声明ID为FilamentMotorSelect
    raise cv.Invalid("Unknown controller_select_type")


CONFIG_SCHEMA = select.SELECT_SCHEMA.extend({
    # cv.GenerateID(CONF_ID) 会被下面的 validate_select_type 覆盖
    cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({TYPE_FILAMENT_STATE: None, TYPE_MOTOR_INDEX: None}, lower=True),
    cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusController),
}).extend(cv.COMPONENT_SCHEMA).extend(cv.Schema({
    # 这个特殊的 schema 片段在其他所有验证之后运行
    cv.Required(CONF_ID): validate_select_type,
}, extra=cv.ALLOW_EXTRA)) # extra=cv.ALLOW_EXTRA 是因为 CONF_ID 已经在 select.SELECT_SCHEMA 中


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_CONTROLLER_ID])
    select_type_str = config[CONF_CONTROLLER_SELECT_TYPE]

    # 由于 validate_select_type, config[CONF_ID] 已经是正确类型的 Pvariable ID
    # 所以 cg.new_Pvariable 会直接用这个ID实例化对应的类型
    if select_type_str == TYPE_FILAMENT_STATE:
        # var = cg.new_Pvariable(config[CONF_ID], parent)
        # 应该这样写，因为 config[CONF_ID] 已经通过 validate_select_type 知道了它是 FilamentStateSelect
        # 并且其构造函数需要一个 parent
        var = cg.Pvariable(config[CONF_ID], FilamentStateSelect.new(parent)) # 生成 id = new FilamentStateSelect(parent_ptr);
    elif select_type_str == TYPE_MOTOR_INDEX:
        var = cg.Pvariable(config[CONF_ID], FilamentMotorSelect.new(parent)) # 生成 id = new FilamentMotorSelect(parent_ptr);
    else:
        raise cv.Invalid(f"Unknown controller_select_type: {select_type_str}")

    await select.register_select(var, config, options=[])