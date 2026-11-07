#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A pump owned by another RevPi. link must outlive the pump.
//   device_name: name the owner registered the pump under in its device_server
class remote_pump : public pump{
    public:
    remote_pump(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool hasSpeedControl() const override; // false if the owner can't be reached
    bool hasAlarm() const override;        // false if the owner can't be reached
    bool hasPressureControl() const override; // false if the owner can't be reached

    bool readAlarm() override; // true if the owner can't be reached
    bool setOn() override;
    bool setOff() override;
    bool setSpeed(float speed) override; // 0-1
    bool readOn(bool& on) override;
    bool readSpeed(float& speed) override;
    bool setPressure(float pressure) override; // in the owner's pressure range
    bool readPressure(float& pressure) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
