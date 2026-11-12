#pragma once
#include <string>
#include "devices.h"
#include "remote_link.h"


// An irrigation zone served by another machine's device_server. link must
// outlive the zone. scheduleIrrigation goes to the owner's device, so on a
// WiseConn zone it starts a real irrigation, like calling it locally.
class remote_irrigation : public irrigation{
    public:
    remote_irrigation(remote_link& link, std::string device_name)
    : link_(&link), device_name_(std::move(device_name)){};

    bool readOn(bool& on) override;
    // Sent as "HH:MM-HH:MM"
    bool scheduleIrrigation(int startHour, int startMinute, int endHour, int endMinute) override;
    // Sent as the number of minutes
    bool scheduleIrrigation(int durationMinutes) override;
    bool readScheduledIrrigation(int& id) override;
    bool cancelIrrigation() override;

    private:
    remote_link* link_;
    std::string device_name_;
};
