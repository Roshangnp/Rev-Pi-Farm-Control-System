#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A level sensor owned by another RevPi. link must outlive the sensor.
class remote_level_sensor : public level_sensor{
    public:
    remote_level_sensor(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readLevel(float& level) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
