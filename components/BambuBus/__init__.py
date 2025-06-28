import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, output 
from esphome.const import CONF_ID
from esphome.components import select # 确保 select 被导入，以便引用 select.Select

from esphome.pins import gpio_output_pin_schema

DEPENDENCIES = ['uart', 'output', 'select']

# empty_uart_component_ns = cg.esphome_ns.namespace('bambu_bus')
# EmptyUARTComponent = empty_uart_component_ns.class_('BambuBus', cg.Component, uart.UARTDevice)

EmptyUARTComponent = cg.esphome_ns.class_('BambuBus', cg.Component, uart.UARTDevice)

# Forward declare select C++ classes (will be fully defined in select.py)
FilamentStateSelect = cg.esphome_ns.class_('FilamentStateSelect', select.Select, cg.Component)
FilamentMotorSelect = cg.esphome_ns.class_('FilamentMotorSelect', select.Select, cg.Component)

# Configuration keys for the select entities this component will manage
CONF_FILAMENT_STATE_SELECT = 'filament_state_select'
CONF_FILAMENT_MOTOR_SELECT = 'filament_motor_select'

CONF_DE_PIN = 'de_pin'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EmptyUARTComponent),
    cv.Optional(CONF_DE_PIN): gpio_output_pin_schema,
    cv.Optional(CONF_FILAMENT_STATE_SELECT): cv.use_id(FilamentStateSelect),
    cv.Optional(CONF_FILAMENT_MOTOR_SELECT): cv.use_id(FilamentMotorSelect),
    # cv.Optional(CONF_FILAMENT_STATE_SELECT): cv.use_id(cv.Any),
    # cv.Optional(CONF_FILAMENT_MOTOR_SELECT): cv.use_id(cv.Any),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

def to_code(config):
    cg.add_header("BambuBus.h")
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    # cg.add_global(cg.RawExpression('using esphome::BambuBus;'))
    # cg.add_library('BambuBus', None)

        # <<<--- 如果配置了 DE 引脚，生成设置代码 ---
    if CONF_DE_PIN in config:
        # vvv--- 获取 GPIOPin* 对象 ---vvv
        # 如果使用 PinSchema:
        conf = config[CONF_DE_PIN]
        pin_conf = config[CONF_DE_PIN] # 获取验证后的配置字典
        pin_expression = yield cg.gpio_pin_expression(pin_conf) # 使用字典生成表达式
        cg.add(var.set_de_pin(pin_expression))
    if CONF_FILAMENT_STATE_SELECT in config:
        select_entity = yield cg.get_variable(config[CONF_FILAMENT_STATE_SELECT])
        cg.add(var.set_filament_state_select(select_entity))
    
    if CONF_FILAMENT_MOTOR_SELECT in config:
        select_entity = yield cg.get_variable(config[CONF_FILAMENT_MOTOR_SELECT])
        cg.add(var.set_filament_motor_select(select_entity))        