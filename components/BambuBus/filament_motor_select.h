#pragma once

#include "esphome/components/select/select.h"
#include "BambuBus.h" // For enum and MyFilamentController class

namespace esphome {

class FilamentMotorSelect : public esphome::select::Select, public esphome::Component {
 public:
  explicit FilamentMotorSelect(BambuBus *parent) : parent_(parent) {}

  void setup() override {
    this->traits.set_options({
        "MOTOR_1",
        "MOTOR_2",
        "MOTOR_3",
        "MOTOR_4"
    });
    // this->publish_state_from_parent(parent_->get_current_motor_index());
  }

  void control(const std::string &value) override {
    ESP_LOGD("filament_motor_select", "Control called with: %s", value.c_str());
    FilamentMotionMotorIndex new_index_enum;

    if (value == "MOTOR_1") new_index_enum = FilamentMotionMotorIndex::MOTOR_1;
    else if (value == "MOTOR_2") new_index_enum = FilamentMotionMotorIndex::MOTOR_2;
    else if (value == "MOTOR_3") new_index_enum = FilamentMotionMotorIndex::MOTOR_3;
    else if (value == "MOTOR_4") new_index_enum = FilamentMotionMotorIndex::MOTOR_4;
    else {
      ESP_LOGW("filament_motor_select", "Received unknown motor index: %s", value.c_str());
      return;
    }
    
    this->parent_->set_current_motor_index(new_index_enum);
  }

  void publish_state_from_parent(FilamentMotionMotorIndex current_app_idx) {
    std::string state_str;
    switch (current_app_idx) {
      case FilamentMotionMotorIndex::MOTOR_1: state_str = "MOTOR_1"; break;
      case FilamentMotionMotorIndex::MOTOR_2: state_str = "MOTOR_2"; break;
      case FilamentMotionMotorIndex::MOTOR_3: state_str = "MOTOR_3"; break;
      case FilamentMotionMotorIndex::MOTOR_4: state_str = "MOTOR_4"; break;
      default: ESP_LOGW("filament_motor_select", "Unknown internal motor index: %d", (int)current_app_idx); return;
    }
    if (this->state != state_str) {
        this->publish_state(state_str);
    }
  }

 protected:
  BambuBus *parent_;
};

}