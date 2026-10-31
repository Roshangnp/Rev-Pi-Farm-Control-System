#include "local_switched_load.h"

namespace {

bool setOnOutput(electrical_interface* output, bool on) {
    if (output == nullptr) return false;

    switch (output->getType()) {
        case electrical_interface_type::RELAY:
            output->get<relay>().value = on;
            break;
        case electrical_interface_type::DIGITAL_OUT:
            output->get<digital_out>().value = on;
            break;
        default:
            return false;
    }
    return output->setHardwareState();
}

}  // namespace

bool local_switched_load::setOn() {
    return setOnOutput(on_off_control_, true);
}

bool local_switched_load::setOff() {
    return setOnOutput(on_off_control_, false);
}

bool local_switched_load::readOn(bool& on) {
    if (on_off_control_ == nullptr || !on_off_control_->getHardwareState()) return false;

    switch (on_off_control_->getType()) {
        case electrical_interface_type::RELAY:
            on = on_off_control_->get<relay>().value;
            return true;
        case electrical_interface_type::DIGITAL_OUT:
            on = on_off_control_->get<digital_out>().value;
            return true;
        default:
            return false;
    }
}
