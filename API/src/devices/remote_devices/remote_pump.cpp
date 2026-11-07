#include "remote_pump.h"

bool remote_pump::hasSpeedControl() const {
    bool has = false;
    return link_->queryBool(device_name_, "hasSpeedControl", has) && has;
}

bool remote_pump::hasAlarm() const {
    bool has = false;
    return link_->queryBool(device_name_, "hasAlarm", has) && has;
}

bool remote_pump::hasPressureControl() const {
    bool has = false;
    return link_->queryBool(device_name_, "hasPressureControl", has) && has;
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

bool remote_pump::readOn(bool& on) {
    return link_->queryBool(device_name_, "readOn", on);
}

bool remote_pump::readSpeed(float& speed) {
    return link_->queryFloat(device_name_, "readSpeed", speed);
}

bool remote_pump::setPressure(float pressure) {
    return link_->command(device_name_, "setPressure", std::to_string(pressure));
}

bool remote_pump::readPressure(float& pressure) {
    return link_->queryFloat(device_name_, "readPressure", pressure);
}
