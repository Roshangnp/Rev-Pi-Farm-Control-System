#pragma once
#include <chrono>
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   pulse_input:      pulse_in (DIO input set to counter mode in PiCtory)
//   pulses_per_gallon: K-factor from the paddle wheel datasheet
// Flow is the average over the time since the previous readFlow() call.
class local_flow_sensor : public flow_sensor{
    public:
    local_flow_sensor(electrical_interface* pulse_input, float pulses_per_gallon)
    : pulse_input_(pulse_input), pulses_per_gallon_(pulses_per_gallon){};

    // false on the first call (no previous sample), if the input can't be read,
    // or if the counter was reset since the last call
    bool readFlow(float& gpm) override;

    private:
    electrical_interface* pulse_input_;
    float pulses_per_gallon_;
    bool has_last_sample_ = false;
    uint32_t last_count_ = 0;
    std::chrono::steady_clock::time_point last_time_;
};
