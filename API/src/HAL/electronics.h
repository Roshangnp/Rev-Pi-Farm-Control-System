#pragma once
#include <cstdint>
#include <string>
#include <variant>

enum class electrical_interface_type
{
    DIGITAL_IN,
    DIGITAL_OUT,
    ANALOG_CURRENT_IN,
    ANALOG_CURRENT_OUT,
    ANALOG_VOLTAGE_IN,
    ANALOG_VOLTAGE_OUT,
    RELAY,
    PWM_DUTY_OUT,
    PULSE_IN
};

struct digital_in {
    bool value;
};
struct digital_out {
    bool value;
};
struct analog_current_in {
    float value;
};
struct analog_current_out {
    float value;
};
struct analog_voltage_in {
    float value;
};
struct analog_voltage_out {
    float value;
};
struct relay {
    bool value;
};
struct pwm_duty_out {
    float value;
};
struct pulse_in {
    uint32_t value;
};

// Order must match electrical_interface_type
using interface_value = std::variant<digital_in, digital_out,
                                     analog_current_in, analog_current_out,
                                     analog_voltage_in, analog_voltage_out,
                                     relay, pwm_duty_out, pulse_in>;
static_assert(std::variant_size_v<interface_value> ==
              static_cast<int>(electrical_interface_type::PULSE_IN) + 1);

class electrical_interface {
    public:
    // pin_name is the PiCtory variable name, e.g. "I_1", "InputValue_1", "Output_Bit_1"
    electrical_interface(int device_id, interface_value interface, std::string pin_name)
        : interface_(interface), device_id_(device_id), pin_name_(std::move(pin_name)) {}

    int getDeviceId() const {return device_id_;}
    const std::string& getPinName() const {return pin_name_;}
    electrical_interface_type getType() const {return static_cast<electrical_interface_type>(interface_.index());}
    template <typename T> T& get() {return std::get<T>(interface_);}
    template <typename T> const T& get() const {return std::get<T>(interface_);}

    // Looks up pin_name_ in the process image and checks its size matches the interface type.
    bool attach();
    bool isAttached() const {return attached_;}

    // Writes interface_ to the pin. Fails for input types.
    bool setHardwareState();
    // Reads the pin into interface_. Works for inputs and outputs.
    bool getHardwareState();

    protected:
    interface_value interface_;
    int device_id_;
    std::string pin_name_;
    uint16_t address_ = 0;
    uint8_t bit_ = 0;
    bool attached_ = false;
};
