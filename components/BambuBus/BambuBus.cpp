#include "filament_state_select.h" // Include to call methods on select entity
#include "filament_motor_select.h" // Include to call methods on select entity
#include "BambuBus.h"
#include "crc.h"
#include <string.h>
#include <stdio.h>
#include "esphome/core/helpers.h" // <<<--- 添加这一行

// Global pointer to hold the single BambuBus instance
esphome::BambuBus *g_bambu_bus_instance = nullptr;

// 定义并初始化全局 CRC 对象 (只在这里做一次)
CRC16 crc_16{0x1021, 0x913D, 0, false, false};
CRC8 crc_8{0x39, 0x66, 0, false, false};
CRC8 _RX_IRQ_crcx{0x39, 0x66, 0x00, false, false};

uint8_t BambuBus_data_buf[1000];
int BambuBus_have_data = 0;
uint16_t BambuBus_address = 0;
uint8_t AMS_num = 1;

std::string packageTypeToString(package_type type)
{
    switch (type)
    {
    case BambuBus_package_ERROR:
        return "BambuBus_package_ERROR";
    case BambuBus_package_NONE:
        return "BambuBus_package_NONE";
    case BambuBus_package_filament_motion_short:
        return "BambuBus_package_filament_motion_short";
    case BambuBus_package_filament_motion_long:
        return "BambuBus_package_filament_motion_long";
    case BambuBus_package_online_detect:
        return "BambuBus_package_online_detect";
    case BambuBus_package_REQx6:
        return "BambuBus_package_REQx6";
    case BambuBus_package_NFC_detect:
        return "BambuBus_package_NFC_detect";
    case BambuBus_package_set_filament:
        return "BambuBus_package_set_filament";
    case BambuBus_long_package_MC_online:
        return "BambuBus_long_package_MC_online";
    case BambuBus_longe_package_filament: // 匹配您提供的拼写
        return "BambuBus_longe_package_filament";
    case BambuBus_long_package_version:
        return "BambuBus_long_package_version";
    case BambuBus_package_heartbeat:
        return "BambuBus_package_heartbeat";
    case BambuBus_package_ETC:
        return "BambuBus_package_ETC";
    case __BambuBus_package_packge_type_size: // 匹配您提供的拼写
        return "__BambuBus_package_packge_type_size";
    default:
        // 处理未知的枚举值，可以返回一个默认字符串或抛出异常
        return "Unknown_package_type (" + std::to_string(static_cast<int>(type)) + ")";
    }
}

struct _filament
{
    // AMS statu
    char ID[8] = "GFG00";
    uint8_t color_R = 0xFF;
    uint8_t color_G = 0xFF;
    uint8_t color_B = 0xFF;
    uint8_t color_A = 0xFF;
    int16_t temperature_min = 220;
    int16_t temperature_max = 240;
    char name[20] = "PETG";

    float meters = 0;
    _filament_status statu = offline;
    // printer_set
    _filament_motion_state_set motion_set = idle;
    uint16_t pressure = 0;
};

#define use_flash_addr ((uint32_t)0x0800F000)

struct alignas(4) flash_save_struct
{
    _filament filament[4][4];
    int BambuBus_now_filament_num = 0;
    uint32_t version = Bambubus_version;
    uint32_t check = 0x40614061;
} data_save;

bool Bambubus_need_to_save = false;
void Bambubus_set_need_to_save()
{
    Bambubus_need_to_save = true;
}
bool Bambubus_read()
{
    if (g_bambu_bus_instance == nullptr || !g_bambu_bus_instance->is_initialized_())
    {
        ESP_LOGW(TAG, "Bambubus_read: BambuBus instance not ready for preferences.");
        return false;
    }

    flash_save_struct temp_data; // 临时结构用于加载
    // 使用 g_bambu_bus_instance 指向的实例的 pref_ 对象加载数据
    if (g_bambu_bus_instance->pref_.load(&temp_data))
    {
        if ((temp_data.check == 0x40614061) && (temp_data.version == Bambubus_version))
        {
            memcpy(&data_save, &temp_data, sizeof(data_save));
            ESP_LOGI(TAG, "Successfully loaded data from flash.");
            return true;
        }
        else
        {
            ESP_LOGW(TAG, "Flash data checksum or version mismatch. (check: 0x%08X, version: %u)", temp_data.check, temp_data.version);
        }
    }
    else
    {
        ESP_LOGD(TAG, "Failed to load data from flash or no data found.");
    }
    return false;
}

void Bambubus_save()
{
    if (g_bambu_bus_instance == nullptr || !g_bambu_bus_instance->is_initialized_())
    {
        ESP_LOGE(TAG, "Bambubus_save: BambuBus instance not ready for preferences.");
        return;
    }

    data_save.version = Bambubus_version;
    data_save.check = 0x40614061; // 设置校验和

    // 使用 g_bambu_bus_instance 指向的实例的 pref_ 对象保存数据
    if (g_bambu_bus_instance->pref_.save(&data_save))
    {
        ESP_LOGI(TAG, "Successfully saved data to flash.");
        Bambubus_need_to_save = false; // 成功保存后清除标志
    }
    else
    {
        ESP_LOGE(TAG, "Failed to save data to flash!");
    }
}

int get_now_filament_num()
{
    return data_save.BambuBus_now_filament_num;
}

void reset_filament_meters(int num)
{
    data_save.filament[num / 4][num % 4].meters = 0;
}
void add_filament_meters(int num, float meters)
{
    data_save.filament[num / 4][num % 4].meters += meters;
}

void set_filament_meters(int num, float meters)
{
    data_save.filament[num / 4][num % 4].meters = meters;
}
float get_filament_meters(int num)
{
    return data_save.filament[num / 4][num % 4].meters;
}
void set_filament_online(int num, bool if_online)
{
    if (if_online)
        data_save.filament[num / 4][num % 4].statu = online;
    else
        data_save.filament[num / 4][num % 4].statu = offline;
}

bool get_filament_online(int num)
{
    if (data_save.filament[num / 4][num % 4].statu == offline)
    {
        return false;
    }
    else
    {
        return true;
    }
}
void set_filament_motion(int num, _filament_motion_state_set motion)
{
    data_save.filament[num / 4][num % 4].motion_set = motion;
}
_filament_motion_state_set get_filament_motion(int num)
{
    return data_save.filament[num / 4][num % 4].motion_set;
}

uint8_t buf_X[1000];
// CRC8 _RX_IRQ_crcx(0x39, 0x66, 0x00, false, false);
void inline RX_IRQ(unsigned char _RX_IRQ_data)
{
    static int _index = 0;
    static int length = 500;
    static uint8_t data_length_index;
    static uint8_t data_CRC8_index;
    unsigned char data = _RX_IRQ_data;

    if (_index == 0)
    {
        if (data == 0x3D)
        {
            BambuBus_data_buf[0] = 0x3D;
            _RX_IRQ_crcx.restart();
            _RX_IRQ_crcx.add(0x3D);
            data_length_index = 4;
            length = data_CRC8_index = 6;
            _index = 1;
        }
        return;
    }
    else
    {
        BambuBus_data_buf[_index] = data;
        if (_index == 1)
        {
            if (data & 0x80)
            {
                data_length_index = 2;
                data_CRC8_index = 3;
            }
            else
            {
                data_length_index = 4;
                data_CRC8_index = 6;
            }
        }
        if (_index == data_length_index)
        {
            length = data;
        }
        if (_index < data_CRC8_index)
        {
            _RX_IRQ_crcx.add(data);
        }
        else if (_index == data_CRC8_index)
        {
            if (data != _RX_IRQ_crcx.calc())
            {
                ESP_LOGW(TAG, "CRC mismatch! Resetting frame parser");
                _index = 0;
                return;
            }
        }
        ++_index;
        if (_index >= length)
        {
            _index = 0;
            memcpy(buf_X, BambuBus_data_buf, length);
            BambuBus_have_data = length;
            // 使用 ESPHome 的 format_hex_pretty
            std::string hexdump = esphome::format_hex_pretty(buf_X, length);
            // 注意：可能需要分行打印，如果 hexdump 太长
            ESP_LOGD(TAG, "Received Data:\n%s length (%d bytes)", hexdump.c_str(), BambuBus_have_data);
        }
        if (_index >= 999)
        {
            _index = 0;
        }
    }
}

void BambuBus_init()
{
    // 尝试从 Flash 加载数据
    bool _init_ready = Bambubus_read();
    if (!_init_ready)
    {
        data_save.filament[0][0].color_R = 0xFF;
        data_save.filament[0][0].color_G = 0x00;
        data_save.filament[0][0].color_B = 0x00;
        data_save.filament[0][1].color_R = 0x00;
        data_save.filament[0][1].color_G = 0xFF;
        data_save.filament[0][1].color_B = 0x00;
        data_save.filament[0][2].color_R = 0x00;
        data_save.filament[0][2].color_G = 0x00;
        data_save.filament[0][2].color_B = 0xFF;
        data_save.filament[0][3].color_R = 0x88;
        data_save.filament[0][3].color_G = 0x88;
        data_save.filament[0][3].color_B = 0x88;

        data_save.filament[1][0].color_R = 0xC0;
        data_save.filament[1][0].color_G = 0x20;
        data_save.filament[1][0].color_B = 0x20;
        data_save.filament[1][1].color_R = 0x20;
        data_save.filament[1][1].color_G = 0xC0;
        data_save.filament[1][1].color_B = 0x20;
        data_save.filament[1][2].color_R = 0x20;
        data_save.filament[1][2].color_G = 0x20;
        data_save.filament[1][2].color_B = 0xC0;
        data_save.filament[1][3].color_R = 0x60;
        data_save.filament[1][3].color_G = 0x60;
        data_save.filament[1][3].color_B = 0x60;

        data_save.filament[2][0].color_R = 0x80;
        data_save.filament[2][0].color_G = 0x40;
        data_save.filament[2][0].color_B = 0x40;
        data_save.filament[2][1].color_R = 0x40;
        data_save.filament[2][1].color_G = 0x80;
        data_save.filament[2][1].color_B = 0x40;
        data_save.filament[2][2].color_R = 0x40;
        data_save.filament[2][2].color_G = 0x40;
        data_save.filament[2][2].color_B = 0x80;
        data_save.filament[2][3].color_R = 0x40;
        data_save.filament[2][3].color_G = 0x40;
        data_save.filament[2][3].color_B = 0x40;

        data_save.filament[3][0].color_R = 0x40;
        data_save.filament[3][0].color_G = 0x20;
        data_save.filament[3][0].color_B = 0x20;
        data_save.filament[3][1].color_R = 0x20;
        data_save.filament[3][1].color_G = 0x40;
        data_save.filament[3][1].color_B = 0x20;
        data_save.filament[3][2].color_R = 0x20;
        data_save.filament[3][2].color_G = 0x20;
        data_save.filament[3][2].color_B = 0x40;
        data_save.filament[3][3].color_R = 0x20;
        data_save.filament[3][3].color_G = 0x20;
        data_save.filament[3][3].color_B = 0x20;
    }
    // 初始化每个耗材槽的默认状态和动态属性
    for (auto &ams_slots : data_save.filament)
    {
        for (auto &slot : ams_slots)
        {
            strncpy(slot.ID, "GFG00", sizeof(slot.ID) - 1); // 默认ID
            slot.ID[sizeof(slot.ID) - 1] = '\0';
            slot.color_A = 0xFF;                               // 默认Alpha
            slot.temperature_min = 220;                        // 默认最低温度
            slot.temperature_max = 240;                        // 默认最高温度
            strncpy(slot.name, "PETG", sizeof(slot.name) - 1); // 默认名称
            slot.name[sizeof(slot.name) - 1] = '\0';
            slot.meters = 0; // 默认使用长度
            // slot.statu = online; // 默认状态
            slot.statu = offline; // 默认状态

            slot.motion_set = idle; // 默认运动状态
        }
    }

    for (auto &slot_in_first_ams : data_save.filament[0])
    {
        slot_in_first_ams.statu = online;
    }

    // 为新初始化的数据设置当前耗材编号、版本和校验和
    data_save.BambuBus_now_filament_num = 0;
    data_save.version = Bambubus_version;
    data_save.check = 0x40614061;

    Bambubus_set_need_to_save(); // 标记这些默认数据需要保存到Flash

    // BambuBUS_UART_Init();
}

bool package_check_crc16(uint8_t *data, int data_length)
{
    crc_16.restart();
    data_length -= 2;
    for (auto i = 0; i < data_length; i++)
    {
        crc_16.add(data[i]);
    }
    uint16_t num = crc_16.calc();
    if ((data[(data_length)] == (num & 0xFF)) && (data[(data_length + 1)] == ((num >> 8) & 0xFF)))
        return true;
    return false;
}
bool need_debug = false;
void package_send_with_crc(uint8_t *data, int data_length)
{
    crc_8.restart();
    if (data[1] & 0x80)
    {
        for (auto i = 0; i < 3; i++)
        {
            crc_8.add(data[i]);
        }
        data[3] = crc_8.calc();
    }
    else
    {
        for (auto i = 0; i < 6; i++)
        {
            crc_8.add(data[i]);
        }
        data[6] = crc_8.calc();
    }
    crc_16.restart();
    data_length -= 2;
    for (auto i = 0; i < data_length; i++)
    {
        crc_16.add(data[i]);
    }
    uint16_t num = crc_16.calc();
    data[(data_length)] = num & 0xFF;
    data[(data_length + 1)] = num >> 8;
    data_length += 2;

    if (g_bambu_bus_instance != nullptr)
    {
        // Call the send method ON THE SPECIFIC INSTANCE
        g_bambu_bus_instance->send_uart_with_de(data, data_length);
    }
    else
    {
        // Log an error if the instance pointer hasn't been set yet
        // Use the globally defined TAG here or pass it somehow if needed
        ESP_LOGE("BambuBusLogic", "Error: BambuBus instance not available for sending in package_send_with_crc!");
    }

    // send_uart_with_de(data, data_length);
    if (need_debug)
    {
        // DEBUG_num(data, data_length);
        need_debug = false;
    }
}

uint8_t packge_send_buf[1000];

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

void Bambubus_long_package_send(long_packge_data *data)
{
    packge_send_buf[0] = 0x3D;
    packge_send_buf[1] = 0x00;
    data->package_length = data->data_length + 15;
    memcpy(packge_send_buf + 2, data, 11);
    memcpy(packge_send_buf + 13, data->datas, data->data_length);
    package_send_with_crc(packge_send_buf, data->data_length + 15);
}

void Bambubus_long_package_analysis(uint8_t *buf, int data_length, long_packge_data *data)
{
    memcpy(data, buf + 2, 11);
    data->datas = buf + 13;
    data->data_length = data_length - 15; // +2byte CRC16
}

long_packge_data printer_data_long;
package_type get_packge_type(unsigned char *buf, int length)
{
    if (package_check_crc16(buf, length) == false)
    {
        return BambuBus_package_NONE;
    }
    if (buf[1] == 0xC5)
    {

        switch (buf[4])
        {
        case 0x03:
            return BambuBus_package_filament_motion_short;
        case 0x04:
            return BambuBus_package_filament_motion_long;
        case 0x05:
            return BambuBus_package_online_detect;
        case 0x06:
            return BambuBus_package_REQx6;
        case 0x07:
            return BambuBus_package_NFC_detect;
        case 0x08:
            return BambuBus_package_set_filament;
        case 0x20:
            return BambuBus_package_heartbeat;
        default:
            return BambuBus_package_ETC;
        }
    }
    else if (buf[1] == 0x05)
    {
        Bambubus_long_package_analysis(buf, length, &printer_data_long);
        if (printer_data_long.target_address == 0x0700)
        {
            BambuBus_address = printer_data_long.target_address;
        }
        else if (printer_data_long.target_address == 0x1200)
        {
            BambuBus_address = printer_data_long.target_address;
        }

        switch (printer_data_long.type)
        {
        case 0x21A:
            return BambuBus_long_package_MC_online;
        case 0x211:
            return BambuBus_longe_package_filament;
        case 0x103:
        case 0x402:
            return BambuBus_long_package_version;
        default:
            return BambuBus_package_ETC;
        }
    }
    return BambuBus_package_NONE;
}
uint8_t package_num = 0;

uint8_t get_filament_left_char(uint8_t AMS_num)
{
    uint8_t data = 0;
    for (int i = 0; i < 4; i++)
    {
        if (data_save.filament[AMS_num][i].statu == online)
        {
            data |= (0x1 << i) << i; // 1<<(2*i)
            if (data_save.filament[AMS_num][i].motion_set != idle)
            {
                data |= (0x2 << i) << i; // 2<<(2*i)
            }
        }
    }
    return data;
}

// ========================== LOG ======================
// Helper struct to track last printed state for a given slot
struct LastPrintedFilamentInfo
{
    _filament_motion_state_set motion_set{idle};
    float meters{0.0f};
    bool online_status{false}; // To track if it's the active one for detailed logging
    // Add other fields if needed for change detection
};
// We need an array for all possible slots, assuming max 4 AMS x 4 slots = 16
static LastPrintedFilamentInfo last_printed_info[16];
static uint32_t last_log_print_time_ms = 0;
const uint32_t LOG_PRINT_INTERVAL_MS = 5000;         // Log every 5 seconds
const float METERS_CHANGE_THRESHOLD_FOR_LOG = 0.01f; // Log if meters change by at least 1cm

static const float SIMULATED_SEND_SPEED_MM_PER_SECOND = 10.0f;   // 模拟送丝速度 (mm/s)
static const float SIMULATED_PULL_SPEED_MM_PER_SECOND = 10.0f;   // 模拟退丝速度 (mm/s)
static const float SIMULATED_CONSUME_SPEED_MM_PER_SECOND = 5.0f; // 模拟打印消耗速度 (mm/s)
// 可以考虑为每个耗材槽模拟一个最大长度，用完后状态变为offline，但暂时简化处理
// static const float MAX_SIMULATED_FILAMENT_LENGTH_METERS = 50.0f;

// Helper function to decode the status byte for logging
std::string decode_ams_status_byte(uint8_t status_byte)
{
    std::string decoded_status = "Slots[";
    for (int i = 0; i < 4; ++i)
    {
        uint8_t slot_pair_status = (status_byte >> (i * 2)) & 0x03; // Get the 2 bits for slot i
        decoded_status += std::to_string(i) + ":";
        switch (slot_pair_status)
        {
        case 0x00:
            decoded_status += "Empty";
            break;
        case 0x01:
            decoded_status += "Idle";
            break;
        // case 0x02: decoded_status += "Error/Reserved"; break; // This case should not happen with current logic if online
        case 0x03:
            decoded_status += "Moving";
            break;
        default:
            decoded_status += "Unknown(" + std::to_string(slot_pair_status) + ")";
            break; // Should also not happen
        }
        if (i < 3)
            decoded_status += ", ";
    }
    decoded_status += "]";
    return decoded_status;
}

void set_motion_res_datas(unsigned char *set_buf, unsigned char AMS_num, unsigned char read_num)
{
    // unsigned char statu_flags = buf[6];

    // unsigned char fliment_motion_flag = buf[8];
    float meters = 0;
    uint8_t flagx = 0x02;
    if (read_num != 0xFF)
    {
        if (BambuBus_address == 0x700) // AMS08
        {
            meters = -data_save.filament[AMS_num][read_num].meters;
        }
        else if (BambuBus_address == 0x1200) // AMS lite
        {
            meters = data_save.filament[AMS_num][read_num].meters;
        }
    }
    set_buf[0] = AMS_num;
    set_buf[2] = flagx;
    set_buf[3] = read_num; // maybe using number
    memcpy(set_buf + 4, &meters, sizeof(meters));
    set_buf[13] = 0;
    set_buf[24] = get_filament_left_char(AMS_num);

    // ===========================================
    // Logging logic starts here
    _filament *target_filament = nullptr;
    int global_filament_idx = -1;
    unsigned char current_ams_id_char_for_logging = AMS_num + '0'; // For logging AMS 0, 1, 2, 3

    if (read_num != 0xFF && read_num < max_filament_num) // Ensure read_num is a valid slot index
    {
        global_filament_idx = AMS_num * max_filament_num + read_num;
        target_filament = &data_save.filament[AMS_num][read_num];
    }
    uint32_t current_time_ms = esphome::millis();
    bool should_print_log = false;

    if (current_time_ms - last_log_print_time_ms >= LOG_PRINT_INTERVAL_MS)
    {
        should_print_log = true;
    }

    // Check for significant changes in the currently reported filament
    if (target_filament && global_filament_idx >= 0 && global_filament_idx < 16)
    { // If a specific filament is being reported on
        bool motion_changed = (last_printed_info[global_filament_idx].motion_set != target_filament->motion_set);
        bool meters_significantly_changed = (fabs(last_printed_info[global_filament_idx].meters - meters) >= METERS_CHANGE_THRESHOLD_FOR_LOG);
        bool is_now_active_filament = (get_now_filament_num() == global_filament_idx);
        bool active_status_changed = (last_printed_info[global_filament_idx].online_status != is_now_active_filament);

        if (motion_changed || meters_significantly_changed || active_status_changed)
        {
            should_print_log = true; // Force print if key state changed
        }

        if (should_print_log)
        {
            ESP_LOGI(TAG, "Reporting AMS %c Slot %d: Motion=%s, MetersSent=%.3fm, IsActive=%s, StatusByte=0x%02X (%s)",
                     current_ams_id_char_for_logging,
                     read_num,
                     filament_state_to_string(target_filament->motion_set),
                     meters,
                     is_now_active_filament ? "Yes" : "No",
                     set_buf[24],                                  // The raw byte
                     decode_ams_status_byte(set_buf[24]).c_str()); // The decoded string

            // Update last printed state for this specific filament
            last_printed_info[global_filament_idx].motion_set = target_filament->motion_set;
            last_printed_info[global_filament_idx].meters = meters; // Store the value that was sent
            last_printed_info[global_filament_idx].online_status = is_now_active_filament;
        }
    }
    else if (read_num == 0xFF && should_print_log)
    {
        // Reporting general status for an AMS, not a specific slot
        ESP_LOGI(TAG, "Reporting AMS %c (No Specific Slot): StatusByte=0x%02X (%s)",
                 current_ams_id_char_for_logging,
                 set_buf[24],
                 decode_ams_status_byte(set_buf[24]).c_str());
    }

    if (should_print_log)
    {
        last_log_print_time_ms = current_time_ms; // Update global log print time
    }
    // =========================================
}
bool set_motion(unsigned char AMS_num, unsigned char read_num, unsigned char statu_flags, unsigned char fliment_motion_flag)
{
    int filament_global_index = AMS_num * 4 + read_num;
    if (BambuBus_address == 0x700) // AMS08
    {
        if ((read_num != 0xFF) && (read_num < 4))
        {
            if ((statu_flags == 0x03) && (fliment_motion_flag == 0x00)) // 03 00
            {
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = need_send_out;
                data_save.filament[AMS_num][read_num].pressure = 0x3600;
            }
            else if ((statu_flags == 0x09) && (fliment_motion_flag == 0xA5)) // 09 A5
            {
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = on_use;
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x7F)) // 07 7F
            {
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = on_use;
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x00)) // 07 00
            {
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = need_pull_back;
            }
        }
        else if ((read_num == 0xFF))
        {
            if ((statu_flags == 0x01) || (statu_flags == 0x03))
            {
                for (auto i = 0; i < 4; i++)
                {
                    data_save.filament[AMS_num][i].motion_set = idle;
                    data_save.filament[AMS_num][i].pressure = 0x3600;
                }
            }
        }
    }
    else if (BambuBus_address == 0x1200) // AMS lite
    {
        if (read_num < 4)
        {
            if ((statu_flags == 0x03) && (fliment_motion_flag == 0x3F)) // 03 3F
            {
                ESP_LOGI(TAG, "Set Motion (AMS Lite): Slot %d -> need_pull_back", filament_global_index);
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = need_pull_back;
            }
            else if ((statu_flags == 0x03) && (fliment_motion_flag == 0xBF)) // 03 BF
            {
                ESP_LOGI(TAG, "Set Motion (AMS Lite): Slot %d -> need_send_out", filament_global_index);
                data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
                data_save.filament[AMS_num][read_num].motion_set = need_send_out;
            }
            else
            {
                if (data_save.filament[AMS_num][read_num].motion_set == need_pull_back)
                {
                    ESP_LOGI(TAG, "Set Motion (AMS Lite): Slot %d -> idle (from pull_back)", filament_global_index);
                    data_save.filament[AMS_num][read_num].motion_set = idle;
                }
                else if (data_save.filament[AMS_num][read_num].motion_set == need_send_out)
                {
                    ESP_LOGI(TAG, "Set Motion (AMS Lite): Slot %d -> on_use (from send_out)", filament_global_index);
                    data_save.filament[AMS_num][read_num].motion_set = on_use;
                }
                else
                {
                    ESP_LOGD(TAG, "Set Motion (AMS Lite): Slot %d - No state change for flags: statu=0x%02X, motion=0x%02X", filament_global_index, statu_flags, fliment_motion_flag);
                }
            }
        }
        else if (read_num == 0xFF)
        {
            ESP_LOGI(TAG, "Set Motion (AMS Lite): All slot for 0xFF AMS_num %d -> idle", AMS_num);
            for (int i = 0; i < 4; i++)
            {
                data_save.filament[AMS_num][i].motion_set = idle;
            }
        }
    }
    else if (BambuBus_address == 0x00) // none
    {
        if ((read_num != 0xFF) && (read_num < 4))
        {
            ESP_LOGI(TAG, "Set Motion (No AMS): Slot %d -> on_use", filament_global_index);
            data_save.BambuBus_now_filament_num = AMS_num * 4 + read_num;
            data_save.filament[AMS_num][read_num].motion_set = on_use;
        }
    }
    else
        return false;
    g_bambu_bus_instance->trigger_ha_update();
    return true;
}
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
/*#define C_test 0x00, 0x00, 0x02, 0x02, \
               0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x00, 0xC0, \
               0x36, 0x00, 0x00, 0x00, \
               0xFC, 0xFF, 0xFC, 0xFF, \
               0x00, 0x00, 0x27, 0x00, \
               0x55,                   \
               0xC1, 0xC3, 0xEC, 0xBC, \
               0x01, 0x01, 0x01, 0x01,
00 00 02 02 EB 8F CA 3F 49 48 E7 1C 97 00 E7 1B F3 FF F2 FF 00 00 90 00 75 F8 EE FC F0 B6 B8 F8 B0 00 00 00 00 FF FF FF FF*/
/*
#define C_test 0x00, 0x00, 0x02, 0x01, \
                0xF8, 0x65, 0x30, 0xBF, \
                0x00, 0x00, 0x28, 0x03, \
                0x2A, 0x03, 0x6F, 0x00, \
                0xB6, 0x04, 0xFC, 0xEC, \
                0xDF, 0xE7, 0x44, 0x00, \
                0x04, \
                0xC3, 0xF2, 0xBF, 0xBC, \
                0x01, 0x01, 0x01, 0x01,*/
unsigned char Cxx_res[] = {0x3D, 0xE0, 0x2C, 0x1A, 0x03,
                           C_test 0x00, 0x00, 0x00, 0x00,
                           0x90, 0xE4};
void send_for_Cxx(unsigned char *buf, int length)
{
    Cxx_res[1] = 0xC0 | (package_num << 3);
    unsigned char AMS_num = buf[5];
    unsigned char statu_flags = buf[6];
    unsigned char read_num = buf[7];
    unsigned char fliment_motion_flag = buf[8];

    /*if (!set_motion(AMS_num, read_num, statu_flags, fliment_motion_flag))
        return;*/

    set_motion_res_datas(Cxx_res + 5, AMS_num, read_num);
    package_send_with_crc(Cxx_res, sizeof(Cxx_res));
    if (package_num < 7)
        package_num++;
    else
        package_num = 0;
}
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
unsigned char Dxx_res[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
                           0x00, //[5]AMS num
                           0x01,
                           0x01,
                           1,                      // humidity wet
                           0x04, 0x04, 0x04, 0xFF, // flags
                           0x00, 0x00, 0x00, 0x00,
                           C_test 0x00, 0x00, 0x00, 0x00,
                           0xFF, 0xFF, 0xFF, 0xFF,
                           0x90, 0xE4};
/*unsigned char Dxx_res2[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
                            0x00, 0x75, 0x01, 0x11,
                            0x0C, 0x04, 0x04, 0x03,
                            0x08, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x03, 0x03,
                            0x5F, 0x6E, 0xD7, 0xBE,
                            0x00, 0x00, 0x03, 0x00,
                            0x44, 0x00, 0x01, 0x00,
                            0xFE, 0xFF, 0xFE, 0xFF,
                            0x00, 0x00, 0x00, 0x00,
                            0x50,
                            0xC1, 0xC3, 0xED, 0xE9,
                            0x01, 0x01, 0x01, 0x01,
                            0x00, 0x00, 0x00, 0x00,
                            0xFF, 0xFF, 0xFF, 0xFF,
                            0xEC, 0xF0};*/
bool need_res_for_06 = false;
uint8_t res_for_06_num = 0xFF;
int last_detect = 0;
uint8_t filament_flag_detected = 0;

void send_for_Dxx(unsigned char *buf, int length)
{
    unsigned char filament_flag_on = 0x00;
    unsigned char filament_flag_NFC = 0x00;
    unsigned char AMS_num = buf[5];
    unsigned char statu_flags = buf[6];
    unsigned char fliment_motion_flag = buf[7];
    unsigned char read_num = buf[9];

    for (auto i = 0; i < 4; i++)
    {
        // filament[i].meters;
        if (data_save.filament[AMS_num][i].statu == online)
        {
            filament_flag_on |= 1 << i;
        }
        else if (data_save.filament[AMS_num][i].statu == NFC_waiting)
        {
            filament_flag_on |= 1 << i;
            filament_flag_NFC |= 1 << i;
        }
    }
    if (!set_motion(AMS_num, read_num, statu_flags, fliment_motion_flag))
        return;
    /*if (need_res_for_06)
    {
        Dxx_res2[1] = 0xC0 | (package_num << 3);
        Dxx_res2[9] = filament_flag_on;
        Dxx_res2[10] = filament_flag_on - filament_flag_NFC;
        Dxx_res2[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res[19] = flagx;
        Dxx_res[20] = Dxx_res2[12] = res_for_06_num;
        Dxx_res2[13] = filament_flag_NFC;
        Dxx_res2[41] = get_filament_left_char();
        package_send_with_crc(Dxx_res2, sizeof(Dxx_res2));
        need_res_for_06 = false;
    }
    else*/

    {
        Dxx_res[1] = 0xC0 | (package_num << 3);
        Dxx_res[5] = AMS_num;
        Dxx_res[9] = filament_flag_on;
        Dxx_res[10] = filament_flag_on - filament_flag_NFC;
        Dxx_res[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res[12] = read_num;
        Dxx_res[13] = filament_flag_NFC;

        set_motion_res_datas(Dxx_res + 17, AMS_num, read_num);
    }
    if (last_detect != 0)
    {
        if (last_detect > 10)
        {
            Dxx_res[19] = 0x01;
        }
        else
        {
            Dxx_res[12] = filament_flag_detected;
            Dxx_res[19] = 0x01;
            Dxx_res[20] = filament_flag_detected;
        }
        last_detect--;
    }
    package_send_with_crc(Dxx_res, sizeof(Dxx_res));
    if (package_num < 7)
        package_num++;
    else
        package_num = 0;
}
unsigned char REQx6_res[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x06,
                             0x00, 0x00, 0x00, 0x00,
                             0x04, 0x04, 0x04, 0xFF, // flags
                             0x00, 0x00, 0x00, 0x00,
                             C_test 0x00, 0x00, 0x00, 0x00,
                             0x64, 0x64, 0x64, 0x64,
                             0x90, 0xE4};
void send_for_REQx6(unsigned char *buf, int length)
{
    /*
        unsigned char filament_flag_on = 0x00;
        unsigned char filament_flag_NFC = 0x00;
        for (auto i = 0; i < 4; i++)
        {
            if (data_save.filament[AMS_num][i].statu == online)
            {
                filament_flag_on |= 1 << i;
            }
            else if (data_save.filament[AMS_num][i].statu == NFC_waiting)
            {
                filament_flag_on |= 1 << i;
                filament_flag_NFC |= 1 << i;
            }
        }
        REQx6_res[1] = 0xC0 | (package_num << 3);
        res_for_06_num = buf[7];
        REQx6_res[9] = filament_flag_on;
        REQx6_res[10] = filament_flag_on - filament_flag_NFC;
        REQx6_res[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res2[12] = res_for_06_num;
        Dxx_res2[12] = res_for_06_num;
        package_send_with_crc(REQx6_res, sizeof(REQx6_res));
        need_res_for_06 = true;
        if (package_num < 7)
            package_num++;
        else
            package_num = 0;*/
}

void NFC_detect_run()
{
    /*uint64_t time = GetTick();
    return;
    if (time > last_detect + 3000)
    {
        filament_flag_detected = 0;
    }*/
}
uint8_t online_detect_num2[] = {0x0E, 0x7D, 0x32, 0x31, 0x31, 0x38, 0x15, 0x00, // 序列号？(额外包含之前一位)
                                0x36, 0x39, 0x37, 0x33, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t online_detect_num[] = {0x90, 0x31, 0x33, 0x34, 0x36, 0x35, 0x02, 0x00, 0x37, 0x39, 0x33, 0x38, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char F01_res[] = {
    0x3D, 0xC0, 0x1D, 0xB4, 0x05, 0x01, 0x00,
    0x16,
    0x0E, 0x7D, 0x32, 0x31, 0x31, 0x38, 0x15, 0x00, 0x36, 0x39, 0x37, 0x33, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x33, 0xF0};
void send_for_Fxx(unsigned char *buf, int length)
{
    // uint8_t AMS_num;
    uint8_t F00_res[4 * sizeof(F01_res)];
    if ((buf[5] == 0x00))
    {
        // if((buf[8]==0x30)&&(buf[9]==0x31));
        for (auto i = 0; i < 4; i++)
        {
            memcpy(F00_res + i * sizeof(F01_res), F01_res, sizeof(F01_res));
            F00_res[i * sizeof(F01_res) + 5] = 0;
            F00_res[i * sizeof(F01_res) + 6] = i;
            F00_res[i * sizeof(F01_res) + 7] = i;
        }
        package_send_with_crc(F00_res, sizeof(F00_res));
    }

    if ((buf[5] == 0x01) && (buf[6] < 4))
    {
        memcpy(F01_res + 4, buf + 4, 3);
        // memcpy(F00_res + 8, online_detect_num, sizeof(online_detect_num));
        /*
        if (buf[6])
            memcpy(F00_res + 8, online_detect_num, sizeof(online_detect_num));
        else
            memcpy(F00_res + 8, online_detect_num2, sizeof(online_detect_num2));*/
        // memcpy(online_detect_num, buf + 8, sizeof(online_detect_num));
        //  F00_res[5] = buf[5];
        //  F00_res[6] = buf[6];
        package_send_with_crc(F01_res, sizeof(F01_res));
    }
}
// 3D C5 0D F1 07 00 00 00 00 00 00 CE EC
// 3D C0 0D 6F 07 00 00 00 00 00 00 9A 70

unsigned char NFC_detect_res[] = {0x3D, 0xC0, 0x0D, 0x6F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xE8};
void send_for_NFC_detect(unsigned char *buf, int length)
{
    last_detect = 20;
    filament_flag_detected = 1 << buf[6];
    NFC_detect_res[6] = buf[6];
    NFC_detect_res[7] = buf[7];
    package_send_with_crc(NFC_detect_res, sizeof(NFC_detect_res));
}

unsigned char long_packge_MC_online[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void send_for_long_packge_MC_online(unsigned char *buf, int length)
{
    long_packge_data data;
    uint8_t AMS_num = printer_data_long.datas[0];
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    if (printer_data_long.target_address == 0x0700)
    {
    }
    else if (printer_data_long.target_address == 0x1200)
    {
    }
    /*else if(printer_data_long.target_address==0x0F00)
    {

    }*/
    else
    {
        return;
    }

    data.datas = long_packge_MC_online;
    data.datas[0] = AMS_num;
    data.data_length = sizeof(long_packge_MC_online);

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char long_packge_filament[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x47, 0x46, 0x42, 0x30, 0x30, 0x00, 0x00, 0x00,
        0x41, 0x42, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xDD, 0xB1, 0xD4, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x18, 0x01, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void send_for_long_packge_filament(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);

    uint8_t AMS_num = printer_data_long.datas[0];
    uint8_t filament_num = printer_data_long.datas[1];
    long_packge_filament[0] = AMS_num;
    long_packge_filament[1] = filament_num;
    memcpy(long_packge_filament + 19, data_save.filament[AMS_num][filament_num].ID, sizeof(data_save.filament[AMS_num][filament_num].ID));
    memcpy(long_packge_filament + 27, data_save.filament[AMS_num][filament_num].name, sizeof(data_save.filament[AMS_num][filament_num].name));
    long_packge_filament[59] = data_save.filament[AMS_num][filament_num].color_R;
    long_packge_filament[60] = data_save.filament[AMS_num][filament_num].color_G;
    long_packge_filament[61] = data_save.filament[AMS_num][filament_num].color_B;
    long_packge_filament[62] = data_save.filament[AMS_num][filament_num].color_A;
    memcpy(long_packge_filament + 79, &data_save.filament[AMS_num][filament_num].temperature_max, 2);
    memcpy(long_packge_filament + 81, &data_save.filament[AMS_num][filament_num].temperature_min, 2);

    data.datas = long_packge_filament;
    data.data_length = sizeof(long_packge_filament);

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char serial_number[] = {"STUDY0ONLY"};
unsigned char long_packge_version_serial_number[] = {9, // length
                                                     'S', 'T', 'U', 'D', 'Y', 'O', 'N', 'L', 'Y', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // serial_number#2
                                                     0x30, 0x30, 0x30, 0x30,
                                                     0xFF, 0xFF, 0xFF, 0xFF,
                                                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBB, 0x44, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

unsigned char long_packge_version_version_and_name_AMS_lite[] = {0x00, 0x00, 0x00, 0x00, // verison number
                                                                 0x41, 0x4D, 0x53, 0x5F, 0x46, 0x31, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char long_packge_version_version_and_name_AMS08[] = {0x00, 0x00, 0x00, 0x00, // verison number
                                                              0x41, 0x4D, 0x53, 0x30, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void send_for_long_packge_version(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    uint8_t AMS_num = printer_data_long.datas[0];
    unsigned char *long_packge_version_version_and_name;

    if (printer_data_long.target_address == 0x0700)
    {
        long_packge_version_version_and_name = long_packge_version_version_and_name_AMS08;
    }
    else if (printer_data_long.target_address == 0x1200)
    {
        long_packge_version_version_and_name = long_packge_version_version_and_name_AMS_lite;
    }
    else
    {
        return;
    }

    switch (printer_data_long.type)
    {
    case 0x402:

        AMS_num = printer_data_long.datas[33];
        long_packge_version_serial_number[0] = sizeof(serial_number);
        memcpy(long_packge_version_serial_number + 1, serial_number, sizeof(serial_number));
        data.datas = long_packge_version_serial_number;
        data.data_length = sizeof(long_packge_version_serial_number);

        data.datas[65] = AMS_num;
        break;
    case 0x103:

        AMS_num = printer_data_long.datas[0];
        data.datas = long_packge_version_version_and_name;
        data.data_length = sizeof(long_packge_version_version_and_name_AMS08);
        data.datas[20] = AMS_num;
        break;
    default:
        return;
    }

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char s = 0x01;

unsigned char Set_filament_res[] = {0x3D, 0xC0, 0x08, 0xB2, 0x08, 0x60, 0xB4, 0x04};
void send_for_Set_filament(unsigned char *buf, int length)
{
    uint8_t read_num = buf[5];
    uint8_t AMS_num = read_num & 0xF0;
    read_num = read_num & 0x0F;
    memcpy(data_save.filament[AMS_num][read_num].ID, buf + 7, sizeof(data_save.filament[AMS_num][read_num].ID));

    data_save.filament[AMS_num][read_num].color_R = buf[15];
    data_save.filament[AMS_num][read_num].color_G = buf[16];
    data_save.filament[AMS_num][read_num].color_B = buf[17];
    data_save.filament[AMS_num][read_num].color_A = buf[18];

    memcpy(&data_save.filament[AMS_num][read_num].temperature_min, buf + 19, 2);
    memcpy(&data_save.filament[AMS_num][read_num].temperature_max, buf + 21, 2);
    memcpy(data_save.filament[AMS_num][read_num].name, buf + 23, sizeof(data_save.filament[AMS_num][read_num].name));
    package_send_with_crc(Set_filament_res, sizeof(Set_filament_res));
    Bambubus_set_need_to_save();
}

namespace esphome
{

    package_type BambuBus::BambuBus_run()
    {
        package_type stu = BambuBus_package_NONE;
        static uint64_t time_set = 0;
        static uint64_t time_motion = 0;
        static uint64_t timeout_time = 4000;

        uint32_t timex = esphome::millis(); // 使用 ESPHome 的时间函数

        // uint64_t timex = get_time64();

        /*for (auto i : data_save.filament)
        {
            i->motion_set = idle;
        }*/

        if (BambuBus_have_data)
        {
            int data_length = BambuBus_have_data;
            BambuBus_have_data = 0;
            need_debug = false;

            stu = get_packge_type(buf_X, data_length); // have_data
            // ESP_LOGI(TAG, "Processing package (Type: %s)...", packageTypeToString(stu).c_str());
            switch (stu)
            {
            case BambuBus_package_heartbeat:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_heartbeat)...");
                time_set = timex + timeout_time;
                break;
            case BambuBus_package_filament_motion_short:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_filament_motion_short)...");
                send_for_Cxx(buf_X, data_length);
                break;
            case BambuBus_package_filament_motion_long:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_filament_motion_long)...");
                send_for_Dxx(buf_X, data_length);
                time_motion = timex + timeout_time;
                break;
            case BambuBus_package_online_detect:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_online_detect)...");
                send_for_Fxx(buf_X, data_length);
                break;
            case BambuBus_package_REQx6:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_REQx6)...");
                // send_for_REQx6(buf_X, data_length);
                break;
            case BambuBus_long_package_MC_online:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_long_package_MC_online)...");
                send_for_long_packge_MC_online(buf_X, data_length);
                break;
            case BambuBus_longe_package_filament:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_longe_package_filament)...");
                send_for_long_packge_filament(buf_X, data_length);
                break;
            case BambuBus_long_package_version:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_long_package_version)...");
                send_for_long_packge_version(buf_X, data_length);
                break;
            case BambuBus_package_NFC_detect:
                ESP_LOGD(TAG, "Processing package (Type: BambuBus_package_NFC_detect)...");
                // send_for_NFC_detect(buf_X, data_length);
                break;
            case BambuBus_package_set_filament:
                ESP_LOGI(TAG, "Processing package (Type: BambuBus_package_set_filament)...");
                send_for_Set_filament(buf_X, data_length);
                break;
            default:
                // It's also good practice to log the default case, especially if it represents an unknown or unhandled package type.
                // You might want to use ESP_LOGW (Warning) or ESP_LOGE (Error) here depending on how unexpected this is.
                // For consistency with the request to just add logs, using ESP_LOGD here.
                // Consider logging the actual value that caused the default case if 'package_type' is accessible.
                // ESP_LOGW(TAG, "Processing package (Type: Unknown/Default, Value: %d)...", package_type);
                ESP_LOGW(TAG, "Processing package (Type: Unhandled/Default)...");
                break;
            }
        }
        // if (timex > time_set)
        // {
        //     stu = BambuBus_package_ERROR; // offline
        // }
        // if (timex > time_motion)
        // {
        //     for (auto i : data_save.filament) // i 是 flash_save_struct::filament数组中的一个元素，即 _filament[4]
        //     {
        //         for (auto &ams_slots : data_save.filament)
        //         { // ams_slots 是 _filament[4]
        //             for (auto &slot : ams_slots)
        //             { // slot 是 _filament
        //                 slot.motion_set = idle;
        //             }
        //         }
        //     }
        // }
        // if (Bambubus_need_to_save)
        // {
        //     Bambubus_save();
        //     time_set = timex + 1000;
        //     Bambubus_need_to_save = false;
        // }
        // // HAL_UART_Transmit(&use_Serial.handle,&s,1,1000);

        // // NFC_detect_run();
        // return stu;

        // 以下超时逻辑检查
        if (time_set != 0 && timex > time_set) // 确保 time_set 被初始化过 (从 heartbeat 收到后)
        {
            time_set = 0; // 重置超时，等待下一次心跳

            ESP_LOGW(TAG, "Heartbeat timeout, printer might be offline.");
            stu = BambuBus_package_ERROR; // offline
            // 当打印机离线时，可能需要重置所有耗材的运动状态为 idle
            for (auto &ams_slots : data_save.filament)
            {
                for (auto &slot : ams_slots)
                {
                    if (slot.motion_set != idle)
                    {
                        slot.motion_set = idle;
                        Bambubus_set_need_to_save();
                        this->trigger_ha_update();
                    }
                }
            }
        }
        if (time_motion != 0 && timex > time_motion)
        {
            time_motion = 0; // 重置超时，等待下一次运动指令

            ESP_LOGW(TAG, "Motion timeout, setting active filaments to idle if not already.");
            // 这个逻辑是：如果一个运动指令后一段时间没有新的运动指令，则认为该运动已完成/超时
            // 并将 *所有* 槽位设置为 idle。这可能需要更精细的控制，
            // 例如只重置那些处于 need_send_out 或 need_pull_back 的槽位。
            bool changed_motion_to_idle = false;
            for (auto &ams_slots : data_save.filament)
            {
                for (auto &slot : ams_slots)
                {
                    // 只改变那些正在主动运动的状态
                    if (slot.motion_set == need_send_out || slot.motion_set == need_pull_back)
                    {
                        slot.motion_set = idle;
                        changed_motion_to_idle = true;
                        ESP_LOGI(TAG, "Motion timeout: Set slot (AMS %ld, Slot %ld) to idle.", std::distance(data_save.filament, &ams_slots), std::distance(ams_slots, &slot));
                    }
                }
            }
            if (changed_motion_to_idle)
            {
                Bambubus_set_need_to_save();
                this->trigger_ha_update();
            }
        }

        if (Bambubus_need_to_save)
        {
            Bambubus_need_to_save = false;
            Bambubus_save();                 // Bambubus_save 内部会重置 Bambubus_need_to_save
            time_set = timex + timeout_time; // 这行在这里可能不需要，除非保存操作也应重置心跳超时
        }

        return stu;
    }

    void BambuBus::setup()
    {
        ESP_LOGI(TAG, "Setup started");

        g_bambu_bus_instance = this;

        const char *preference_key = "bambubus_storage_001"; // 选择一个对您的组件来说唯一的键

        // 初始化 ESPPreferenceObject
        // 使用 get_object_id_hash() 来为每个组件实例创建唯一的key
        // 第二个参数 false 表示 autosave=false，需要显式调用 save()
        this->pref_ = esphome::global_preferences->make_preference<flash_save_struct>(esphome::fnv1_hash(preference_key));
        this->mark_initialized_(); // 标记 preferences 对象已初始化并可用

        // 设置 DE 引脚 (如果已配置)
        if (this->de_pin_ != nullptr)
        {
            // GPIOBinaryOutput* 的 setup 通常由框架自动调用
            this->de_pin_->setup();              // 可能不需要
            this->de_pin_->digital_write(false); // <<<--- 使用 turn_off() 设置初始状态 (接收)
            // vvv--- 获取引脚号需要通过 get_pin() 方法 ---vvv
            ESP_LOGI(TAG, "DE Pin (GPIOBinaryOutput) configured on GPIO%d. Initial state: OFF (Receive)", this->de_pin_->dump_summary());
            // ^^^--- 注意是 de_pin_->get_pin()->get_pin() ---^^^
        }
        else
        {
            ESP_LOGI(TAG, "DE Pin not configured.");
        }

        BambuBus_init();

        this->trigger_ha_update(); // New: publish initial state based on data_save

        ESP_LOGI(TAG, "Setup ended");
    }

    void BambuBus::loop()
    {
        static uint8_t buf[1000];
        static size_t pos = 0;

        // Read incoming data
        while (available())
        {
            // ESP_LOGI(BambuBus::TAG, "data available");
            uint8_t c;
            if (read_byte(&c))
            {
                RX_IRQ(c);
            }
        }

        // Process received data
        BambuBus_run();

    }

    // 用于带 DE 控制发送的新函数
    void BambuBus::send_uart_with_de(const uint8_t *data, uint16_t length)
    {
        if (this->de_pin_ != nullptr)
        {
            this->de_pin_->digital_write(true); // 激活发送 (高电平)
            // 可能需要极短的延迟确保收发器状态切换 (通常非常快)
            esphome::delayMicroseconds(50); // 示例: 5 微秒，根据硬件调整
            ESP_LOGV(TAG, "DE pin set HIGH.");
        }
        else
        {
            ESP_LOGV(TAG, "DE pin not configured, sending without DE control.");
        }

        ESP_LOGD(TAG, "Sending %d bytes...", length);
        // 使用 format_hex_pretty 打印准备发送的数据
        if (true)
        { // 或者使用更精细的日志级别控制
            ESP_LOGD(TAG, "  %s", esphome::format_hex_pretty(data, length).c_str());
        }
        this->write_array(data, length);

        // 等待发送完成 - 非常重要!
        this->flush();
        ESP_LOGV(TAG, "UART flush complete.");

        // 在 flush() 之后再禁用 DE
        if (this->de_pin_ != nullptr)
        {
            // 在禁用 DE 之前可能需要短暂延迟，确保最后一个停止位完全发出
            esphome::delayMicroseconds(50);      // 示例: 5 微秒，根据硬件调整
            this->de_pin_->digital_write(false); // 禁用发送 (低电平)
            ESP_LOGV(TAG, "DE pin set LOW.");
        }
    }

    // In BambuBus.cpp
    _filament_motion_state_set BambuBus::get_current_selected_filament_motion_state() const
    {
        int now_num = get_now_filament_num(); // This is BambuBus_now_filament_num
        if (now_num < 0 || now_num >= 16)
        { // Basic bounds check for 4 AMS x 4 slots
            ESP_LOGW(TAG, "get_current_selected_filament_motion_state: BambuBus_now_filament_num (%d) is out of expected range.", now_num);
            return idle; // Return a default safe state
        }
        unsigned char ams_idx = now_num / 4;
        unsigned char slot_idx = now_num % 4;

        // Assuming data_save is globally accessible and populated
        return data_save.filament[ams_idx][slot_idx].motion_set;
    }

    FilamentMotionMotorIndex BambuBus::get_current_selected_motor_index() const
    {
        int now_num = get_now_filament_num();
        if (now_num < 0 || now_num >= 16)
        {
            ESP_LOGW(TAG, "get_current_selected_motor_index: BambuBus_now_filament_num (%d) is out of expected range.", now_num);
            return FilamentMotionMotorIndex::MOTOR_1; // Default
        }
        unsigned char slot_idx = now_num % 4;

        switch (slot_idx)
        {
        case 0:
            return FilamentMotionMotorIndex::MOTOR_1;
        case 1:
            return FilamentMotionMotorIndex::MOTOR_2;
        case 2:
            return FilamentMotionMotorIndex::MOTOR_3;
        case 3:
            return FilamentMotionMotorIndex::MOTOR_4;
        default: // Should not happen if % 4
            return FilamentMotionMotorIndex::MOTOR_1;
        }
    }

    char BambuBus::get_current_selected_ams_index() const
    {
        int now_num = get_now_filament_num();
        if (now_num < 0 || now_num >= 16)
        {
            ESP_LOGW(TAG, "get_current_selected_motor_index: BambuBus_now_filament_num (%d) is out of expected range.", now_num);
            return 0; // Default
        }
        unsigned char ams_idx = now_num / 4;
        return ams_idx;
    }

    // In BambuBus.cpp
    void BambuBus::publish_selected_filament_state_to_ha() // Renamed
    {
        if (this->state_select_entity_)
        {
            this->state_select_entity_->publish_state_from_parent(this->get_current_selected_filament_motion_state());
        }
    }

    void BambuBus::publish_selected_motor_index_to_ha() // Renamed
    {
        if (this->motor_select_entity_)
        {
            this->motor_select_entity_->publish_state_from_parent(this->get_current_selected_motor_index());
        }
    }

    // In BambuBus.cpp
    void BambuBus::trigger_ha_update()
    {
        ESP_LOGD(TAG, "Triggering HA update based on internal state change.");
        this->publish_selected_motor_index_to_ha();
        this->publish_selected_filament_state_to_ha();
    }

    // In BambuBus.cpp
    void BambuBus::update_ha_for_motion_state(_filament_motion_state_set new_state_for_current_filament)
    {
        // This function is called IF HA wants to change the state.
        // For now, HA is display-only, so this might not be used.
        // If used, it should update data_save for the *currently selected* filament.
        ESP_LOGD(TAG, "update_ha_for_motion_state called by HA (currently for display only or future extension) with state: %d", (int)new_state_for_current_filament);

        // To make this functional for control:
        /*
        int now_num = get_now_filament_num();
        if (now_num < 0 || now_num >= 16) return;
        unsigned char ams_idx = now_num / 4;
        unsigned char slot_idx = now_num % 4;

        if (data_save.filament[ams_idx][slot_idx].motion_set != new_state_for_current_filament) {
            data_save.filament[ams_idx][slot_idx].motion_set = new_state_for_current_filament;
            // Potentially update BambuBus_now_filament_num based on the new state
            // ... logic for that ...
            Bambubus_set_need_to_save();
            this->trigger_ha_update(); // Update HA with the change
        }
        */
    }

    void BambuBus::update_ha_for_motor_index(FilamentMotionMotorIndex new_active_motor_idx_enum)
    {
        // This function is called IF HA wants to change the selected motor.
        // For now, HA is display-only.
        ESP_LOGD(TAG, "update_ha_for_motor_index called by HA (currently for display only or future extension) with motor index: %d", (int)new_active_motor_idx_enum);

        // To make this functional for control:
        /*
        unsigned char target_slot_idx;
        switch (new_active_motor_idx_enum) {
            case FilamentMotionMotorIndex::MOTOR_1: target_slot_idx = 0; break;
            case FilamentMotionMotorIndex::MOTOR_2: target_slot_idx = 1; break;
            case FilamentMotionMotorIndex::MOTOR_3: target_slot_idx = 2; break;
            case FilamentMotionMotorIndex::MOTOR_4: target_slot_idx = 3; break;
            default: return;
        }

        // Assuming we are always operating on AMS 0 for now if controlled by HA directly
        unsigned char current_ams_idx_for_ha_control = 0; // Or some other logic
        int new_global_filament_num = current_ams_idx_for_ha_control * 4 + target_slot_idx;

        if (data_save.BambuBus_now_filament_num != new_global_filament_num) {
            data_save.BambuBus_now_filament_num = new_global_filament_num;
            // The motion state of this newly selected filament should also be considered/updated.
            // For simplicity, we just change the selection. The motion state will be read by HA.
            Bambubus_set_need_to_save();
            this->trigger_ha_update();
        }
        */
    }
}
