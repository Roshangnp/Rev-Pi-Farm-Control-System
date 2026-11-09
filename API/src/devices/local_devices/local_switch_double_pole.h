#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interfaces are not owned by the switch and must outlive it.
//   pole_1_input: digital_in, high = pole 1 on
//   pole_2_input: digital_in, high = pole 2 on
class local_switch_double_pole : public toggle_switch_double_pole{
    public:
    local_switch_double_pole(electrical_interface* pole_1_input, electrical_interface* pole_2_input)
    : pole_1_input_(pole_1_input), pole_2_input_(pole_2_input){};

    bool readPole1(bool& on) override; // false if the input can't be read
    bool readPole2(bool& on) override; // false if the input can't be read

    private:
    electrical_interface* pole_1_input_;
    electrical_interface* pole_2_input_;
};
