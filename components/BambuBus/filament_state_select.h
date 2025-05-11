#pragma once

#include "esphome/components/select/select.h"
#include "BambuBus.h" // For enum and MyFilamentController class

namespace esphome {

class FilamentStateSelect : public esphome::select::Select, public esphome::Component {
 public:
  explicit FilamentStateSelect(BambuBus *parent) : parent_(parent) {}

  void setup() override {
    ESP_LOGI("FilamentStateSelect setup");
    this->traits.set_options({
        "need_pull_back",
        "need_send_out",
        "on_use",
        "idle"
    });
    // Initial state will be published by parent after it's fully set up
    // or if you want to read it directly:
    // this->publish_state_from_parent(parent_->get_current_motion_state());
  }

  void control(const std::string &value) override {
    ESP_LOGD("filament_state_select", "Control called with: %s", value.c_str());
    _filament_motion_state_set new_state_enum;

    if (value == "need_pull_back") new_state_enum = need_pull_back;
    else if (value == "need_send_out") new_state_enum = need_send_out;
    else if (value == "on_use") new_state_enum = on_use;
    else if (value == "idle") new_state_enum = idle;
    else {
      ESP_LOGW("filament_state_select", "Received unknown state: %s", value.c_str());
      return;
    }
    
    this->parent_->set_current_motion_state(new_state_enum);
    // The parent will call publish_state_from_parent which eventually calls this->publish_state(value)
  }

  // Called by parent component to update HA
  void publish_state_from_parent(_filament_motion_state_set current_app_state) {
    std::string state_str;
    switch (current_app_state) {
      case need_pull_back: state_str = "need_pull_back"; break;
      case need_send_out: state_str = "need_send_out"; break;
      case on_use: state_str = "on_use"; break;
      case idle: state_str = "idle"; break;
      default: ESP_LOGW("filament_state_select", "Unknown internal state: %d", (int)current_app_state); return;
    }
    if (this->state != state_str) {
        this->publish_state(state_str);
    }
  }

 protected:
  BambuBus *parent_;
};

}
