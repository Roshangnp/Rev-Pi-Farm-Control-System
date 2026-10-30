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
