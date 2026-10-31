#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   temperature_input: analog_current_in (4-20 mA) or analog_voltage_in (0-10 V)
//   min_deg_f / max_deg_f: temperature at the bottom / top of the signal range
class local_temperature_sensor : public temperature_sensor{
    public:
    local_temperature_sensor(electrical_interface* temperature_input, float min_deg_f, float max_deg_f)
    : temperature_input_(temperature_input), min_deg_f_(min_deg_f), max_deg_f_(max_deg_f){};

    bool readTemperature(float& deg_f) override; // false if the input can't be read or the wire is broken

    private:
    electrical_interface* temperature_input_;
    float min_deg_f_;
    float max_deg_f_;
};
