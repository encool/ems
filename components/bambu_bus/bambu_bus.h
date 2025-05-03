#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h" // <<<--- 确保包含 GPIOPin 定义

#include "crc.h"

enum _filament_status
{
    offline,
    online,
    NFC_waiting
};
enum _filament_motion_state_set
{
    idle,
    need_send_out,
    on_use,
    need_pull_back
};
enum package_type
{
    BambuBus_package_NONE,
    BambuBus_package_filament_motion_short,
    BambuBus_package_filament_motion_long,
    BambuBus_package_online_detect,
    BambuBus_package_REQx6,
    BambuBus_package_NFC_detect,
    BambuBus_package_set_filament,
    BambuBus_package_heartbeat,
    BambuBus_package_ETC,
    BambuBus_package_ERROR,
    BambuBus_long_package_MC_online,
    BambuBus_longe_package_filament,
    BambuBus_long_package_version
};

struct _filament
{
    char ID[8] = "GFG00";
    uint8_t color_R = 0xFF;
    uint8_t color_G = 0xFF;
    uint8_t color_B = 0xFF;
    uint8_t color_A = 0xFF;
    int16_t temperature_min = 220;
    int16_t temperature_max = 240;
    char name[20] = "PETG";
    float meters = 0;
    _filament_status statu = online;
    _filament_motion_state_set motion_set = idle;
    uint16_t pressure = 0;
};

struct flash_save_struct
{
    _filament filament[4][4];
    int BambuBus_now_filament_num = 0;
    uint32_t version = 1; // Bambubus_version
    uint32_t check = 0x40614061;
};

namespace bambu_bus
{
    class BambuBus : public esphome::Component, public esphome::uart::UARTDevice
    {

        // 添加这个声明
    protected:
        esphome::ESPPreferenceObject pref_;
        esphome::GPIOPin *de_pin_{nullptr}; // <<<--- 添加 DE 引脚成员变量

    public:
        BambuBus() : UARTDevice() {}

        void setup() override;
        void loop() override;
        // 添加 DE 引脚的设置方法
        void set_de_pin(esphome::GPIOPin *de_pin) { this->de_pin_ = de_pin; }

        package_type BambuBus_run();
        void set_need_to_save();
        int get_now_filament_num();
        void reset_filament_meters(int num);
        void add_filament_meters(int num, float meters);
        float get_filament_meters(int num);
        void set_filament_online(int num, bool if_online);
        bool get_filament_online(int num);
        void set_filament_motion(int num, _filament_motion_state_set motion);
        _filament_motion_state_set get_filament_motion(int num);
        static constexpr const char *TAG = "BambuBus"; // 必须这样定义

    private:
        void BambuBUS_UART_Init();
        void BambuBus_init();
        bool Bambubus_read();
        void Bambubus_save();
        void RX_IRQ(uint8_t data);
        void send_uart(const uint8_t *data, uint16_t length);
        void send_uart_with_de(const uint8_t *data, uint16_t length); // 用于带 DE 控制发送的新方法
        bool package_check_crc16(uint8_t *data, int data_length);
        void package_send_with_crc(uint8_t *data, int data_length);
        package_type get_packge_type(uint8_t *buf, int length);

        // Response handlers
        void send_for_Cxx(uint8_t *buf, int length);
        void send_for_Dxx(uint8_t *buf, int length);
        void send_for_REQx6(uint8_t *buf, int length);
        void send_for_Fxx(uint8_t *buf, int length);
        void send_for_NFC_detect(uint8_t *buf, int length);
        void send_for_long_packge_MC_online(uint8_t *buf, int length);
        void send_for_long_packge_filament(uint8_t *buf, int length);
        void send_for_long_packge_version(uint8_t *buf, int length);
        void send_for_Set_filament(uint8_t *buf, int length);

        // Helper methods needed by handlers
        uint8_t get_filament_left_char(uint8_t ams_id);
        void set_motion_res_datas(uint8_t *set_buf, uint8_t ams_id, uint8_t read_num);
        bool set_motion(uint8_t ams_id, uint8_t read_num, uint8_t statu_flags, uint8_t fliment_motion_flag);

#pragma pack(push, 1) // 将结构体按1字节对齐
        struct long_packge_data
        {
            uint16_t package_number;
            uint16_t package_length;
            uint8_t crc8;
            uint16_t target_address;
            uint16_t source_address;
            uint16_t type;
            uint8_t *datas;
            uint16_t data_length;
        };
#pragma pack(pop) // 恢复默认对齐

        // Long package helpers
        void Bambubus_long_package_analysis(uint8_t *buf, int data_length, long_packge_data *data);
        void Bambubus_long_package_send(long_packge_data *data);
        long_packge_data parsed_long_package; // To store parsed result

        // State variables for RX parsing (moved from static local in RX_IRQ)
        int rx_index_ = 0;
        int rx_length_ = 500;
        uint8_t rx_data_length_index_ = 0;
        uint8_t rx_data_CRC8_index_ = 0;

        // CRC instances
        CRC16 crc_16{0x1021, 0x913D, 0, false, false};
        CRC8 crc_8{0x39, 0x66, 0, false, false};
        CRC8 _RX_IRQ_crcx{0x39, 0x66, 0x00, false, false};

        // Buffers and state
        uint8_t BambuBus_data_buf[1000];
        uint8_t buf_X[1000];
        uint8_t packge_send_buf[1000];
        int BambuBus_have_data = 0;
        uint16_t BambuBus_address = 0;
        bool Bambubus_need_to_save = false;
        uint8_t package_num = 0;
        bool need_debug = true;
        bool need_res_for_06 = false;
        uint8_t res_for_06_num = 0xFF;
        int last_detect = 0;
        uint8_t filament_flag_detected = 0;

        // Data storage
        flash_save_struct data_save;

// 3D E0 3C 12 04 00 00 00 00 09 09 09 00 00 00 00 00 00 00
// 02 00 E9 3F 14 BF 00 00 76 03 6A 03 6D 00 E5 FB 99 14 2E 19 6A 03 41 F4 C3 BE E8 01 01 01 01 00 00 00 00 64 64 64 64 0A 27
// 3D E0 2C C9 03 00 00
// 04 01 79 30 61 BE 00 00 03 00 44 00 12 00 FF FF FF FF 00 00 44 00 54 C1 F4 EE E7 01 01 01 01 00 00 00 00 FA 35
#define C_test 0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x80, 0xBF, \
               0x00, 0x00, 0x00, 0x00, \
               0x36, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x27, 0x00, \
               0x55,                   \
               0xFF, 0xFF, 0xFF, 0xFF, \
               0xFF, 0xFF, 0xFF, 0xFF,
        // #define C_test 0x00, 0x00, 0x02, 0x02, \
        // 0x00, 0x00, 0x00, 0x00, \
        // 0x00, 0x00, 0x00, 0xC0, \
        // 0x36, 0x00, 0x00, 0x00, \
        // 0xFC, 0xFF, 0xFC, 0xFF, \
        // 0x00, 0x00, 0x27, 0x00, \
        // 0x55,                   \
        // 0xC1, 0xC3, 0xEC, 0xBC, \
        // 0x01, 0x01, 0x01, 0x01,
        // 00 00 02 02 EB 8F CA 3F 49 48 E7 1C 97 00 E7 1B F3 FF F2 FF 00 00 90 00 75 F8 EE FC F0 B6 B8 F8 B0 00 00 00 00 FF FF FF FF*/
        
        // #define C_test 0x00, 0x00, 0x02, 0x01, \
        // 0xF8, 0x65, 0x30, 0xBF, \
        // 0x00, 0x00, 0x28, 0x03, \
        // 0x2A, 0x03, 0x6F, 0x00, \
        // 0xB6, 0x04, 0xFC, 0xEC, \
        // 0xDF, 0xE7, 0x44, 0x00, \
        // 0x04, \
        // 0xC3, 0xF2, 0xBF, 0xBC, \
        // 0x01, 0x01, 0x01, 0x01,
        uint8_t Cxx_res[44] = {0x3D, 0xE0, 0x2C, 0x1A, 0x03,
                               C_test 0x00, 0x00, 0x00, 0x00,
                               0x90, 0xE4};
        // Response templates
        // uint8_t Cxx_res[42] = {0x3D, 0xE0, 0x2C, 0x1A, 0x03,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x80, 0xBF,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x36, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x27, 0x00,
        //                        0x55,
        //                        0xFF, 0xFF, 0xFF, 0xFF,
        //                        0xFF, 0xFF, 0xFF, 0xFF,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x90, 0xE4};

        /*
        0x00, 0x00, 0x00, 0xFF, // 0x0C...
        0x00, 0x00, 0x80, 0xBF, // distance
        0x00, 0x00, 0x00, 0xC0,
        0x00, 0xC0, 0x5D, 0xFF,
        0xFE, 0xFF, 0xFE, 0xFF, // 0xFE, 0xFF, 0xFE, 0xFF,
        0x00, 0x44, 0x00, 0x00,
        0x10,
        0xC1, 0xC3, 0xEC, 0xBC,
        0x01, 0x01, 0x01, 0x01,
        */
        unsigned char Dxx_res[62] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
                                     0x00, //[5]AMS num
                                     0x01,
                                     0x01,
                                     1,                      // humidity wet
                                     0x04, 0x04, 0x04, 0xFF, // flags
                                     0x00, 0x00, 0x00, 0x00,
                                     C_test 0x00, 0x00, 0x00, 0x00,
                                     0xFF, 0xFF, 0xFF, 0xFF,
                                     0x90, 0xE4};
        // unsigned char Dxx_res2[62] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
        //     0x00, 0x75, 0x01, 0x11,
        //     0x0C, 0x04, 0x04, 0x03,
        //     0x08, 0x00, 0x00, 0x00,
        //     0x00, 0x00, 0x03, 0x03,
        //     0x5F, 0x6E, 0xD7, 0xBE,
        //     0x00, 0x00, 0x03, 0x00,
        //     0x44, 0x00, 0x01, 0x00,
        //     0xFE, 0xFF, 0xFE, 0xFF,
        //     0x00, 0x00, 0x00, 0x00,
        //     0x50,
        //     0xC1, 0xC3, 0xED, 0xE9,
        //     0x01, 0x01, 0x01, 0x01,
        //     0x00, 0x00, 0x00, 0x00,
        //     0xFF, 0xFF, 0xFF, 0xFF,
        //     0xEC, 0xF0};

        // uint8_t Dxx_res[62] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
        //                        0x00, 0x01, 0x01, 0x01,
        //                        0x04, 0x04, 0x04, 0xFF,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x80, 0xBF,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x36, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0x00, 0x00, 0x27, 0x00,
        //                        0x55,
        //                        0xFF, 0xFF, 0xFF, 0xFF,
        //                        0xFF, 0xFF, 0xFF, 0xFF,
        //                        0x00, 0x00, 0x00, 0x00,
        //                        0xFF, 0xFF, 0xFF, 0xFF,
        //                        0x90, 0xE4};

        uint8_t F01_res[29] = {
            0x3D, 0xC0, 0x1D, 0xB4, 0x05, 0x01, 0x00,
            0x16,
            0x0E, 0x7D, 0x32, 0x31, 0x31, 0x38, 0x15, 0x00, 0x36, 0x39, 0x37, 0x33, 0xFF, 0xFF, 0xFF, 0xFF,
            0x00, 0x00, 0x00, 0x33, 0xF0};

        uint8_t NFC_detect_res[13] = {0x3D, 0xC0, 0x0D, 0x6F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xE8};

        uint8_t Set_filament_res[8] = {0x3D, 0xC0, 0x08, 0xB2, 0x08, 0x60, 0xB4, 0x04};

        // Long package data
        uint8_t long_packge_MC_online[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        uint8_t long_packge_filament[131] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x47, 0x46, 0x42, 0x30, 0x30, 0x00, 0x00, 0x00,
            0x41, 0x42, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xDD, 0xB1, 0xD4, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x18, 0x01, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        uint8_t long_packge_version_serial_number[66] = {9, 'S', 'T', 'U', 'D', 'Y', 'O', 'N', 'L', 'Y', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                         0x30, 0x30, 0x30, 0x30,
                                                         0xFF, 0xFF, 0xFF, 0xFF,
                                                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBB, 0x44, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

        uint8_t long_packge_version_version_and_name_AMS_lite[21] = {0x00, 0x00, 0x00, 0x00,
                                                                     0x41, 0x4D, 0x53, 0x5F, 0x46, 0x31, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        uint8_t long_packge_version_version_and_name_AMS08[21] = {0x00, 0x00, 0x00, 0x00,
                                                                  0x41, 0x4D, 0x53, 0x30, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    };
}