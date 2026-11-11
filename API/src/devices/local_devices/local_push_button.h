#pragma once
#include <cstdint>
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the button and must outlive it.
//   press_input: pulse_in (DIO input set to counter mode in PiCtory), one count per press
// readPressTotal() is the raw counter, so any number of readers (local code,
// remote machines through the device server) can count presses without taking
// them from each other.
class local_push_button : public push_button{
    public:
    local_push_button(electrical_interface* press_input)
    : press_input_(press_input){};

    // false if the input can't be read or isn't a pulse_in
    bool readPressTotal(uint32_t& total) override;

    private:
    electrical_interface* press_input_;
};
