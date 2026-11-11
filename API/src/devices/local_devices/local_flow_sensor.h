#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the sensor and must outlive it.
//   pulse_input:      pulse_in (DIO input set to counter mode in PiCtory)
//   pulses_per_gallon: K-factor from the paddle wheel datasheet
//   window:            length of the rolling average
//
// update(), called every main loop pass by the RevPi that owns the sensor,
// samples the counter and recomputes flow as a rolling average over the last
// window (from the newest stored sample at least window old). readFlow() only
// returns that value, so readers (local control code, other machines through
// the device server, the web UI) never affect each other.
class local_flow_sensor : public flow_sensor{
    public:
    local_flow_sensor(electrical_interface* pulse_input, float pulses_per_gallon,
                      std::chrono::milliseconds window = std::chrono::milliseconds(1000))
    : pulse_input_(pulse_input), pulses_per_gallon_(pulses_per_gallon), window_(window){};

    // false until update() has seen a full window (after startup or a counter
    // reset), if the last update() failed to read the input, or if update()
    // hasn't run for 3 windows (main loop stuck or not calling it)
    bool readFlow(float& gpm) override;

    void update() override;

    private:
    using clock_type = std::chrono::steady_clock;
    struct sample {
        clock_type::time_point time;
        uint32_t count;
    };

    electrical_interface* pulse_input_;
    float pulses_per_gallon_;
    std::chrono::milliseconds window_;
    std::deque<sample> samples_;  // oldest first

    bool valid_ = false;          // gpm_ holds a full-window average
    float gpm_ = 0.0f;
    clock_type::time_point last_update_{};
};
