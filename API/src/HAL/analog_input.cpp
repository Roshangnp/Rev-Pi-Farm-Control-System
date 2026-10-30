#include "analog_input.h"

namespace {

constexpr float CURRENT_MIN = 4.0f;          // mA
constexpr float CURRENT_MAX = 20.0f;         // mA
constexpr float CURRENT_WIRE_BREAK = 3.5f;   // mA
constexpr float VOLTAGE_MIN = 0.0f;          // V
constexpr float VOLTAGE_MAX = 10.0f;         // V

float scale(float signal, float signal_min, float signal_max, float min_value, float max_value) {
    return min_value + (signal - signal_min) / (signal_max - signal_min) * (max_value - min_value);
}

}  // namespace

bool readScaledAnalog(electrical_interface* input, float min_value, float max_value, float& value) {
    if (input == nullptr) return false;

    switch (input->getType()) {
        case electrical_interface_type::ANALOG_CURRENT_IN: {
            if (!input->getHardwareState()) return false;
            float ma = input->get<analog_current_in>().value;
            if (ma < CURRENT_WIRE_BREAK) return false;
            value = scale(ma, CURRENT_MIN, CURRENT_MAX, min_value, max_value);
            return true;
        }
        case electrical_interface_type::ANALOG_VOLTAGE_IN: {
            if (!input->getHardwareState()) return false;
            float volts = input->get<analog_voltage_in>().value;
            value = scale(volts, VOLTAGE_MIN, VOLTAGE_MAX, min_value, max_value);
            return true;
        }
        default:
            return false;
    }
}
