#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A conductivity sensor owned by another RevPi. link must outlive the sensor.
class remote_conductivity_sensor : public conductivity_sensor{
    public:
    remote_conductivity_sensor(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readConductivity(float& us_per_cm) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
