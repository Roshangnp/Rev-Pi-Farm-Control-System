#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A valve owned by another RevPi. link must outlive the valve.
class remote_valve : public valve{
    public:
    remote_valve(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool setOpen() override;
    bool setClosed() override;
    bool readOpen(bool& open) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
