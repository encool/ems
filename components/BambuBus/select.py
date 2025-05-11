import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_NAME, CONF_INTERNAL, CONF_DISABLED_BY_DEFAULT, CONF_ICON, CONF_ENTITY_CATEGORY, ICON_EMPTY, ENTITY_CATEGORY_CONFIG


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


def _get_id_declarator_for_select_type(select_type_str):
    if select_type_str == TYPE_FILAMENT_STATE:
        return cv.declare_id(_FilamentStateSelect)
    elif select_type_str == TYPE_MOTOR_INDEX:
        return cv.declare_id(_FilamentMotorSelect)
    raise cv.Invalid(f"Unknown controller_select_type for ID declaration: {select_type_str}")

def final_schema_processor(config):
    id_declarator_validator = _get_id_declarator_for_select_type(config[CONF_CONTROLLER_SELECT_TYPE])
    # config[CONF_ID] 此时是 PvariableID (由 select.SELECT_SCHEMA 中的 cv.declare_id(Select) 创建)
    # 我们需要修改这个 PvariableID 的 .type 属性
    
    # id_obj = config[CONF_ID] # 这个是 PvariableID 对象
    # if config[CONF_CONTROLLER_SELECT_TYPE] == TYPE_FILAMENT_STATE:
    #     id_obj.type = _FilamentStateSelect
    # elif config[CONF_CONTROLLER_SELECT_TYPE] == TYPE_MOTOR_INDEX:
    #     id_obj.type = _FilamentMotorSelect
    # else: # Should not happen
    #     pass

    # 更好的方法：让 id_declarator_validator 重新声明，但这可能会导致 ID 重复声明的警告
    # 或者，如果 id_declarator_validator 返回一个新的 PvariableID，则替换它
    id_value_str = config[CONF_ID].id # 获取原始 ID 字符串
    new_id_obj = id_declarator_validator(id_value_str) # 用原始 ID 字符串重新声明
    config[CONF_ID] = new_id_obj # 替换为带有正确类型的 PvariableID

    return config

# 使用 select.SELECT_SCHEMA 作为基础，它已经包含了 CONF_ID, CONF_DISABLED_BY_DEFAULT 等
# select.SELECT_SCHEMA 内部会用 cv.declare_id(Select) 来声明 CONF_ID
CONFIG_SCHEMA = cv.All(
    select.SELECT_SCHEMA.extend({
        # 添加我们平台特定的字段
        cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({
            TYPE_FILAMENT_STATE: None,
            TYPE_MOTOR_INDEX: None,
        }, lower=True),
        cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusController),
        # 如果 select.SELECT_SCHEMA 中有些字段你不想要，需要想办法移除或覆盖
        # 通常，我们只是添加字段
    }).extend(cv.COMPONENT_SCHEMA), # COMPONENT_SCHEMA 通常用于 setup_priority 等

    # final_schema_processor 现在需要处理一个已经被 select.SELECT_SCHEMA 声明为
    # select.Select 类型的 ID。它的任务是“覆盖”或“修正”这个类型。
    final_schema_processor
)

# to_code 函数保持不变或类似之前的结构
async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_CONTROLLER_ID])
    select_type_str = config[CONF_CONTROLLER_SELECT_TYPE]
    
    var_id_obj = config[CONF_ID] # 这个 PvariableID 的 .type 应该是被 final_schema_processor 修正过的

    if select_type_str == TYPE_FILAMENT_STATE:
        var = cg.new_Pvariable(var_id_obj, parent)
    elif select_type_str == TYPE_MOTOR_INDEX:
        var = cg.new_Pvariable(var_id_obj, parent)
    else:
        raise cv.Invalid(f"Unknown controller_select_type in to_code: {select_type_str}")

    # options=[] 是因为我们的 options 是在 C++ 的 setup() 中动态设置的
    await select.register_select(var, config, options=[])