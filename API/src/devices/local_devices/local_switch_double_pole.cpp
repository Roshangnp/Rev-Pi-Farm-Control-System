#include "local_switch_double_pole.h"

namespace {

bool readInput(electrical_interface* input, bool& on) {
    if (input == nullptr) return false;
    if (input->getType() != electrical_interface_type::DIGITAL_IN) return false;
    if (!input->getHardwareState()) return false;

    on = input->get<digital_in>().value;
    return true;
}

}  // namespace

bool local_switch_double_pole::readPole1(bool& on) {
    return readInput(pole_1_input_, on);
}

bool local_switch_double_pole::readPole2(bool& on) {
    return readInput(pole_2_input_, on);
}
