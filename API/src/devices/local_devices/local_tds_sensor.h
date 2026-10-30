#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   tds_input: analog_current_in (4-20 mA) or analog_voltage_in (0-10 V)
//   min_ppm / max_ppm: TDS at the bottom / top of the signal range
class local_tds_sensor : public tds_sensor{
    public:
    local_tds_sensor(electrical_interface* tds_input, float min_ppm, float max_ppm)
    : tds_input_(tds_input), min_ppm_(min_ppm), max_ppm_(max_ppm){};

    bool readTds(float& ppm) override; // false if the input can't be read or the wire is broken

    private:
    electrical_interface* tds_input_;
    float min_ppm_;
    float max_ppm_;
};
