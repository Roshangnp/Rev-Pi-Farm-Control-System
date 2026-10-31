#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A temperature sensor owned by another RevPi. link must outlive the sensor.
class remote_temperature_sensor : public temperature_sensor{
    public:
    remote_temperature_sensor(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readTemperature(float& deg_f) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
