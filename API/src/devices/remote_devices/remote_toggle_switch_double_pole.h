#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A double pole toggle switch owned by another RevPi. link must outlive the switch.
class remote_toggle_switch_double_pole : public toggle_switch_double_pole{
    public:
    remote_toggle_switch_double_pole(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readPole1(bool& on) override;
    bool readPole2(bool& on) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
