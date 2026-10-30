#include "remote_pump.h"

bool remote_pump::hasSpeedControl() const {
    bool has = false;
    return link_->queryBool(device_name_, "hasSpeedControl", has) && has;
}

bool remote_pump::hasAlarm() const {
    bool has = false;
    return link_->queryBool(device_name_, "hasAlarm", has) && has;
}

bool remote_pump::readAlarm() {
    bool alarm = true;
    if (!link_->queryBool(device_name_, "readAlarm", alarm)) return true;
    return alarm;
}

bool remote_pump::setOn() {
    return link_->command(device_name_, "setOn");
}

bool remote_pump::setOff() {
    return link_->command(device_name_, "setOff");
}

bool remote_pump::setSpeed(float speed) {
    return link_->command(device_name_, "setSpeed", std::to_string(speed));
}
