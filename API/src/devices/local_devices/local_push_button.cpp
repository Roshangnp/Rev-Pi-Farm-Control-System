#include "local_push_button.h"

bool local_push_button::readPushButtonCount(uint32_t& presses) {
    if (press_input_ == nullptr) return false;
    if (press_input_->getType() != electrical_interface_type::PULSE_IN) return false;
    if (!press_input_->getHardwareState()) return false;

    uint32_t count = press_input_->get<pulse_in>().value;

    bool had_count_on_last_read = has_count_on_last_read_;
    uint32_t count_on_last_read = count_on_last_read_;
    has_count_on_last_read_ = true;
    count_on_last_read_ = count;

    if (!had_count_on_last_read) return false;
    if (count < count_on_last_read) return false;

    presses = count - count_on_last_read;
    return true;
}
