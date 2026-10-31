#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A switched load owned by another RevPi. link must outlive the load.
class remote_switched_load : public switched_load{
    public:
    remote_switched_load(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool setOn() override;
    bool setOff() override;
    bool readOn(bool& on) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
