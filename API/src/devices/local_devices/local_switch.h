#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the switch and must outlive it.
//   switch_input: digital_in, high = switch on
class local_switch : public toggle_switch{
    public:
    local_switch(electrical_interface* switch_input)
    : switch_input_(switch_input){};

    bool readValue(bool& value) override; // false if the input can't be read

    private:
    electrical_interface* switch_input_;
};
