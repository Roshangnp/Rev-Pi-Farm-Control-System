#pragma once
#include <cstdint>
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the button and must outlive it.
//   press_input: pulse_in (DIO input set to counter mode in PiCtory), one count per press
// Every reader shares the same count: a read returns the presses since the
// previous read by anyone (local or remote).
class local_push_button : public push_button{
    public:
    local_push_button(electrical_interface* press_input)
    : press_input_(press_input){};

    // false on the first call (no previous count), if the input can't be read,
    // or if the counter was reset since the last call
    bool readPushButtonCount(uint32_t& presses) override;

    private:
    electrical_interface* press_input_;
    bool has_count_on_last_read_ = false;
    uint32_t count_on_last_read_ = 0;
};
