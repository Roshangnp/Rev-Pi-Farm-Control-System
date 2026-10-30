#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A float switch owned by another RevPi. link must outlive the float.
class remote_float_switch : public float_switch{
    public:
    remote_float_switch(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readSubmerged(bool& submerged) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
