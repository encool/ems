#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h" // <<<--- 确保包含 GPIOPin 定义
#include "esphome/components/select/select.h"

// #include "main.h"
#include "crc.h"

#define Bambubus_version 5

// 前向声明移到 namespace esphome 内
namespace esphome {
    class FilamentStateSelect;
    class FilamentMotorSelect;
} // namespace esphome

#ifdef __cplusplus
extern "C"
{
#endif

    static constexpr const char *TAG = "BambuBus"; // 必须这样定义

    enum _filament_status
    {
        offline,
        online,
        NFC_waiting
    };
    enum class FilamentMotionMotorIndex : int // Specifying 'int' as underlying type is optional here as it's the default
    {
        MOTOR_1 = 1, // Assigns the value 1 to MOTOR_1
        MOTOR_2 = 2, // Assigns the value 2 to MOTOR_2
        MOTOR_3 = 3, // Assigns the value 3 to MOTOR_3
        MOTOR_4 = 4  // Assigns the value 4 to MOTOR_4
        // You can also choose other names like INDEX_1, OPTION_1 etc.
    };
    enum _filament_motion_state_set
    {
        need_pull_back,
        need_send_out,
        on_use,
        idle
    };
    enum package_type
    {
        BambuBus_package_ERROR = -1,
        BambuBus_package_NONE = 0,
        BambuBus_package_filament_motion_short,
        BambuBus_package_filament_motion_long,
        BambuBus_package_online_detect,
        BambuBus_package_REQx6,
        BambuBus_package_NFC_detect,
        BambuBus_package_set_filament,
        BambuBus_long_package_MC_online,
        BambuBus_longe_package_filament,
        BambuBus_long_package_version,
        BambuBus_package_heartbeat,
        BambuBus_package_ETC,
        __BambuBus_package_packge_type_size
    };
    extern void BambuBus_init();
    // extern package_type BambuBus_run();
#define max_filament_num 4
    extern bool Bambubus_read();
    extern void Bambubus_set_need_to_save();
    extern int get_now_filament_num();
    extern void reset_filament_meters(int num);
    extern void add_filament_meters(int num, float meters);
    extern float get_filament_meters(int num);
    extern void set_filament_online(int num, bool if_online);
    extern bool get_filament_online(int num);
    _filament_motion_state_set get_filament_motion(int num);

#ifdef __cplusplus
}
#endif

// 将枚举值转换为字符串的辅助函数 (用于发布状态和比较)
// 注意：这些字符串必须与 select.py 中定义的 options 完全一致
inline const char *filament_state_to_string(_filament_motion_state_set state)
{
    switch (state)
    {
    case need_pull_back:
        return "need_pull_back";
    case need_send_out:
        return "need_send_out";
    case on_use:
        return "on_use";
    case idle:
        return "idle";
    default:
        return "unknown"; // 或者抛出错误
    }
}

// 将字符串转换为枚举值的辅助函数 (用于 control 方法)
inline esphome::optional<_filament_motion_state_set> string_to_filament_state(const std::string &str_state)
{
    if (str_state == "need_pull_back")
        return _filament_motion_state_set::need_pull_back;
    if (str_state == "need_send_out")
        return _filament_motion_state_set::need_send_out;
    if (str_state == "on_use")
        return _filament_motion_state_set::on_use;
    if (str_state == "idle")
        return _filament_motion_state_set::idle;
    return {}; // esphome::optional is empty
}

namespace esphome {

class BambuBus : public esphome::Component, public esphome::uart::UARTDevice
{
protected:
    esphome::GPIOPin *de_pin_{nullptr}; // <<<--- 添加 DE 引脚成员变量
    bool initialized_{false};           // 用于跟踪 pref_ 是否已初始化

    _filament_motion_state_set current_motion_state_ = idle;
    FilamentMotionMotorIndex current_motor_index_ = FilamentMotionMotorIndex::MOTOR_1;

    // Helper to update HA when internal state changes
    void publish_motion_state_to_ha();
    void publish_motor_index_to_ha();

public:
    esphome::ESPPreferenceObject pref_;

    BambuBus() : UARTDevice() {}

    void setup() override;
    void loop() override;

    // 添加 DE 引脚的设置方法
    void set_de_pin(esphome::GPIOPin *de_pin) { this->de_pin_ = de_pin; }
    void send_uart_with_de(const uint8_t *data, uint16_t length); // 用于带 DE 控制发送的新方法
    package_type BambuBus_run();
    bool is_initialized_() const { return this->initialized_; }
    void mark_initialized_() { this->initialized_ = true; }

    // Pointers to the select entities to update them
    FilamentStateSelect *state_select_entity_{nullptr};
    FilamentMotorSelect *motor_select_entity_{nullptr};

    void set_motor_state(unsigned char AMS_num, unsigned char read_num, _filament_motion_state_set motor_state);

    // Getters (could be used by select entities for initial state, or by other parts)
    _filament_motion_state_set get_current_motion_state() const { return current_motion_state_; }
    FilamentMotionMotorIndex get_current_motor_index() const { return current_motor_index_; }

    // Methods to link select entities (called from generated code via __init__.py)
    void set_filament_state_select(FilamentStateSelect *select_entity) { this->state_select_entity_ = select_entity; }
    void set_filament_motor_select(FilamentMotorSelect *select_entity) { this->motor_select_entity_ = select_entity; }

private:
    bool need_debug = true;
        // Methods to set the states (called by select entities)
    void set_current_motion_state(_filament_motion_state_set state);
    void set_current_motor_index(FilamentMotionMotorIndex index);
};

}
// ... 其他类/函数声明 ...

// 声明全局 CRC 对象 (不要在这里初始化)
extern CRC16 crc_16;
extern CRC8 crc_8;
extern CRC8 _RX_IRQ_crcx;
