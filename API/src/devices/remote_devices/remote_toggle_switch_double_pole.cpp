#include "remote_toggle_switch_double_pole.h"

bool remote_toggle_switch_double_pole::readPole1(bool& on) {
    return link_->queryBool(device_name_, "readPole1", on);
}

bool remote_toggle_switch_double_pole::readPole2(bool& on) {
    return link_->queryBool(device_name_, "readPole2", on);
}
