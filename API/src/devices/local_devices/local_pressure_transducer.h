#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the transducer and must outlive it.
//   pressure_input: analog_current_in (4-20 mA) or analog_voltage_in (0-10 V)
//   min_psi / max_psi: pressure at the bottom / top of the signal range
class local_pressure_transducer : public pressure_transducer{
    public:
    local_pressure_transducer(electrical_interface* pressure_input, float min_psi, float max_psi)
    : pressure_input_(pressure_input), min_psi_(min_psi), max_psi_(max_psi){};

    bool readPressure(float& psi) override; // false if the input can't be read or the wire is broken

    private:
    electrical_interface* pressure_input_;
    float min_psi_;
    float max_psi_;
};
