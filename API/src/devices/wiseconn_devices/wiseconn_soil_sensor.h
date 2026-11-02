#pragma once
#include <string>
#include <vector>

#include "devices.h"
#include "wiseconn_keys.h"
#include "wiseconn_link.h"

// Soil probe read from the WiseConn cloud: one value per depth, shallowest
// first (4 in, 8 in, 12 in on this farm), from the link's cache.
//   readMoisture     soil moisture, %
//   readETC          EC / salinity, WiseConn's raw "VIC" units (not uS/cm)
//   readTemperature  soil temperature, deg F
// A read fails (and leaves the vector unchanged) unless every depth has a value
// no older than the link's max age. Read only.
class wiseconn_soil_sensor : public soil_sensor{
    public:
    // ids: the [name] section of wiseconn_keys.txt. Registers its zone with the link.
    wiseconn_soil_sensor(wiseconn_link& link, const wiseconn_soil_ids& ids);

    bool readMoisture(std::vector<float>& moisture) override;
    bool readETC(std::vector<float>& etc) override;
    bool readTemperature(std::vector<float>& temperature) override;

    private:
    bool readAll(const std::vector<std::string>& measure_ids, std::vector<float>& values) const;

    wiseconn_link& link_;
    wiseconn_soil_ids ids_;
};
