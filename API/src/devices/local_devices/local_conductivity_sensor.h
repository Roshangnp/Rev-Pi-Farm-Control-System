#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   conductivity_input: analog_current_in (4-20 mA) or analog_voltage_in (0-10 V)
//   min_us_per_cm / max_us_per_cm: conductivity at the bottom / top of the signal range
class local_conductivity_sensor : public conductivity_sensor{
    public:
    local_conductivity_sensor(electrical_interface* conductivity_input, float min_us_per_cm, float max_us_per_cm)
    : conductivity_input_(conductivity_input), min_us_per_cm_(min_us_per_cm), max_us_per_cm_(max_us_per_cm){};

    bool readConductivity(float& us_per_cm) override; // false if the input can't be read or the wire is broken

    private:
    electrical_interface* conductivity_input_;
    float min_us_per_cm_;
    float max_us_per_cm_;
};
