#pragma once
#include <string>

#include "devices.h"
#include "wiseconn_link.h"

// One WiseConn irrigation zone (a valve or overhead and its zone).
//   readOn: true while the zone's valve is "On" in the link's hydraulics list
//   scheduleIrrigation(start h, m, end h, m): today, farm local time (24 h);
//       end must be after start, and start can't be in the past.
//       POST /irrigations {zoneId, initTime, endTime, directSchedule: true}
//   scheduleIrrigation(minutes): starting now for that long (1 min to 24 h).
//       POST /irrigations {zoneId, duration}
// Bad arguments are refused without contacting WiseConn. A call blocks until
// WiseConn answers (about 1 s) and returns true only if it accepted the
// irrigation. THESE START REAL IRRIGATIONS on the live system.
class wiseconn_irrigation : public irrigation{
    public:
    // zone_id: [irrigations] in wiseconn_keys.txt. Starts the link's hydraulics polling.
    wiseconn_irrigation(wiseconn_link& link, std::string name, std::string zone_id);

    bool readOn(bool& on) override;
    bool scheduleIrrigation(int startHour, int startMinute, int endHour, int endMinute) override;
    bool scheduleIrrigation(int durationMinutes) override;

    private:
    bool report(bool ok, const std::string& what, const std::string& error) const;

    wiseconn_link& link_;
    std::string name_;     // for log messages
    std::string zone_id_;
};
