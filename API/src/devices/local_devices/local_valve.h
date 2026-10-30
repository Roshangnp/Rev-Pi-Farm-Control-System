#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the valve and must outlive it.
//   open_control: relay or digital_out, on = open (normally closed valve)
class local_valve : public valve{
    public:
    local_valve(electrical_interface* open_control)
    : open_control_(open_control){};

    bool setOpen() override;
    bool setClosed() override;

    private:
    electrical_interface* open_control_;
};
