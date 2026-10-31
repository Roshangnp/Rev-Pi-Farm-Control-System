#include "local_valve.h"

namespace {

bool setOpenOutput(electrical_interface* output, bool open) {
    if (output == nullptr) return false;

    switch (output->getType()) {
        case electrical_interface_type::RELAY:
            output->get<relay>().value = open;
            break;
        case electrical_interface_type::DIGITAL_OUT:
            output->get<digital_out>().value = open;
            break;
        default:
            return false;
    }
    return output->setHardwareState();
}

}  // namespace

bool local_valve::setOpen() {
    return setOpenOutput(open_control_, true);
}

bool local_valve::setClosed() {
    return setOpenOutput(open_control_, false);
}

bool local_valve::readOpen(bool& open) {
    if (open_control_ == nullptr || !open_control_->getHardwareState()) return false;

    switch (open_control_->getType()) {
        case electrical_interface_type::RELAY:
            open = open_control_->get<relay>().value;
            return true;
        case electrical_interface_type::DIGITAL_OUT:
            open = open_control_->get<digital_out>().value;
            return true;
        default:
            return false;
    }
}
