#include "local_pump.h"

namespace {

constexpr float SPEED_VOLTAGE_MIN = 0.0f;   // V
constexpr float SPEED_VOLTAGE_MAX = 10.0f;  // V
constexpr float SPEED_CURRENT_MIN = 4.0f;   // mA
constexpr float SPEED_CURRENT_MAX = 20.0f;  // mA
constexpr float SPEED_DUTY_MAX = 100.0f;    // %

bool setRunOutput(electrical_interface* output, bool on) {
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

bool local_pump::readAlarm() {
    if (alarm_ == nullptr) return false;
    if (alarm_->getType() != electrical_interface_type::DIGITAL_IN) return true;
    if (!alarm_->getHardwareState()) return true;
    return alarm_->get<digital_in>().value;
}

bool local_pump::setOn() {
    return setRunOutput(on_off_control_, true);
}

bool local_pump::setOff() {
    return setRunOutput(on_off_control_, false);
}

bool local_pump::setSpeed(float speed) {
    if (electrical_interface_speed_control_ == nullptr) return false;
    if (!(speed >= 0.0f && speed <= 1.0f)) return false;

    electrical_interface& output = *electrical_interface_speed_control_;
    switch (output.getType()) {
        case electrical_interface_type::ANALOG_VOLTAGE_OUT:
            output.get<analog_voltage_out>().value = SPEED_VOLTAGE_MIN + speed * (SPEED_VOLTAGE_MAX - SPEED_VOLTAGE_MIN);
            break;
        case electrical_interface_type::ANALOG_CURRENT_OUT:
            output.get<analog_current_out>().value = SPEED_CURRENT_MIN + speed * (SPEED_CURRENT_MAX - SPEED_CURRENT_MIN);
            break;
        case electrical_interface_type::PWM_DUTY_OUT:
            output.get<pwm_duty_out>().value = speed * SPEED_DUTY_MAX;
            break;
        default:
            return false;
    }
    return output.setHardwareState();
}
