#include "local_switch.h"

bool local_switch::readValue(bool& value) {
    if (switch_input_ == nullptr) return false;
    if (switch_input_->getType() != electrical_interface_type::DIGITAL_IN) return false;
    if (!switch_input_->getHardwareState()) return false;

    value = switch_input_->get<digital_in>().value;
    return true;
}
