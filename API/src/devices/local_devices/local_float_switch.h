#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the float and must outlive it.
//   level_input: digital_in, high = float submerged
class local_float_switch : public float_switch{
    public:
    local_float_switch(electrical_interface* level_input)
    : level_input_(level_input){};

    bool readSubmerged(bool& submerged) override; // false if the input can't be read

    private:
    electrical_interface* level_input_;
};
