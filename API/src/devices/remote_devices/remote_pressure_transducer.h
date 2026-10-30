#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// A pressure transducer owned by another RevPi. link must outlive the transducer.
class remote_pressure_transducer : public pressure_transducer{
    public:
    remote_pressure_transducer(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readPressure(float& psi) override;

    private:
    remote_link* link_;
    std::string device_name_;
};
