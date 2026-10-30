#include "local_flow_sensor.h"

bool local_flow_sensor::readFlow(float& gpm) {
    if (pulse_input_ == nullptr) return false;
    if (pulse_input_->getType() != electrical_interface_type::PULSE_IN) return false;
    if (pulses_per_gallon_ <= 0.0f) return false;
    if (!pulse_input_->getHardwareState()) return false;

    uint32_t count = pulse_input_->get<pulse_in>().value;
    auto now = std::chrono::steady_clock::now();

    bool had_last_sample = has_last_sample_;
    uint32_t last_count = last_count_;
    auto last_time = last_time_;
    has_last_sample_ = true;
    last_count_ = count;
    last_time_ = now;

    if (!had_last_sample) return false;
    if (count < last_count) return false;

    float minutes = std::chrono::duration<float, std::ratio<60>>(now - last_time).count();
    if (minutes <= 0.0f) return false;

    gpm = static_cast<float>(count - last_count) / pulses_per_gallon_ / minutes;
    return true;
}
