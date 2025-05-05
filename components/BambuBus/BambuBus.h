#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h" // <<<--- 确保包含 GPIOPin 定义
// #include "main.h"

#define Bambubus_version 5

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
    extern package_type BambuBus_run();
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

class BambuBus : public esphome::Component, public esphome::uart::UARTDevice
{
protected:
    esphome::ESPPreferenceObject pref_;
    esphome::GPIOPin *de_pin_{nullptr}; // <<<--- 添加 DE 引脚成员变量
public:
    BambuBus() : UARTDevice() {}

    void setup() override;
    void loop() override;
    // 添加 DE 引脚的设置方法
    void set_de_pin(esphome::GPIOPin *de_pin) { this->de_pin_ = de_pin; }
    void send_uart_with_de(const uint8_t *data, uint16_t length); // 用于带 DE 控制发送的新方法
    package_type BambuBus_run();
private:
    bool need_debug = true;

};

// CRC instances
CRC16 crc_16{0x1021, 0x913D, 0, false, false};
CRC8 crc_8{0x39, 0x66, 0, false, false};
CRC8 _RX_IRQ_crcx{0x39, 0x66, 0x00, false, false};