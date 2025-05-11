#include "BambuBus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h" // For format_hex_pretty
#include <string.h> // For memcpy, strncpy

// Initialize static response templates
// Sizes need to be exact. Using sizeof on original definitions is best.
// For brevity, I'm using placeholders for sizes. You'll need to fill these from original.
// Example: uint8_t BambuBus::RESPONSE_TEMPLATE_MOTION_SHORT[sizeof_original_Cxx_res] = {...};
// Ensure C_test macro content is properly expanded here.

#define C_TEST_DATA 0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x80, 0xBF, \
                    0x00, 0x00, 0x00, 0x00, \
                    0x36, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x00, 0x00, \
                    0x00, 0x00, 0x27, 0x00, \
                    0x55,                   \
                    0xFF, 0xFF, 0xFF, 0xFF, \
                    0xFF, 0xFF, 0xFF, 0xFF

uint8_t BambuBus::RESPONSE_TEMPLATE_MOTION_SHORT[] = {0x3D, 0xE0, 0x2C, 0x1A, 0x03,
                                                     C_TEST_DATA, 0x00, 0x00, 0x00, 0x00,
                                                     0x90, 0xE4}; // Size: 30 (5 + 21 + 4)

uint8_t BambuBus::RESPONSE_TEMPLATE_MOTION_LONG[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
                                                    0x00, //[5]AMS num
                                                    0x01, 0x01, 1,    // humidity wet
                                                    0x04, 0x04, 0x04, 0xFF, // flags
                                                    0x00, 0x00, 0x00, 0x00,
                                                    C_TEST_DATA, 0x00, 0x00, 0x00, 0x00,
                                                    0xFF, 0xFF, 0xFF, 0xFF,
                                                    0x90, 0xE4}; // Size: 46 (5 + 12 + 21 + 4 + 4)

uint8_t BambuBus::RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE[] = {
    0x3D, 0xC0, 0x1D, 0xB4, 0x05, 0x01, 0x00, // Header part, seq, ams, slot will be filled
    0x16, // Fixed value?
    // Placeholder for serial-like data, original had online_detect_num[]
    0x0E, 0x7D, 0x32, 0x31, 0x31, 0x38, 0x15, 0x00, 0x36, 0x39, 0x37, 0x33, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, // CRC placeholder (2 bytes) + 1 byte from original size calc
    0x33, 0xF0 // Example original CRC bytes
}; // Size: 31

uint8_t BambuBus::RESPONSE_TEMPLATE_NFC_DETECT[] = {0x3D, 0xC0, 0x0D, 0x6F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xE8}; // Size: 13

uint8_t BambuBus::RESPONSE_TEMPLATE_SET_FILAMENT_ACK[] = {0x3D, 0xC0, 0x08, 0xB2, 0x08, 0x60, 0xB4, 0x04}; // Size: 8

uint8_t BambuBus::PAYLOAD_LONG_MC_ONLINE_RESPONSE[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Size: 6

uint8_t BambuBus::PAYLOAD_LONG_FILAMENT_INFO_RESPONSE_TEMPLATE[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    'G', 'F', 'G', '0', '0', 0x00, 0x00, 0x00, // ID placeholder
    'A', 'B', 'S', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Name placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xDD, 0xB1, 0xD4, 0xFF, // Color placeholder (RGBA)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x01, 0xF0, 0x00, // Temp placeholder (Max, Min)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}; // Size: 122

const uint8_t BambuBus::SERIAL_NUMBER_DATA[] = "STUDY0ONLY"; // Original was char array

uint8_t BambuBus::PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE[] = {
    sizeof(BambuBus::SERIAL_NUMBER_DATA) -1, // Length, excluding null terminator if any
    // Serial number data will be copied here
    'S', 'T', 'U', 'D', 'Y', 'O', 'N', 'L', 'Y', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // serial_number#2
    0x30, 0x30, 0x30, 0x30,
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBB, 0x44, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 // AMS Num placeholder at [65]
}; // Size: 66

uint8_t BambuBus::PAYLOAD_LONG_VERSION_AMS_LITE_RESPONSE[] = {
    0x00, 0x00, 0x00, 0x00, // version number
    'A', 'M', 'S', '_', 'F', '1', '0', '2', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // AMS_F102
    0x00 // AMS_Num placeholder at [20]
}; // Size: 21 (4 + 13 + 1 + 3 padding based on original size of long_packge_version_version_and_name_AMS08)

uint8_t BambuBus::PAYLOAD_LONG_VERSION_AMS08_RESPONSE[] = {
    0x00, 0x00, 0x00, 0x00, // version number
    'A', 'M', 'S', '0', '8', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // AMS08
    0x00 // AMS_Num placeholder at [20]
}; // Size: 21

BambuBus::BambuBus() :
    // Initialize CRC calculators with parameters from original global definitions
    crc16_calculator_(0x1021, 0x913D, 0, false, false),
    crc8_calculator_(0x39, 0x66, 0, false, false), // General purpose or payload CRC
    rx_header_crc_calculator_(0x39, 0x66, 0, false, false) // For RX frame header
    {}


void BambuBus::setup() {
    ESP_LOGCONFIG(TAG, "Setting up BambuBus...");
    
    // ESPPreferenceObject setup
    // Using a fixed key, or derive from get_object_id() for multiple instances
    // this->pref_ = esphome::global_preferences->make_preference<FlashSaveData>(this->get_object_id_hash());
    // For a single instance or if key must be exact as original:
    this->pref_ = esphome::global_preferences->make_preference<FlashSaveData>(esphome::fnv1_hash("bambubus_storage_001"));
    this->preferences_initialized_ = true; // Mark as ready for load/save

    if (!this->load_preferences()) {
        ESP_LOGI(TAG, "No valid data in flash or version mismatch, initializing with defaults.");
        this->initialize_default_data();
        this->set_need_to_save_preferences(); // Mark for saving
    } else {
        ESP_LOGI(TAG, "Successfully loaded data from flash.");
    }

    if (this->de_pin_ != nullptr) {
        this->de_pin_->setup();
        this->de_pin_->digital_write(false); // Initial state: Receive
        ESP_LOGCONFIG(TAG, "DE Pin configured on GPIO%u. Initial state: OFF (Receive)", this->de_pin_->get_pin());
    } else {
        ESP_LOGCONFIG(TAG, "DE Pin not configured.");
    }
    this->last_heartbeat_time_ = esphome::millis(); // Initialize timers
    this->last_motion_command_time_ = esphome::millis();
}

void BambuBus::dump_config(){
    ESP_LOGCONFIG(TAG, "BambuBus:");
    if (this->de_pin_ != nullptr) {
        ESP_LOGCONFIG(TAG, "  DE Pin: GPIO%u", this->de_pin_->get_pin());
    } else {
        ESP_LOGCONFIG(TAG, "  DE Pin: None");
    }
    // Log other configurations if any
}

void BambuBus::loop() {
    // Read incoming UART data
    while (available()) {
        uint8_t byte;
        if (read_byte(&byte)) {
            process_received_byte(byte);
        }
    }

    // If a complete frame has been received (current_processing_frame_len_ > 0)
    if (this->current_processing_frame_len_ > 0) {
        // Log received data before processing
        ESP_LOGD(TAG, "Processing Frame (%d bytes):\n%s", 
                 this->current_processing_frame_len_, 
                 esphome::format_hex_pretty(this->current_processing_frame_, this->current_processing_frame_len_).c_str());
        
        handle_received_frame(this->current_processing_frame_, this->current_processing_frame_len_);
        this->current_processing_frame_len_ = 0; // Reset for next frame
    }
    
    // Timeout checks (formerly in BambuBus_run)
    uint32_t now = esphome::millis();
    if (now - this->last_heartbeat_time_ > 1000) { // 1 second timeout for heartbeat
        // ESP_LOGW(TAG, "Heartbeat timeout. Bus might be offline.");
        // Consider what action to take on timeout, e.g., set a status sensor
        // The original code set stu = BambuBus_package_ERROR but didn't seem to use it broadly
    }

    if (now - this->last_motion_command_time_ > 1000) { // 1 second timeout for motion
        // ESP_LOGD(TAG, "Motion command timeout. Resetting filament motion states to idle.");
        for (auto &ams_slots : this->persistent_data_.filament) {
            for (auto &slot : ams_slots) {
                slot.motion_set = FILAMENT_MOTION_IDLE;
            }
        }
    }

    if (this->need_to_save_preferences_) {
        this->save_preferences();
        // this->last_heartbeat_time_ = now; // Original logic reset time_set here
    }

    // NFC detect run (original logic was minimal)
    // if (this->nfc_detect_countdown_ > 0) {
    //     this->nfc_detect_countdown_--;
    //     if (this->nfc_detect_countdown_ == 0) {
    //         this->nfc_detected_filament_slot_flags_ = 0;
    //     }
    // }
}

void BambuBus::set_need_to_save_preferences(bool need_save) {
    this->need_to_save_preferences_ = need_save;
}

bool BambuBus::load_preferences() {
    if (!this->preferences_initialized_) {
        ESP_LOGW(TAG, "Preferences not initialized, cannot load.");
        return false;
    }
    FlashSaveData temp_data;
    if (this->pref_.load(&temp_data)) {
        if (temp_data.check == FLASH_DATA_CHECKSUM && temp_data.version == BAMBUBUS_PROTOCOL_VERSION) {
            memcpy(&this->persistent_data_, &temp_data, sizeof(FlashSaveData));
            ESP_LOGI(TAG, "Successfully loaded data from flash.");
            return true;
        } else {
            ESP_LOGW(TAG, "Flash data checksum (0x%08X vs 0x%08X) or version (%u vs %u) mismatch.",
                     temp_data.check, FLASH_DATA_CHECKSUM, temp_data.version, BAMBUBUS_PROTOCOL_VERSION);
        }
    } else {
        ESP_LOGD(TAG, "Failed to load data from flash or no data found.");
    }
    return false;
}

void BambuBus::save_preferences() {
    if (!this->preferences_initialized_) {
        ESP_LOGE(TAG, "Preferences not initialized, cannot save.");
        return;
    }
    this->persistent_data_.version = BAMBUBUS_PROTOCOL_VERSION;
    this->persistent_data_.check = FLASH_DATA_CHECKSUM;

    if (this->pref_.save(&this->persistent_data_)) {
        ESP_LOGI(TAG, "Successfully saved data to flash.");
        this->need_to_save_preferences_ = false;
    } else {
        ESP_LOGE(TAG, "Failed to save data to flash!");
    }
}

void BambuBus::initialize_default_data() {
    // Simplified initialization based on original BambuBus_init
    // Color initializations (example for first AMS)
    // Explicitly create a FilamentData temporary object for assignment
    this->persistent_data_.filament[0][0] = FilamentData{"GFG00", 0xFF, 0x00, 0x00, 0xFF, 220, 240, "PETG", 0.0f, FILAMENT_ONLINE, FILAMENT_MOTION_IDLE, 0};
    this->persistent_data_.filament[0][1] = FilamentData{"GFG00", 0x00, 0xFF, 0x00, 0xFF, 220, 240, "PETG", 0.0f, FILAMENT_ONLINE, FILAMENT_MOTION_IDLE, 0};
    this->persistent_data_.filament[0][2] = FilamentData{"GFG00", 0x00, 0x00, 0xFF, 0xFF, 220, 240, "PETG", 0.0f, FILAMENT_ONLINE, FILAMENT_MOTION_IDLE, 0};
    this->persistent_data_.filament[0][3] = FilamentData{"GFG00", 0x88, 0x88, 0x88, 0xFF, 220, 240, "PETG", 0.0f, FILAMENT_ONLINE, FILAMENT_MOTION_IDLE, 0};
    
    // For all slots:
    for (int i = 0; i < MAX_AMS_COUNT; ++i) {
        for (int j = 0; j < MAX_SLOTS_PER_AMS; ++j) {
            // If not set by specific colors above, apply generic defaults
            if (i == 0 && j < 4) { 
                // Already initialized above, but the general defaults below will re-apply some values.
                // This is fine if the values are the same or intended to be overwritten.
            } else {
                 // Generic default for other AMS units/slots
                 this->persistent_data_.filament[i][j] = FilamentData{"GFG00", 0xCC, 0xCC, 0xCC, 0xFF, 220, 240, "PETG", 0.0f, FILAMENT_ONLINE, FILAMENT_MOTION_IDLE, 0};
            }

            // The following individual assignments will override parts of the FilamentData object
            // assigned above or set them if they were default-initialized.
            // This part of your logic can remain, ensuring specific fields are set as intended
            // after the initial bulk assignment.
            strncpy(this->persistent_data_.filament[i][j].id, "GFG00", sizeof(this->persistent_data_.filament[i][j].id) - 1);
            this->persistent_data_.filament[i][j].id[sizeof(this->persistent_data_.filament[i][j].id) - 1] = '\0';
            
            // If the FilamentData temporary already set these, some of these are redundant,
            // but harmless. For example, color_a, temps, name, meters, status, motion_set are
            // likely already set by the FilamentData{...} line.
            this->persistent_data_.filament[i][j].color_a = 0xFF;
            this->persistent_data_.filament[i][j].temperature_min = 220;
            this->persistent_data_.filament[i][j].temperature_max = 240;
            strncpy(this->persistent_data_.filament[i][j].name, "PETG", sizeof(this->persistent_data_.filament[i][j].name) - 1);
            this->persistent_data_.filament[i][j].name[sizeof(this->persistent_data_.filament[i][j].name) - 1] = '\0';
            this->persistent_data_.filament[i][j].meters = 0.0f; 
            this->persistent_data_.filament[i][j].status = FILAMENT_ONLINE; 
            this->persistent_data_.filament[i][j].motion_set = FILAMENT_MOTION_IDLE;
            // pressure will take its value from the FilamentData{...} assignment
        }
    }
    this->persistent_data_.now_filament_num = 0;
    // Version and check are set during save_preferences()
    ESP_LOGI(TAG, "Initialized with default filament data.");
}

// RX_IRQ logic moved into process_received_byte
void BambuBus::process_received_byte(uint8_t byte) {
    if (this->rx_frame_pos_ == 0) { // Waiting for start byte
        if (byte == 0x3D) {
            this->rx_frame_buffer_[0] = 0x3D;
            this->rx_header_crc_calculator_.restart();
            this->rx_header_crc_calculator_.add(0x3D);
            // Default to long header, adjust if short header detected
            this->rx_data_len_idx_ = 4; 
            this->rx_header_crc_idx_ = 6;
            this->rx_expected_length_ = 6; // Minimum length to read header CRC
            this->rx_frame_pos_ = 1;
        }
        return;
    }

    // Receiving subsequent bytes
    this->rx_frame_buffer_[this->rx_frame_pos_] = byte;

    if (this->rx_frame_pos_ == 1) { // Second byte, determines header type
        if (byte & 0x80) { // Short header
            this->rx_data_len_idx_ = 2;
            this->rx_header_crc_idx_ = 3;
        } else { // Long header (already default)
            this->rx_data_len_idx_ = 4;
            this->rx_header_crc_idx_ = 6;
        }
    }

    if (this->rx_frame_pos_ == this->rx_data_len_idx_) { // Byte that indicates payload length
        this->rx_expected_length_ = byte; // This is the total frame length from the frame itself
    }
    
    if (this->rx_frame_pos_ < this->rx_header_crc_idx_) { // Bytes contributing to header CRC
        this->rx_header_crc_calculator_.add(byte);
    } else if (this->rx_frame_pos_ == this->rx_header_crc_idx_) { // Header CRC byte itself
        if (byte != this->rx_header_crc_calculator_.calc()) {
            ESP_LOGW(TAG, "Header CRC mismatch! Expected 0x%02X, Got 0x%02X. Resetting parser.", this->rx_header_crc_calculator_.calc(), byte);
            // ESP_LOGW(TAG, "Buffer: %s", format_hex_pretty(this->rx_frame_buffer_, this->rx_frame_pos_ + 1).c_str());
            this->rx_frame_pos_ = 0; // Reset parser
            return;
        }
        // If this was a short header, the length byte (at rx_data_len_idx_) is the total frame length.
        // If this was a long header, the length byte (at rx_data_len_idx_) is the total frame length.
        // The logic for rx_expected_length_ seems correct.
    }

    this->rx_frame_pos_++;

    if (this->rx_frame_pos_ >= this->rx_expected_length_) { // Full frame received
        // Copy to processing buffer and signal main loop
        memcpy(this->current_processing_frame_, this->rx_frame_buffer_, this->rx_expected_length_);
        this->current_processing_frame_len_ = this->rx_expected_length_;
        
        this->rx_frame_pos_ = 0; // Reset for next frame
    }

    if (this->rx_frame_pos_ >= sizeof(this->rx_frame_buffer_)) { // Buffer overflow check
        ESP_LOGE(TAG, "RX buffer overflow! Resetting parser.");
        this->rx_frame_pos_ = 0;
    }
}


void BambuBus::send_uart_payload(const uint8_t *data, uint16_t length) {
    if (this->de_pin_ != nullptr) {
        this->de_pin_->digital_write(true);
        esphome::delayMicroseconds(10); // Small delay for DE to settle
    }

    ESP_LOGD(TAG, "Sending UART (%d bytes):\n%s", length, esphome::format_hex_pretty(data, length).c_str());
    this->write_array(data, length);
    this->flush(); // Wait for transmission to complete

    if (this->de_pin_ != nullptr) {
        esphome::delayMicroseconds(10); // Ensure last byte is sent before disabling DE
        this->de_pin_->digital_write(false);
    }
}

bool BambuBus::check_payload_crc16(const uint8_t *data, int data_length) {
    if (data_length < 2) return false; // Need at least 2 bytes for CRC
    this->crc16_calculator_.restart();
    int payload_len = data_length - 2;
    for (int i = 0; i < payload_len; i++) {
        this->crc16_calculator_.add(data[i]);
    }
    uint16_t calculated_crc = this->crc16_calculator_.calc();
    uint16_t received_crc = data[payload_len] | (data[payload_len + 1] << 8);
    
    if (calculated_crc == received_crc) {
        return true;
    }
    ESP_LOGW(TAG, "Payload CRC16 mismatch! Expected 0x%04X, Got 0x%04X", calculated_crc, received_crc);
    return false;
}

void BambuBus::send_frame_with_crc(uint8_t *data, int data_length) {
    // Calculate header CRC8
    this->crc8_calculator_.restart(); // Using the general crc8_calculator_ as the state is reset
    int header_crc_len;
    int header_crc_idx;

    if (data[1] & 0x80) { // Short header
        header_crc_len = 3; // Start byte, cmd, len
        header_crc_idx = 3; // Index of CRC byte
    } else { // Long header
        header_crc_len = 6; // Start, cmd, seq, len_msb, len_lsb, target_addr_msb
        header_crc_idx = 6; // Index of CRC byte (actually target_addr_lsb is also included by original code)
                            // Original code was: for (auto i = 0; i < 6; i++) { crc_8.add(data[i]); } data[6] = crc_8.calc();
                            // This implies data[0] to data[5] are part of CRC calculation, and data[6] is the CRC.
                            // So, the length is 7 bytes for the header part that includes its own CRC.
    }
    for (int i = 0; i < header_crc_idx; i++) { // Iterate up to, but not including, the CRC byte itself
        this->crc8_calculator_.add(data[i]);
    }
    data[header_crc_idx] = this->crc8_calculator_.calc();

    // Calculate payload CRC16
    // data_length is the total length *including* the 2 bytes for CRC16
    if (data_length < 2) {
        ESP_LOGE(TAG, "Data length too short for CRC16 calculation.");
        return;
    }
    int payload_and_header_len = data_length - 2;
    this->crc16_calculator_.restart();
    for (int i = 0; i < payload_and_header_len; i++) {
        this->crc16_calculator_.add(data[i]);
    }
    uint16_t crc16_val = this->crc16_calculator_.calc();
    data[payload_and_header_len] = crc16_val & 0xFF;
    data[payload_and_header_len + 1] = (crc16_val >> 8) & 0xFF;

    this->send_uart_payload(data, data_length);
}


void BambuBus::send_long_package(LongPackageData &package_data) {
    // Use tx_buffer_ as the staging area
    this->tx_buffer_[0] = 0x3D; // Start byte
    this->tx_buffer_[1] = 0x00; // Command for long package (seems to be type marker, not ACK/NAK flag)
    
    // Total length of the long package frame: 
    // 1(start)+1(cmd)+2(pkg_num)+2(total_len_field)+1(hdr_crc8)+2(target)+2(source)+2(type_field) = 13 bytes for "outer" header
    // + payload_length + 2(CRC16)
    // package_data.package_length should be this total length.
    // The "package_length" field inside the LongPackageData struct is the length of the *entire frame* including start, CRCs etc.
    // The original code: data->package_length = data->data_length + 15;
    // This means 13 bytes of "long package header" + data_payload_length + 2 bytes of CRC16.
    package_data.package_length = package_data.data_payload_length + 13 + 2;

    // Copy the "long package header" fields into tx_buffer_
    // Original: memcpy(packge_send_buf + 2, data, 11);
    // This copied: package_number, package_length, crc8 (placeholder), target_address, source_address, type
    // Note: crc8 is calculated later.
    memcpy(this->tx_buffer_ + 2, &package_data.package_number, 2);
    memcpy(this->tx_buffer_ + 4, &package_data.package_length, 2);
    // tx_buffer_[6] is for crc8, calculated by send_frame_with_crc
    memcpy(this->tx_buffer_ + 7, &package_data.target_address, 2);
    memcpy(this->tx_buffer_ + 9, &package_data.source_address, 2);
    memcpy(this->tx_buffer_ + 11, &package_data.type, 2);

    // Copy payload
    if (package_data.data_payload != nullptr && package_data.data_payload_length > 0) {
       if (13 + package_data.data_payload_length > sizeof(this->tx_buffer_) - 2) {
            ESP_LOGE(TAG, "Long package payload too large for tx_buffer_!");
            return;
        }
        memcpy(this->tx_buffer_ + 13, package_data.data_payload, package_data.data_payload_length);
    }
    
    // send_frame_with_crc will calculate header CRC8 and payload CRC16
    this->send_frame_with_crc(this->tx_buffer_, package_data.package_length);
}

void BambuBus::parse_long_package(const uint8_t *buffer, int length, LongPackageData &parsed_data) {
    // Assuming buffer[0] is 0x3D, buffer[1] is 0x00 or 0x05 etc.
    // The long package specific header starts at buffer[2]
    // Original: memcpy(data, buf + 2, 11);
    // This copies: package_number, package_length, crc8, target_address, source_address, type
    if (length < 13 + 2) { // Min long package header (13) + CRC16 (2)
        ESP_LOGW(TAG, "Frame too short to be a valid long package.");
        parsed_data.data_payload_length = 0;
        parsed_data.data_payload = nullptr;
        return;
    }
    memcpy(&parsed_data.package_number, buffer + 2, 2);
    memcpy(&parsed_data.package_length, buffer + 4, 2);
    parsed_data.crc8 = buffer[6]; // This is the received header CRC
    memcpy(&parsed_data.target_address, buffer + 7, 2);
    memcpy(&parsed_data.source_address, buffer + 9, 2);
    memcpy(&parsed_data.type, buffer + 11, 2);

    // Point to payload within the original buffer
    // Payload starts after the 13-byte "long package header"
    parsed_data.data_payload = (uint8_t*)buffer + 13; 
    // Payload length is total frame length - "long package header" (13) - CRC16 (2)
    parsed_data.data_payload_length = length - 13 - 2; 

    if (parsed_data.data_payload_length < 0) { // Should not happen if length check above is fine
        parsed_data.data_payload_length = 0;
    }
}


BambuBusPackageType BambuBus::determine_package_type(const uint8_t *buffer, int length) {
    if (!this->check_payload_crc16(buffer, length)) {
        ESP_LOGW(TAG, "Overall CRC16 check failed for received package.");
        return PACKAGE_NONE; // Or PACKAGE_ERROR
    }

    // Short packages (ACK/NAK style)
    if (buffer[1] & 0x80) { // Check MSB of command byte (original was buffer[1] == 0xC5 etc.)
                            // This is more general. 0xC5 is 11000101.
        // The command type for short packages is in buffer[4]
        // buffer[0]=0x3D, buffer[1]=CMD(seq,ack/nak), buffer[2]=LEN, buffer[3]=HDR_CRC8
        // buffer[4]=SubCMD/Type
        if (length < 5) { // Need at least up to type byte
            ESP_LOGW(TAG, "Short package too short to determine type.");
            return PACKAGE_NONE;
        }
        switch (buffer[4]) {
            case 0x03: return PACKAGE_FILAMENT_MOTION_SHORT;
            case 0x04: return PACKAGE_FILAMENT_MOTION_LONG; // This is a type for short ACKs, not the "long package" format
            case 0x05: return PACKAGE_ONLINE_DETECT;
            case 0x06: return PACKAGE_REQ_X6;
            case 0x07: return PACKAGE_NFC_DETECT;
            case 0x08: return PACKAGE_SET_FILAMENT;
            case 0x20: return PACKAGE_HEARTBEAT;
            default:
                ESP_LOGW(TAG, "Unknown short package type: 0x%02X", buffer[4]);
                return PACKAGE_ETC;
        }
    } 
    // Long packages (data exchange)
    else if (buffer[1] == 0x00 || buffer[1] == 0x05) { // Original check was just buffer[1] == 0x05
                                                       // 0x00 is also seen for requests that expect long responses.
        // Parse the "inner" header of the long package
        // The LongPackageData struct is populated here for use in specific handlers
        this->parse_long_package(buffer, length, this->received_long_package_data_);

        // Store the bus address from the target_address of the received package
        // Assuming this device's address is identified when it's a target.
        // Original logic updated BambuBus_address here.
        if (this->received_long_package_data_.target_address == 0x0700 || // AMS08
            this->received_long_package_data_.target_address == 0x1200) { // AMS Lite
            this->current_bus_address_ = this->received_long_package_data_.target_address;
        }
        // It could also be other addresses like 0x0F00 (Toolhead?)
        
        switch (this->received_long_package_data_.type) {
            case 0x21A: return PACKAGE_LONG_MC_ONLINE;
            case 0x211: return PACKAGE_LONG_FILAMENT_INFO;
            case 0x103: // Fall-through
            case 0x402: return PACKAGE_LONG_VERSION_INFO;
            default:
                ESP_LOGW(TAG, "Unknown long package type: 0x%04X", this->received_long_package_data_.type);
                return PACKAGE_ETC;
        }
    }
    ESP_LOGW(TAG, "Cannot determine package type for frame starting with 0x3D 0x%02X", buffer[1]);
    return PACKAGE_NONE;
}

void BambuBus::handle_received_frame(const uint8_t *frame_buffer, int length) {
    BambuBusPackageType pkg_type = determine_package_type(frame_buffer, length);

    // ESP_LOGD(TAG, "Determined package type: %d", static_cast<int>(pkg_type));

    switch (pkg_type) {
        case PACKAGE_HEARTBEAT:
            ESP_LOGD(TAG, "Processing Heartbeat");
            this->handle_heartbeat();
            break;
        case PACKAGE_FILAMENT_MOTION_SHORT:
            ESP_LOGD(TAG, "Processing Filament Motion Short Request");
            this->handle_filament_motion_short(frame_buffer, length);
            break;
        case PACKAGE_FILAMENT_MOTION_LONG: // This is the ACK for filament motion, not the "long package" structure
            ESP_LOGD(TAG, "Processing Filament Motion Long Request (actually an ACK)");
            this->handle_filament_motion_long(frame_buffer, length);
            break;
        case PACKAGE_ONLINE_DETECT:
            ESP_LOGD(TAG, "Processing Online Detect Request");
            this->handle_online_detect(frame_buffer, length);
            break;
        case PACKAGE_REQ_X6:
            ESP_LOGD(TAG, "Processing REQ_X6 Request");
            this->handle_req_x6(frame_buffer, length);
            break;
        case PACKAGE_NFC_DETECT:
            ESP_LOGD(TAG, "Processing NFC Detect Request");
            // this->handle_nfc_detect(frame_buffer, length); // Original logic commented out actual send
            break;
        case PACKAGE_SET_FILAMENT:
            ESP_LOGI(TAG, "Processing Set Filament Request");
            this->handle_set_filament(frame_buffer, length);
            break;
        case PACKAGE_LONG_MC_ONLINE:
            ESP_LOGD(TAG, "Processing Long Package: MC Online");
            this->handle_long_mc_online(frame_buffer, length);
            break;
        case PACKAGE_LONG_FILAMENT_INFO:
            ESP_LOGD(TAG, "Processing Long Package: Filament Info");
            this->handle_long_filament_info(frame_buffer, length);
            break;
        case PACKAGE_LONG_VERSION_INFO:
            ESP_LOGD(TAG, "Processing Long Package: Version Info");
            this->handle_long_version_info(frame_buffer, length);
            break;
        case PACKAGE_NONE:
            ESP_LOGD(TAG, "No valid package or CRC error.");
            break;
        case PACKAGE_ETC:
            ESP_LOGD(TAG, "Processing ETC (Unknown/Other) package.");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled package type: %d", static_cast<int>(pkg_type));
            break;
    }
}

// --- Specific Packet Handlers ---
// These need to be carefully adapted, ensuring all array indexing and data interpretation
// matches the original logic. tx_buffer_ should be used for constructing responses.

void BambuBus::handle_heartbeat() {
    this->last_heartbeat_time_ = esphome::millis();
    // Usually, a heartbeat request expects an immediate heartbeat reply.
    // The original code didn't show sending a reply for 0x20. If it's just an incoming ping, this is fine.
}

uint8_t BambuBus::get_filament_status_flags_for_ams(uint8_t ams_idx) {
    if (ams_idx >= MAX_AMS_COUNT) return 0;
    uint8_t status_byte = 0;
    for (int i = 0; i < MAX_SLOTS_PER_AMS; ++i) {
        if (this->persistent_data_.filament[ams_idx][i].status != FILAMENT_OFFLINE) { // online or nfc_waiting
            status_byte |= (1 << (2 * i)); // Bit 0 for slot 0, Bit 2 for slot 1, etc.
            if (this->persistent_data_.filament[ams_idx][i].motion_set != FILAMENT_MOTION_IDLE) {
                status_byte |= (2 << (2 * i)); // Bit 1 for slot 0, Bit 3 for slot 1, etc.
            }
        }
    }
    return status_byte;
}


void BambuBus::prepare_motion_response_data(uint8_t *response_payload_start, uint8_t ams_idx, uint8_t current_filament_slot) {
    // response_payload_start points to where the 29-byte motion data block should begin
    // Original Cxx_res[5] / Dxx_res[17]
    
    float meters = 0.0f;
    uint8_t flagx = 0x02; // Default flag from original code

    if (current_filament_slot != 0xFF && ams_idx < MAX_AMS_COUNT && current_filament_slot < MAX_SLOTS_PER_AMS) {
        if (this->current_bus_address_ == 0x0700) { // AMS08
            meters = -this->persistent_data_.filament[ams_idx][current_filament_slot].meters;
        } else if (this->current_bus_address_ == 0x1200) { // AMS lite
            meters = this->persistent_data_.filament[ams_idx][current_filament_slot].meters;
        }
        // If pressure/flagx needs to be dynamic based on slot:
        // flagx = this->persistent_data_.filament[ams_idx][current_filament_slot].pressure related?
    }
    
    response_payload_start[0] = ams_idx; // AMS number
    response_payload_start[1] = 0x00; // Unknown byte, original was part of C_TEST_DATA
    response_payload_start[2] = flagx; // A status/flag byte
    response_payload_start[3] = current_filament_slot; // Current filament slot ID (0-3, or 0xFF if none)
    memcpy(response_payload_start + 4, &meters, sizeof(meters)); // Bytes 4-7: float meters

    // Fill remaining parts of the 29-byte structure based on C_TEST_DATA
    // Bytes 0-7 are set above. Bytes 8-12:
    response_payload_start[8] = 0x00; response_payload_start[9] = 0x00; response_payload_start[10] = 0x00; response_payload_start[11] = 0x00;
    
    // Bytes 12-15 (pressure was at index 12 of C_TEST_DATA, which is index 12 here):
    uint16_t pressure_val = 0x3600; // Default
    if (current_filament_slot != 0xFF && ams_idx < MAX_AMS_COUNT && current_filament_slot < MAX_SLOTS_PER_AMS) {
         pressure_val = this->persistent_data_.filament[ams_idx][current_filament_slot].pressure;
    }
    memcpy(response_payload_start + 12, &pressure_val, 2); // Pressure
    response_payload_start[14] = 0x00; response_payload_start[15] = 0x00;
    
    // Bytes 16-20:
    response_payload_start[16] = 0x00; response_payload_start[17] = 0x00; response_payload_start[18] = 0x00; response_payload_start[19] = 0x00;
    
    // Byte 21 (temperature_related? index 20 of C_TEST_DATA):
    response_payload_start[20] = 0x27; // Fixed value from C_TEST_DATA

    // Byte 22 (unknown, index 21 of C_TEST_DATA):
    response_payload_start[22] = 0x55; // Fixed value from C_TEST_DATA
    
    // Bytes 23-24 (unknown flags/status, FF FF FF FF FF FF FF FF from C_TEST_DATA, but only 2 used here)
    // Original: Cxx_res[27], index 24 of payload, was get_filament_left_char()
    // The C_TEST_DATA has FF FF FF FF FF FF FF FF which is 8 bytes.
    // The get_filament_left_char() returns a single uint8_t.
    // Original set_motion_res_datas has set_buf[24] = get_filament_left_char(AMS_num);
    // So this means byte at index 24 of the payload (which is 29 bytes long)
    // response_payload_start is Cxx_res+5. So Cxx_res[5+24] = Cxx_res[29]
    // Cxx_res size is 30. So Cxx_res[29] is the last byte before CRC.
    // The original Cxx_res has 4 bytes 0x00, 0x00, 0x00, 0x00 after C_TEST_DATA,
    // and C_TEST_DATA is 29 bytes long.
    // This part is confusing. Let's assume the last byte of the 29-byte payload (index 28) is the status flags.
    // The structure of C_TEST_DATA is:
    // 4 unknown, 4 distance, 4 unknown, 4 pressure, 4 unknown, 2 temp_related, 1 unknown, 8 status_colors?
    // Total 29 bytes.
    // Original: set_buf[13] = 0; set_buf[24] = get_filament_left_char(AMS_num);
    // If set_buf is the start of the Cxx_res+5 payload:
    // payload[13] = 0;
    // payload[24] = get_filament_left_char(AMS_num);
    response_payload_start[13] = 0; // As per original set_motion_res_datas
    response_payload_start[24] = get_filament_status_flags_for_ams(ams_idx); // As per original set_motion_res_datas
                                                                            // This overwrites one of the FF bytes from C_TEST_DATA.
}


bool BambuBus::update_filament_motion_state(uint8_t ams_idx, uint8_t filament_slot, 
                                         uint8_t status_flags, uint8_t motion_flag) {
    if (ams_idx >= MAX_AMS_COUNT) return false;

    int combined_slot_idx = ams_idx * MAX_SLOTS_PER_AMS + filament_slot;

    if (this->current_bus_address_ == 0x0700) { // AMS08
        if (filament_slot != 0xFF && filament_slot < MAX_SLOTS_PER_AMS) {
            this->persistent_data_.now_filament_num = combined_slot_idx;
            if ((status_flags == 0x03) && (motion_flag == 0x00)) { // 03 00
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_NEED_SEND_OUT;
                this->persistent_data_.filament[ams_idx][filament_slot].pressure = 0x3600; // Default pressure?
            } else if ((status_flags == 0x09) && (motion_flag == 0xA5)) { // 09 A5
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_ON_USE;
            } else if ((status_flags == 0x07) && (motion_flag == 0x7F)) { // 07 7F
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_ON_USE;
            } else if ((status_flags == 0x07) && (motion_flag == 0x00)) { // 07 00
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_NEED_PULL_BACK;
            }
        } else if (filament_slot == 0xFF) { // Command for all slots in AMS
            if ((status_flags == 0x01) || (status_flags == 0x03)) {
                for (int i = 0; i < MAX_SLOTS_PER_AMS; ++i) {
                    this->persistent_data_.filament[ams_idx][i].motion_set = FILAMENT_MOTION_IDLE;
                    this->persistent_data_.filament[ams_idx][i].pressure = 0x3600;
                }
            }
        }
    } else if (this->current_bus_address_ == 0x1200) { // AMS lite
        if (filament_slot < MAX_SLOTS_PER_AMS) { // Valid slot
             this->persistent_data_.now_filament_num = combined_slot_idx;
            if ((status_flags == 0x03) && (motion_flag == 0x3F)) { // 03 3F
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_NEED_PULL_BACK;
            } else if ((status_flags == 0x03) && (motion_flag == 0xBF)) { // 03 BF
                this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_NEED_SEND_OUT;
            } else { // Other flags might mean transition to IDLE or ON_USE
                if (this->persistent_data_.filament[ams_idx][filament_slot].motion_set == FILAMENT_MOTION_NEED_PULL_BACK)
                    this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_IDLE;
                else if (this->persistent_data_.filament[ams_idx][filament_slot].motion_set == FILAMENT_MOTION_NEED_SEND_OUT)
                    this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_ON_USE;
            }
        } else if (filament_slot == 0xFF) { // Command for all slots
            for (int i = 0; i < MAX_SLOTS_PER_AMS; ++i) {
                this->persistent_data_.filament[ams_idx][i].motion_set = FILAMENT_MOTION_IDLE;
            }
        }
    } else if (this->current_bus_address_ == 0x0000) { // No specific AMS (Hub? Printer direct?)
         if (filament_slot != 0xFF && filament_slot < MAX_SLOTS_PER_AMS) {
            this->persistent_data_.now_filament_num = combined_slot_idx; // Assuming ams_idx 0 for hub
            this->persistent_data_.filament[ams_idx][filament_slot].motion_set = FILAMENT_MOTION_ON_USE;
        }
    } else {
        ESP_LOGW(TAG, "update_filament_motion_state: Unknown bus address 0x%04X", this->current_bus_address_);
        return false;
    }
    this->last_motion_command_time_ = esphome::millis();
    return true;
}


void BambuBus::handle_filament_motion_short(const uint8_t *request_frame, int length) {
    // request_frame[0]=0x3D, [1]=CMD(0xC0-0xFF), [2]=LEN, [3]=HDR_CRC8
    // [4]=SubCMD(0x03), [5]=AMS_Num, [6]=StatusFlags, [7]=FilamentSlot, [8]=MotionFlag
    if (length < 9) {
        ESP_LOGW(TAG, "Filament motion short request too short.");
        return;
    }
    uint8_t ams_idx = request_frame[5];
    uint8_t status_flags = request_frame[6];
    uint8_t filament_slot = request_frame[7]; // 0-3 or 0xFF
    uint8_t motion_flag = request_frame[8];

    if (!update_filament_motion_state(ams_idx, filament_slot, status_flags, motion_flag)) {
        // Optionally send NAK or log error if state update failed for known address
        // Original just returned.
        return;
    }

    // Prepare response using RESPONSE_TEMPLATE_MOTION_SHORT (Cxx_res)
    // Size of RESPONSE_TEMPLATE_MOTION_SHORT is 30 bytes
    memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_MOTION_SHORT, sizeof(RESPONSE_TEMPLATE_MOTION_SHORT));
    
    // Set sequence number and ACK flag (original: 0xC0 | (package_num << 3))
    // The request frame's cmd byte (request_frame[1]) contains the sequence number.
    // Response should typically match sequence number and set ACK.
    // If request_frame[1] = 0xA0 (seq 0, NAK), response could be 0x80 (seq 0, ACK)
    // If request_frame[1] = 0xC5 (seq 0, REQ), response could be 0x05 (seq 0, RESP) -> This is from other protocols.
    // Bambu seems to use seq in bits 3-5, MSB for type (request/response)
    // Let's assume the original logic of incrementing package_num is for the response sequence.
    this->tx_buffer_[1] = 0xC0 | (this->current_package_num_ << 3); // Short ACK type
    // Sub-command in response is same as request (0x03)
    // this->tx_buffer_[4] = 0x03; // Already in template

    prepare_motion_response_data(this->tx_buffer_ + 5, ams_idx, filament_slot);

    send_frame_with_crc(this->tx_buffer_, sizeof(RESPONSE_TEMPLATE_MOTION_SHORT));

    this->current_package_num_ = (this->current_package_num_ + 1) % 8;
}

void BambuBus::handle_filament_motion_long(const uint8_t *request_frame, int length) {
    // This is type 0x04, structure is different from 0x03.
    // request_frame[0]=0x3D, [1]=CMD, [2]=LEN, [3]=HDR_CRC8
    // [4]=SubCMD(0x04), [5]=AMS_Num, [6]=StatusFlags, [7]=MotionFlag, [8]=??, [9]=FilamentSlot
    if (length < 10) {
        ESP_LOGW(TAG, "Filament motion long (type 0x04) request too short.");
        return;
    }
    uint8_t ams_idx = request_frame[5];
    uint8_t status_flags = request_frame[6];
    uint8_t motion_flag = request_frame[7]; 
    // uint8_t unknown_byte = request_frame[8]; // Not used in original set_motion
    uint8_t filament_slot = request_frame[9]; // 0-3 or 0xFF for all

    if (!update_filament_motion_state(ams_idx, filament_slot, status_flags, motion_flag)) {
        return;
    }
    
    // Prepare response using RESPONSE_TEMPLATE_MOTION_LONG (Dxx_res)
    // Size of RESPONSE_TEMPLATE_MOTION_LONG is 46 bytes
    memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_MOTION_LONG, sizeof(RESPONSE_TEMPLATE_MOTION_LONG));

    this->tx_buffer_[1] = 0xC0 | (this->current_package_num_ << 3); // Short ACK type
    // this->tx_buffer_[4] = 0x04; // Sub-command, already in template

    // Populate AMS specific fields in the response (bytes 5-16 of template)
    this->tx_buffer_[5] = ams_idx; // AMS number for this response part
    // tx_buffer_[6,7,8] are flags (0x01, 0x01, 0x01 humidity_wet from template)

    uint8_t filament_present_flags = 0; // Bitmask of present filaments
    uint8_t nfc_present_flags = 0;    // Bitmask of filaments with NFC detected/waiting
    if (ams_idx < MAX_AMS_COUNT) {
        for (int i = 0; i < MAX_SLOTS_PER_AMS; ++i) {
            if (this->persistent_data_.filament[ams_idx][i].status != FILAMENT_OFFLINE) {
                filament_present_flags |= (1 << i);
            }
            if (this->persistent_data_.filament[ams_idx][i].status == FILAMENT_NFC_WAITING) {
                // Original logic: filament_flag_on - filament_flag_NFC. This is strange.
                // Let's assume filament_flag_NFC is just the NFC detected ones.
                nfc_present_flags |= (1 << i);
            }
        }
    }
    this->tx_buffer_[9]  = filament_present_flags; // Overall presence
    this->tx_buffer_[10] = filament_present_flags; // Original: filament_flag_on - filament_flag_NFC
    this->tx_buffer_[11] = filament_present_flags; // Original: filament_flag_on - filament_flag_NFC
    this->tx_buffer_[12] = filament_slot;          // Current target slot for motion
    this->tx_buffer_[13] = nfc_present_flags;      // NFC status flags

    // Bytes 14,15,16 are 0x00 from template.

    // The 29-byte motion data block starts at tx_buffer_[17]
    prepare_motion_response_data(this->tx_buffer_ + 17, ams_idx, filament_slot);

    // Original NFC detect countdown logic
    if (this->nfc_detect_countdown_ > 0) {
        // This logic was in send_for_Dxx and modified Dxx_res[19] and Dxx_res[12], Dxx_res[20]
        // Dxx_res[19] corresponds to tx_buffer_[17+2] = tx_buffer_[19] (flagx)
        // Dxx_res[12] corresponds to tx_buffer_[12] (read_num / filament_slot)
        // Dxx_res[20] corresponds to tx_buffer_[17+3] = tx_buffer_[20] (read_num in payload)
        if (this->nfc_detect_countdown_ > 10) { // Original: last_detect > 10
            this->tx_buffer_[19] = 0x01; // Set some flag in the motion payload
        } else {
            this->tx_buffer_[12] = this->nfc_detected_filament_slot_flags_; // Set target slot to NFC detected
            this->tx_buffer_[19] = 0x01; // Set flag in motion payload
            this->tx_buffer_[20] = this->nfc_detected_filament_slot_flags_; // Set current slot in motion payload
        }
        this->nfc_detect_countdown_--;
        if (this->nfc_detect_countdown_ == 0) this->nfc_detected_filament_slot_flags_ = 0;
    }
    
    send_frame_with_crc(this->tx_buffer_, sizeof(RESPONSE_TEMPLATE_MOTION_LONG));
    this->current_package_num_ = (this->current_package_num_ + 1) % 8;
}


void BambuBus::handle_online_detect(const uint8_t *request_frame, int length) {
    // request_frame[5] = 0x00 for all slots query, or 0x01 for specific slot query
    // request_frame[6] = slot_index if type 0x01
    // request_frame[7] = AMS index (seems to be passed here)

    uint8_t query_type = request_frame[5];
    // uint8_t slot_idx = request_frame[6]; // Only if query_type is 0x01
    // uint8_t ams_idx = request_frame[7];  // This seems to be the AMS index

    // Original F01_res (online_detect_num)
    const uint8_t online_detect_data_slot0[] = {0x0E, 0x7D, 0x32, 0x31, 0x31, 0x38, 0x15, 0x00, 0x36, 0x39, 0x37, 0x33, 0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t online_detect_data_others[] = {0x90, 0x31, 0x33, 0x34, 0x36, 0x35, 0x02, 0x00, 0x37, 0x39, 0x33, 0x38, 0xFF, 0xFF, 0xFF, 0xFF};


    if (query_type == 0x00) { // Query all slots for a given AMS (ams_idx from request_frame[7]?)
        uint8_t ams_idx_for_all_query = request_frame[7]; // Assuming this is the AMS index
        int response_size = 4 * sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE);
        if (response_size > sizeof(this->tx_buffer_)) {
             ESP_LOGE(TAG, "Online detect all slots response too large for tx_buffer_");
             return;
        }
        for (int i = 0; i < 4; ++i) { // For each slot
            memcpy(this->tx_buffer_ + i * sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE), 
                   RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE, 
                   sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE));
            
            uint8_t* current_response_ptr = this->tx_buffer_ + i * sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE);
            current_response_ptr[1] = 0xC0 | (this->current_package_num_ << 3); // Set CMD/SEQ
            current_response_ptr[5] = ams_idx_for_all_query; // AMS index
            current_response_ptr[6] = i;   // Slot index being reported
            current_response_ptr[7] = i;   // "data index" or "report index" (original had F00_res[... + 7] = i)
            
            // Copy the 16-byte "serial number like" data
            if (i == 0) { // Slot 0 has different data in original
                memcpy(current_response_ptr + 8, online_detect_data_slot0, sizeof(online_detect_data_slot0));
            } else {
                memcpy(current_response_ptr + 8, online_detect_data_others, sizeof(online_detect_data_others));
            }
            this->current_package_num_ = (this->current_package_num_ + 1) % 8;
        }
        // CRC is calculated per individual message by send_frame_with_crc.
        // This means we need to send 4 separate messages.
        for (int i=0; i<4; ++i) {
            send_frame_with_crc(this->tx_buffer_ + i * sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE), 
                                sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE));
             // Potentially add small delay between sends if needed by receiver
            if (i < 3) esphome::delayMicroseconds(500);
        }

    } else if (query_type == 0x01) { // Query specific slot
        uint8_t slot_idx_specific = request_frame[6];
        uint8_t ams_idx_specific = request_frame[7];

        if (slot_idx_specific < MAX_SLOTS_PER_AMS) {
            memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE, sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE));
            this->tx_buffer_[1] = 0xC0 | (this->current_package_num_ << 3);
            this->tx_buffer_[5] = ams_idx_specific; // AMS index from request
            this->tx_buffer_[6] = slot_idx_specific; // Slot index from request
            this->tx_buffer_[7] = slot_idx_specific; // "data index" matching slot

            if (slot_idx_specific == 0) {
                memcpy(this->tx_buffer_ + 8, online_detect_data_slot0, sizeof(online_detect_data_slot0));
            } else {
                memcpy(this->tx_buffer_ + 8, online_detect_data_others, sizeof(online_detect_data_others));
            }
            send_frame_with_crc(this->tx_buffer_, sizeof(RESPONSE_TEMPLATE_ONLINE_DETECT_SINGLE));
            this->current_package_num_ = (this->current_package_num_ + 1) % 8;
        } else {
            ESP_LOGW(TAG, "Online detect: Invalid slot index %d for specific query.", slot_idx_specific);
        }
    } else {
        ESP_LOGW(TAG, "Online detect: Unknown query type %d.", query_type);
    }
}


void BambuBus::handle_req_x6(const uint8_t *request_frame, int length) {
    ESP_LOGD(TAG, "REQ_X6 (0x06) received. Original logic was placeholder/commented.");
    // Original logic for REQx6_res was commented out and seemed to reuse Dxx_res2.
    // If a specific response is needed, it should be implemented here.
    // For now, sending a generic ACK or nothing, as per original behavior.
    // uint8_t res_for_06_num = request_frame[7]; // From original commented code
    // The actual response logic was complex and tied to Dxx.
    // If it's just an ACK:
    // memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_REQ_X6_ACK_IF_ANY, ...);
    // send_frame_with_crc(this->tx_buffer_, ...);
}

void BambuBus::handle_nfc_detect(const uint8_t *request_frame, int length) {
    // request_frame[6] is slot_index, request_frame[7] is ams_index (or other id)
    if (length < 8) return;
    uint8_t slot_idx = request_frame[6];
    // uint8_t ams_idx = request_frame[7]; // Or some ID.

    this->nfc_detect_countdown_ = 20; // Original: last_detect = 20
    if (slot_idx < MAX_SLOTS_PER_AMS) { // Assuming slot_idx is 0-3
      this->nfc_detected_filament_slot_flags_ = (1 << slot_idx);
    } else {
      this->nfc_detected_filament_slot_flags_ = 0; // Invalid slot
    }


    memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_NFC_DETECT, sizeof(RESPONSE_TEMPLATE_NFC_DETECT));
    this->tx_buffer_[1] = 0xC0 | (this->current_package_num_ << 3); // Set CMD/SEQ
    this->tx_buffer_[6] = request_frame[6]; // Echo slot_idx
    this->tx_buffer_[7] = request_frame[7]; // Echo ams_idx/id
    
    send_frame_with_crc(this->tx_buffer_, sizeof(RESPONSE_TEMPLATE_NFC_DETECT));
    this->current_package_num_ = (this->current_package_num_ + 1) % 8;
}

void BambuBus::handle_set_filament(const uint8_t *request_frame, int length) {
    // Request frame has filament data
    // Example: [0]=0x3D, [1]=CMD, [2]=LEN, [3]=HDR_CRC8, [4]=0x08 (SubCMD)
    // [5]=ams_slot_combined, [6]=??
    // [7..14]=ID, [15..18]=ColorRGBA, [19..20]=TempMin, [21..22]=TempMax, [23..42]=Name
    if (length < 43) { // Approx size based on original field copies
        ESP_LOGW(TAG, "Set filament request too short.");
        return;
    }

    uint8_t ams_slot_combined = request_frame[5];
    uint8_t ams_idx = (ams_slot_combined >> 4) & 0x0F; // Upper nibble for AMS index
    uint8_t slot_idx = ams_slot_combined & 0x0F;      // Lower nibble for Slot index

    if (ams_idx < MAX_AMS_COUNT && slot_idx < MAX_SLOTS_PER_AMS) {
        FilamentData &target_filament = this->persistent_data_.filament[ams_idx][slot_idx];
        
        memcpy(target_filament.id, request_frame + 7, sizeof(target_filament.id));
        target_filament.id[sizeof(target_filament.id)-1] = '\0'; // Ensure null termination

        target_filament.color_r = request_frame[15];
        target_filament.color_g = request_frame[16];
        target_filament.color_b = request_frame[17];
        target_filament.color_a = request_frame[18];

        memcpy(&target_filament.temperature_min, request_frame + 19, 2);
        memcpy(&target_filament.temperature_max, request_frame + 21, 2);

        memcpy(target_filament.name, request_frame + 23, sizeof(target_filament.name));
        target_filament.name[sizeof(target_filament.name)-1] = '\0'; // Ensure null termination
        
        ESP_LOGI(TAG, "Set Filament for AMS %d Slot %d: ID=%s, Name=%s, Color=(%d,%d,%d,%d)",
                 ams_idx, slot_idx, target_filament.id, target_filament.name,
                 target_filament.color_r, target_filament.color_g, target_filament.color_b, target_filament.color_a);

        this->set_need_to_save_preferences();
        
        // Send ACK
        memcpy(this->tx_buffer_, RESPONSE_TEMPLATE_SET_FILAMENT_ACK, sizeof(RESPONSE_TEMPLATE_SET_FILAMENT_ACK));
        this->tx_buffer_[1] = 0xC0 | (this->current_package_num_ << 3); // Set CMD/SEQ for ACK
        // tx_buffer_[5] is ams_slot_combined echoed. Template might already have a placeholder, or set it.
        // The template has 0x60, which implies AMS 6, slot 0. This needs to be dynamic.
        this->tx_buffer_[5] = ams_slot_combined;

        send_frame_with_crc(this->tx_buffer_, sizeof(RESPONSE_TEMPLATE_SET_FILAMENT_ACK));
        this->current_package_num_ = (this->current_package_num_ + 1) % 8;
    } else {
        ESP_LOGW(TAG, "Set filament: Invalid AMS (%d) or Slot (%d)", ams_idx, slot_idx);
    }
}


// --- Long Package Handlers ---
// These use this->received_long_package_data_ which is populated by determine_package_type

void BambuBus::handle_long_mc_online(const uint8_t *request_frame, int length) {
    // request_frame is the full received frame.
    // this->received_long_package_data_ contains parsed info like source/target addr, type, payload.
    // Payload is at this->received_long_package_data_.data_payload
    // Payload length is this->received_long_package_data_.data_payload_length
    
    if (this->received_long_package_data_.data_payload_length < 1) {
        ESP_LOGW(TAG, "MC Online request payload too short.");
        return;
    }
    uint8_t request_ams_num = this->received_long_package_data_.data_payload[0];

    LongPackageData response_pkg;
    response_pkg.package_number = this->received_long_package_data_.package_number; // Echo package number
    response_pkg.type = this->received_long_package_data_.type;                     // Echo type
    response_pkg.source_address = this->received_long_package_data_.target_address; // Our address (target of request)
    response_pkg.target_address = this->received_long_package_data_.source_address; // Destination (source of request)

    // PAYLOAD_LONG_MC_ONLINE_RESPONSE is {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
    // We need a temporary buffer for the payload if it's modified.
    uint8_t mc_online_payload[sizeof(PAYLOAD_LONG_MC_ONLINE_RESPONSE)];
    memcpy(mc_online_payload, PAYLOAD_LONG_MC_ONLINE_RESPONSE, sizeof(mc_online_payload));
    mc_online_payload[0] = request_ams_num; // Echo AMS num in response payload's first byte

    response_pkg.data_payload = mc_online_payload;
    response_pkg.data_payload_length = sizeof(mc_online_payload);

    send_long_package(response_pkg);
}

void BambuBus::handle_long_filament_info(const uint8_t *request_frame, int length) {
    if (this->received_long_package_data_.data_payload_length < 2) {
        ESP_LOGW(TAG, "Filament Info request payload too short.");
        return;
    }
    uint8_t req_ams_idx = this->received_long_package_data_.data_payload[0];
    uint8_t req_slot_idx = this->received_long_package_data_.data_payload[1];

    if (req_ams_idx >= MAX_AMS_COUNT || req_slot_idx >= MAX_SLOTS_PER_AMS) {
        ESP_LOGW(TAG, "Filament Info request for invalid AMS/Slot: %d/%d", req_ams_idx, req_slot_idx);
        // Optionally send NAK or error response for long packages if protocol supports it
        return;
    }

    LongPackageData response_pkg;
    response_pkg.package_number = this->received_long_package_data_.package_number;
    response_pkg.type = this->received_long_package_data_.type;
    response_pkg.source_address = this->received_long_package_data_.target_address;
    response_pkg.target_address = this->received_long_package_data_.source_address;

    // Use a temporary buffer for the response payload
    uint8_t filament_info_payload[sizeof(PAYLOAD_LONG_FILAMENT_INFO_RESPONSE_TEMPLATE)];
    memcpy(filament_info_payload, PAYLOAD_LONG_FILAMENT_INFO_RESPONSE_TEMPLATE, sizeof(filament_info_payload));

    const FilamentData &f_data = this->persistent_data_.filament[req_ams_idx][req_slot_idx];

    filament_info_payload[0] = req_ams_idx;
    filament_info_payload[1] = req_slot_idx;
    // indices for PAYLOAD_LONG_FILAMENT_INFO_RESPONSE_TEMPLATE:
    // ID: 19 (8 bytes)
    // Name: 27 (20 bytes)
    // Color R: 59, G: 60, B: 61, A: 62
    // TempMax: 79 (2 bytes), TempMin: 81 (2 bytes)
    memcpy(filament_info_payload + 19, f_data.id, sizeof(f_data.id));
    memcpy(filament_info_payload + 27, f_data.name, sizeof(f_data.name));
    filament_info_payload[59] = f_data.color_r;
    filament_info_payload[60] = f_data.color_g;
    filament_info_payload[61] = f_data.color_b;
    filament_info_payload[62] = f_data.color_a;
    memcpy(filament_info_payload + 79, &f_data.temperature_max, 2);
    memcpy(filament_info_payload + 81, &f_data.temperature_min, 2);
    // Other fields in template are defaults or fixed values.

    response_pkg.data_payload = filament_info_payload;
    response_pkg.data_payload_length = sizeof(filament_info_payload);
    
    send_long_package(response_pkg);
}


void BambuBus::handle_long_version_info(const uint8_t *request_frame, int length) {
    uint8_t req_ams_idx = 0; // Default
    uint8_t* response_payload_template_ptr = nullptr;
    size_t response_payload_size = 0;

    // Determine which version response template to use based on target address
    uint8_t temp_payload_buffer[128]; // Max possible size of version payloads

    if (this->received_long_package_data_.target_address == 0x0700) { // AMS08
        response_payload_template_ptr = PAYLOAD_LONG_VERSION_AMS08_RESPONSE;
        response_payload_size = sizeof(PAYLOAD_LONG_VERSION_AMS08_RESPONSE);
    } else if (this->received_long_package_data_.target_address == 0x1200) { // AMS Lite
        response_payload_template_ptr = PAYLOAD_LONG_VERSION_AMS_LITE_RESPONSE;
        response_payload_size = sizeof(PAYLOAD_LONG_VERSION_AMS_LITE_RESPONSE);
    } else {
        ESP_LOGW(TAG, "Version info request for unknown target address: 0x%04X", this->received_long_package_data_.target_address);
        return;
    }
    
    if (response_payload_size > sizeof(temp_payload_buffer)) {
        ESP_LOGE(TAG, "Version response template too large for temp_payload_buffer");
        return;
    }
    memcpy(temp_payload_buffer, response_payload_template_ptr, response_payload_size);


    LongPackageData response_pkg;
    response_pkg.package_number = this->received_long_package_data_.package_number;
    response_pkg.type = this->received_long_package_data_.type; // Echo the type (0x103 or 0x402)
    response_pkg.source_address = this->received_long_package_data_.target_address;
    response_pkg.target_address = this->received_long_package_data_.source_address;

    switch (this->received_long_package_data_.type) {
        case 0x402: // Serial Number request
            if (this->received_long_package_data_.data_payload_length < 34) { // Check min length for AMS num byte
                 ESP_LOGW(TAG, "Version type 0x402 payload too short."); return;
            }
            req_ams_idx = this->received_long_package_data_.data_payload[33]; // AMS num is at offset 33 in request

            // Use PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE template
            memcpy(temp_payload_buffer, PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE, sizeof(PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE));
            response_payload_size = sizeof(PAYLOAD_LONG_VERSION_SERIAL_NUMBER_RESPONSE);
            
            // temp_payload_buffer[0] is length of serial, already set in template.
            // Copy actual serial number data (SERIAL_NUMBER_DATA) into temp_payload_buffer + 1
            memcpy(temp_payload_buffer + 1, SERIAL_NUMBER_DATA, sizeof(SERIAL_NUMBER_DATA)-1); // Exclude null term if char array
            temp_payload_buffer[65] = req_ams_idx; // Set AMS num in response payload
            break;

        case 0x103: // Version and Name request
             if (this->received_long_package_data_.data_payload_length < 1) {
                 ESP_LOGW(TAG, "Version type 0x103 payload too short."); return;
            }
            req_ams_idx = this->received_long_package_data_.data_payload[0]; // AMS num is at offset 0

            // The correct template (AMS08 or AMS_LITE) is already in temp_payload_buffer
            // Set AMS num in response payload (index 20 for both templates)
            temp_payload_buffer[20] = req_ams_idx;
            break;
        default:
            ESP_LOGW(TAG, "Unhandled version info sub-type: 0x%04X", this->received_long_package_data_.type);
            return;
    }

    response_pkg.data_payload = temp_payload_buffer;
    response_pkg.data_payload_length = response_payload_size;

    send_long_package(response_pkg);
}


// --- Getter methods for external use (example) ---
int BambuBus::get_now_filament_num() const {
    return this->persistent_data_.now_filament_num;
}

float BambuBus::get_filament_meters(int ams_idx, int slot_idx) const {
    if (ams_idx >=0 && ams_idx < MAX_AMS_COUNT && slot_idx >=0 && slot_idx < MAX_SLOTS_PER_AMS) {
        return this->persistent_data_.filament[ams_idx][slot_idx].meters;
    }
    return 0.0f;
}

FilamentStatus BambuBus::get_filament_status(int ams_idx, int slot_idx) const {
    if (ams_idx >=0 && ams_idx < MAX_AMS_COUNT && slot_idx >=0 && slot_idx < MAX_SLOTS_PER_AMS) {
        return this->persistent_data_.filament[ams_idx][slot_idx].status;
    }
    return FILAMENT_OFFLINE;
}

void BambuBus::set_filament_online_status(int ams_idx, int slot_idx, bool is_online) {
    if (ams_idx >=0 && ams_idx < MAX_AMS_COUNT && slot_idx >=0 && slot_idx < MAX_SLOTS_PER_AMS) {
        this->persistent_data_.filament[ams_idx][slot_idx].status = is_online ? FILAMENT_ONLINE : FILAMENT_OFFLINE;
        this->set_need_to_save_preferences();
        ESP_LOGD(TAG, "Set AMS %d Slot %d to %s", ams_idx, slot_idx, is_online ? "Online" : "Offline");
    }
}
void BambuBus::reset_filament_meters_action(int ams_idx, int slot_idx) {
     if (ams_idx >=0 && ams_idx < MAX_AMS_COUNT && slot_idx >=0 && slot_idx < MAX_SLOTS_PER_AMS) {
        this->persistent_data_.filament[ams_idx][slot_idx].meters = 0.0f;
        this->set_need_to_save_preferences();
        ESP_LOGD(TAG, "Reset meters for AMS %d Slot %d", ams_idx, slot_idx);
    }
}

// Original global filament accessors - adapt if needed, or use the class methods above
// For example:
// int get_now_filament_num() {
//    // Find the BambuBus component instance if this were global
//    // return bambu_bus_component_instance->get_now_filament_num();
// }