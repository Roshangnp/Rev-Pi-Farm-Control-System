#include "remote_switched_load.h"

bool remote_switched_load::setOn() {
    return link_->command(device_name_, "setOn");
}

bool remote_switched_load::setOff() {
    return link_->command(device_name_, "setOff");
}

bool remote_switched_load::readOn(bool& on) {
    return link_->queryBool(device_name_, "readOn", on);
}
