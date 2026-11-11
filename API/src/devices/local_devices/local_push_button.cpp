#include "local_push_button.h"

bool local_push_button::readPressTotal(uint32_t& total) {
    if (press_input_ == nullptr) return false;
    if (press_input_->getType() != electrical_interface_type::PULSE_IN) return false;
    if (!press_input_->getHardwareState()) return false;

    total = press_input_->get<pulse_in>().value;
    return true;
}
