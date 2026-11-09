#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A toggle switch owned by another RevPi. link must outlive the switch.
class remote_toggle_switch : public toggle_switch{
    public:
    remote_toggle_switch(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readValue(bool& value) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
