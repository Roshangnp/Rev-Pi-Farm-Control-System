#pragma once
#include <string>
#include <vector>
#include "devices.h"
#include "remote_link.h"


// A soil probe served by another machine's device_server (one value per depth,
// sent as "a,b,c"). link must outlive the sensor.
class remote_soil_sensor : public soil_sensor{
    public:
    remote_soil_sensor(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readMoisture(std::vector<float>& moisture) override;
    bool readETC(std::vector<float>& etc) override;
    bool readTemperature(std::vector<float>& temperature) override;

    private:
    bool readDepths(const char* method, std::vector<float>& values);

    remote_link* link_;
    std::string device_name_;
};
