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

bool readRunOutput(electrical_interface* output, bool& on) {
    if (output == nullptr || !output->getHardwareState()) return false;

    switch (output->getType()) {
        case electrical_interface_type::RELAY:
            on = output->get<relay>().value;
            return true;
        case electrical_interface_type::DIGITAL_OUT:
            on = output->get<digital_out>().value;
            return true;
        default:
            return false;
    }
}

// Analog/PWM outputs (speed, pressure setpoint): fraction 0-1 maps to
// 0-10 V, 4-20 mA or 0-100 % duty.
bool writeFraction(electrical_interface* output, float fraction) {
    if (output == nullptr) return false;

    switch (output->getType()) {
        case electrical_interface_type::ANALOG_VOLTAGE_OUT:
            output->get<analog_voltage_out>().value = SPEED_VOLTAGE_MIN + fraction * (SPEED_VOLTAGE_MAX - SPEED_VOLTAGE_MIN);
            break;
        case electrical_interface_type::ANALOG_CURRENT_OUT:
            output->get<analog_current_out>().value = SPEED_CURRENT_MIN + fraction * (SPEED_CURRENT_MAX - SPEED_CURRENT_MIN);
            break;
        case electrical_interface_type::PWM_DUTY_OUT:
            output->get<pwm_duty_out>().value = fraction * SPEED_DUTY_MAX;
            break;
        default:
            return false;
    }
    return output->setHardwareState();
}

bool readFraction(electrical_interface* output, float& fraction) {
    if (output == nullptr || !output->getHardwareState()) return false;

    switch (output->getType()) {
        case electrical_interface_type::ANALOG_VOLTAGE_OUT:
            fraction = (output->get<analog_voltage_out>().value - SPEED_VOLTAGE_MIN) / (SPEED_VOLTAGE_MAX - SPEED_VOLTAGE_MIN);
            return true;
        case electrical_interface_type::ANALOG_CURRENT_OUT:
            fraction = (output->get<analog_current_out>().value - SPEED_CURRENT_MIN) / (SPEED_CURRENT_MAX - SPEED_CURRENT_MIN);
            return true;
        case electrical_interface_type::PWM_DUTY_OUT:
            fraction = output->get<pwm_duty_out>().value / SPEED_DUTY_MAX;
            return true;
        default:
            return false;
    }
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
    if (!(speed >= 0.0f && speed <= 1.0f)) return false;
    return writeFraction(electrical_interface_speed_control_, speed);
}

bool local_pump::readOn(bool& on) {
    return readRunOutput(on_off_control_, on);
}

bool local_pump::readSpeed(float& speed) {
    return readFraction(electrical_interface_speed_control_, speed);
}

bool local_pump::setPressure(float pressure) {
    if (!(pressure_max_ > pressure_min_)) return false;
    if (!(pressure >= pressure_min_ && pressure <= pressure_max_)) return false;
    return writeFraction(pressure_control_, (pressure - pressure_min_) / (pressure_max_ - pressure_min_));
}

bool local_pump::readPressure(float& pressure) {
    float fraction = 0.0f;
    if (!readFraction(pressure_control_, fraction)) return false;
    pressure = pressure_min_ + fraction * (pressure_max_ - pressure_min_);
    return true;
}
