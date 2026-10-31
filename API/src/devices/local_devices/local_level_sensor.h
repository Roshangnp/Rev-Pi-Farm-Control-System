#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   level_input: analog_current_in (4-20 mA) or analog_voltage_in (0-10 V)
//   min_level / max_level: level at the bottom / top of the signal range
class local_level_sensor : public level_sensor{
    public:
    local_level_sensor(electrical_interface* level_input, float min_level, float max_level)
    : level_input_(level_input), min_level_(min_level), max_level_(max_level){};

    bool readLevel(float& level) override; // false if the input can't be read or the wire is broken

    private:
    electrical_interface* level_input_;
    float min_level_;
    float max_level_;
};
