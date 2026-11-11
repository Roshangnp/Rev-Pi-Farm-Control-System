#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A flow sensor owned by another RevPi. link must outlive the sensor.
// The owner keeps a 1 s rolling average (local_flow_sensor::update() in its main
// loop); reads here return it and change nothing. update() does nothing here.
class remote_flow_sensor : public flow_sensor{
    public:
    remote_flow_sensor(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readFlow(float& gpm) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
