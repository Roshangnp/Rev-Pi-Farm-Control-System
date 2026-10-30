#include "local_float_switch.h"

bool local_float_switch::readSubmerged(bool& submerged) {
    if (level_input_ == nullptr) return false;
    if (level_input_->getType() != electrical_interface_type::DIGITAL_IN) return false;
    if (!level_input_->getHardwareState()) return false;

    submerged = level_input_->get<digital_in>().value;
    return true;
}
