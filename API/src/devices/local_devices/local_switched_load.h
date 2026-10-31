#pragma once
#include "devices.h"
#include "HAL/electronics.h"


// Interface is not owned by the load and must outlive it.
//   on_off_control: relay or digital_out, on = load on
class local_switched_load : public switched_load{
    public:
    local_switched_load(electrical_interface* on_off_control)
    : on_off_control_(on_off_control){};

    bool setOn() override;
    bool setOff() override;
    bool readOn(bool& on) override;

    private:
    electrical_interface* on_off_control_;
};
