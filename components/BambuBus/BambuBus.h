#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h" // Already included by uart.h usually, but good for explicitness
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h" 
#include "crc.h" // Assumed to be in the same directory or include path

// Moved from .cpp, makes sense to be here
static constexpr const char *TAG = "BambuBus";
static constexpr uint32_t BAMBUBUS_PROTOCOL_VERSION = 5; // Renamed from Bambubus_version for clarity
static constexpr uint32_t FLASH_DATA_CHECKSUM = 0x40614061;

// Protocol-specific enums
enum FilamentStatus { // Renamed from _filament_status
    FILAMENT_OFFLINE,
    FILAMENT_ONLINE,
    FILAMENT_NFC_WAITING
};

enum FilamentMotionStateSet { // Renamed from _filament_motion_state_set
    FILAMENT_MOTION_NEED_PULL_BACK,
    FILAMENT_MOTION_NEED_SEND_OUT,
    FILAMENT_MOTION_ON_USE,
    FILAMENT_MOTION_IDLE
};

enum BambuBusPackageType { // Renamed from package_type
    PACKAGE_ERROR = -1,
    PACKAGE_NONE = 0,
    PACKAGE_FILAMENT_MOTION_SHORT,
    PACKAGE_FILAMENT_MOTION_LONG,
    PACKAGE_ONLINE_DETECT,
    PACKAGE_REQ_X6, // Renamed from BambuBus_package_REQx6
    PACKAGE_NFC_DETECT,
    PACKAGE_SET_FILAMENT,
    PACKAGE_LONG_MC_ONLINE,
    PACKAGE_LONG_FILAMENT_INFO, // Renamed from BambuBus_longe_package_filament
    PACKAGE_LONG_VERSION_INFO,  // Renamed from BambuBus_long_package_version
    PACKAGE_HEARTBEAT,
    PACKAGE_ETC, // Uncategorized/Other
    __PACKAGE_TYPE_SIZE // For sizing, if needed
};


// Data structures - kept similar to original as per requirement
struct FilamentData { // Renamed from _filament
    char id[8] = "GFG00";
    uint8_t color_r = 0xFF;
    uint8_t color_g = 0xFF;
    uint8_t color_b = 0xFF;
    uint8_t color_a = 0xFF;
    int16_t temperature_min = 220;
    int16_t temperature_max = 240;
    char name[20] = "PETG";
    float meters = 0;
    FilamentStatus status = FILAMENT_ONLINE; // enum used
    FilamentMotionStateSet motion_set = FILAMENT_MOTION_IDLE; // enum used
    uint16_t pressure = 0;
};

#define MAX_AMS_COUNT 4 // Assuming max 4 AMS units
#define MAX_SLOTS_PER_AMS 4 // Assuming max 4 slots per AMS

struct alignas(4) FlashSaveData { // Renamed from flash_save_struct
    FilamentData filament[MAX_AMS_COUNT][MAX_SLOTS_PER_AMS];
    int now_filament_num = 0; // Renamed from BambuBus_now_filament_num
    uint32_t version = BAMBUBUS_PROTOCOL_VERSION;
    uint32_t check = 0; // Initialized to 0, set before saving
};

#pragma pack(push, 1)
struct LongPackageData { // Renamed from long_packge_data
    uint16_t package_number;
    uint16_t package_length;
    uint8_t crc8;
    uint16_t target_address;
    uint16_t source_address;
    uint16_t type;
    uint8_t *data_payload; // Renamed from datas
    uint16_t data_payload_length; // Renamed from data_length
};
#pragma pack(pop)


class BambuBus : public esphome::Component, public esphome::uart::UARTDevice {
public:
    BambuBus();

    void setup() override;
    void loop() override;
    void dump_config() override;

    void set_de_pin(esphome::GPIOPin *de_pin) { this->de_pin_ = de_pin; }

    // Public interface for filament data (if needed by other ESPHome components, e.g., sensors)
    // These are just examples, expand as needed
    int get_now_filament_num() const;
    float get_filament_meters(int ams_idx, int slot_idx) const;
    FilamentStatus get_filament_status(int ams_idx, int slot_idx) const;
    void set_filament_online_status(int ams_idx, int slot_idx, bool is_online); // Example external control
    void reset_filament_meters_action(int ams_idx, int slot_idx); // Example for an action


protected:
    // Internal helper methods, formerly global functions
    bool load_preferences();
    void save_preferences();
    void initialize_default_data();
    void set_need_to_save_preferences(bool need_save = true);

    void process_received_byte(uint8_t byte);
    BambuBusPackageType determine_package_type(const uint8_t *buffer, int length);
    void handle_received_frame(const uint8_t *frame_buffer, int length);
    
    // Protocol send/receive logic
    bool check_payload_crc16(const uint8_t *data, int data_length);
    void send_frame_with_crc(uint8_t *data, int data_length);
    void send_long_package(LongPackageData &package_data);
    void parse_long_package(const uint8_t *buffer, int length, LongPackageData &parsed_data);

    // Specific packet handlers
    void handle_heartbeat();
    void handle_filament_motion_short(const uint8_t *request_frame, int length);
    void handle_filament_motion_long(const uint8_t *request_frame, int length);
    void handle_online_detect(const uint8_t *request_frame, int length);
    void handle_req_x6(const uint8_t *request_frame, int length); // Placeholder, original was empty
    void handle_nfc_detect(const uint8_t *request_frame, int length);
    void handle_set_filament(const uint8_t *request_frame, int length);
    void handle_long_mc_online(const uint8_t *request_frame, int length);
    void handle_long_filament_info(const uint8_t *request_frame, int length);
    void handle_long_version_info(const uint8_t *request_frame, int length);

    // Helper for motion state
    uint8_t get_filament_status_flags_for_ams(uint8_t ams_idx);
    void prepare_motion_response_data(uint8_t *response_payload, uint8_t ams_idx, uint8_t current_filament_slot);
    bool update_filament_motion_state(uint8_t ams_idx, uint8_t filament_slot, uint8_t status_flags, uint8_t motion_flag);
    
    void send_uart_payload(const uint8_t *data, uint16_t length);

    // Member variables
    esphome::GPIOPin *de_pin_{nullptr};
    bool preferences_initialized_{false};
    esphome::ESPPreferenceObject pref_;
    FlashSaveData persistent_data_; // Holds the data loaded from/to be saved to flash

    bool need_to_save_preferences_{false};
    uint16_t current_bus_address_{0}; // Formerly BambuBus_address
    // uint8_t active_ams_count_{1}; // Formerly AMS_num, if it means count. If it's an index, usage needs review. Original was fixed to 1.

    // RX processing state
    uint8_t rx_frame_buffer_[1000]; // Formerly BambuBus_data_buf
    int rx_frame_pos_{0};           // Formerly static _index in RX_IRQ
    int rx_expected_length_{500};   // Formerly static length in RX_IRQ
    uint8_t rx_data_len_idx_{0};    // Formerly static data_length_index
    uint8_t rx_header_crc_idx_{0};  // Formerly static data_CRC8_index

    uint8_t tx_buffer_[1000]; // Formerly packge_send_buf
    
    // Temporary buffer for processing a complete received frame
    uint8_t current_processing_frame_[1000]; // Formerly buf_X
    int current_processing_frame_len_{0}; // Formerly BambuBus_have_data

    uint8_t current_package_num_{0}; // Formerly package_num

    uint32_t last_heartbeat_time_{0};
    uint32_t last_motion_command_time_{0};

    // CRC objects
    CRC16 crc16_calculator_;
    CRC8 crc8_calculator_; // For general use
    CRC8 rx_header_crc_calculator_; // Specifically for RX header parsing

    // For NFC detect feature
    int nfc_detect_countdown_{0}; // Formerly last_detect
    uint8_t nfc_detected_filament_slot_flags_{0}; // Formerly filament_flag_detected

    LongPackageData received_long_package_data_; // Formerly printer_data_long

    // Static response templates (original were global arrays)
    // These are modified before sending, so they can't be const.
    // Consider making them const and copying to tx_buffer_ for modification if strictness is desired.
    static uint8_t RESPONSE_TEMPLATE_MOTION_SHORT[];
    static uint8_t RESPONSE_TEMPLATE_MOTION_LONG[];
    static uint8_t RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE[];
    // static uint8_t RESPONSE_TEMPLATE_REQ_X6[]; // Original was commented out logic
    static uint8_t RESPONSE_TEMPLATE_NFC_DETECT[];
    static uint8_t RESPONSE_TEMPLATE_SET_FILAMENT_ACK[];
    static uint8_t PAYLOAD_LONG_MC_ONLINE_RESPONSE[];
    static uint8_t PAYLOAD_LONG_FILAMENT_INFO_RESPONSE_TEMPLATE[];
    static uint8_t PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE[];
    static uint8_t PAYLOAD_LONG_VERSION_AMS_LITE_RESPONSE[];
    static uint8_t PAYLOAD_LONG_VERSION_AMS08_RESPONSE[];

    static const uint8_t SERIAL_NUMBER_DATA[];
};