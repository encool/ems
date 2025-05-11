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
    # config 是一个字典，包含了当前 select 实体的配置
    # config[CONF_ID] 此时是用户在 YAML 中提供的 ID 字符串或一个简单的 ID 对象
    
    id_declarator_validator = _get_id_declarator_for_select_type(config[CONF_CONTROLLER_SELECT_TYPE])
    
    # 应用这个验证器 (它也负责声明) 到 ID 的值上
    # cv.declare_id(TYPE) 返回的验证器会接受 ID 字符串，
    # 声明它，并返回一个 esphome.config_validation.ID 对象 (实际上是 PvariableID)。
    # 这个 PvariableID 对象的 .type 属性将被设置为 TYPE。
    id_with_correct_type = id_declarator_validator(config[CONF_ID])
    
    # 将带有正确类型的 ID 对象放回配置中
    config[CONF_ID] = id_with_correct_type
    return config

CONFIG_SCHEMA = cv.All(
    # 1. 基础结构验证，但不最终确定 CONF_ID 的类型
    cv.Schema({
        # 从 select.SELECT_SCHEMA 或 cv.ENTITY_BASE_SCHEMA 复制必要的字段
        # 例如: CONF_NAME, CONF_INTERNAL, etc.
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_INTERNAL): cv.boolean,
        cv.Optional(CONF_DISABLED_BY_DEFAULT): cv.boolean,
        cv.Optional(CONF_ICON): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
        
        # 我们自己的字段
        cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({
            TYPE_FILAMENT_STATE: None,
            TYPE_MOTOR_INDEX: None,
        }, lower=True),
        cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusController),
        
        # CONF_ID：暂时只验证它存在且是个有效的 ID 字符串
        cv.Required(CONF_ID): cv.string, # 或者 cv.string, cv.declare_id 会处理实际的 ID 对象创建
    }).extend(cv.COMPONENT_SCHEMA), # COMPONENT_SCHEMA 通常是空的或处理 setup_priority 等

    # 2. 应用 final_schema_processor 来根据类型正确声明/修改 CONF_ID
    final_schema_processor
)

# CONFIG_SCHEMA = cv.All(
#     cv.Schema({
#         cv.Required(CONF_ID): cv.string, # 或者 cv.valid_id_name
#         cv.Required(CONF_NAME): cv.string_strict,
#         # 其他如 ICON, ENTITY_CATEGORY 等可以从 cv.ENTITY_BASE_SCHEMA 中酌情选取
#         cv.Optional(CONF_INTERNAL): cv.boolean, 
#         # ...
#         cv.Required(CONF_CONTROLLER_SELECT_TYPE): cv.enum({
#             TYPE_FILAMENT_STATE: None,
#             TYPE_MOTOR_INDEX: None,
#         }, lower=True),
#         cv.Required(CONF_PARENT_CONTROLLER_ID): cv.use_id(BambuBusController),
#     }).extend(cv.COMPONENT_SCHEMA), # 通常是安全的
#     final_schema_processor
# )

# to_code 函数保持不变，它期望 config[CONF_ID] 是一个具有正确类型的 PvariableID
async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_CONTROLLER_ID])
    select_type_str = config[CONF_CONTROLLER_SELECT_TYPE]
    
    # config[CONF_ID] 应该是由 final_schema_processor 处理过的 PvariableID
    # 它已经有关联的 C++ 类型 (_FilamentStateSelect 或 _FilamentMotorSelect)
    var_id_obj = config[CONF_ID]

    if select_type_str == TYPE_FILAMENT_STATE:
        # cg.new_Pvariable(id_obj, *constructor_args)
        # 第一个参数是 PvariableID 对象，第二个开始是构造函数参数
        var = cg.new_Pvariable(var_id_obj, parent)
    elif select_type_str == TYPE_MOTOR_INDEX:
        var = cg.new_Pvariable(var_id_obj, parent)
    else:
        raise cv.Invalid(f"Unknown controller_select_type in to_code: {select_type_str}")

    await select.register_select(var, config, options=[])