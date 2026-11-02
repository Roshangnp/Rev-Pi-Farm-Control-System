#include "wiseconn_soil_sensor.h"

wiseconn_soil_sensor::wiseconn_soil_sensor(wiseconn_link& link, const wiseconn_soil_ids& ids)
    : link_(link), ids_(ids) {
    if (!ids_.zone_id.empty()) link_.addZone(ids_.zone_id);
}

bool wiseconn_soil_sensor::readMoisture(std::vector<float>& moisture) {
    return readAll(ids_.moisture, moisture);
}

bool wiseconn_soil_sensor::readETC(std::vector<float>& etc) {
    return readAll(ids_.etc, etc);
}

bool wiseconn_soil_sensor::readTemperature(std::vector<float>& temperature) {
    return readAll(ids_.temperature, temperature);
}

bool wiseconn_soil_sensor::readAll(const std::vector<std::string>& measure_ids, std::vector<float>& values) const {
    if (measure_ids.empty()) return false;
    std::vector<float> read(measure_ids.size());
    for (size_t i = 0; i < measure_ids.size(); ++i) {
        if (!link_.readMeasure(measure_ids[i], read[i])) return false;
    }
    values = std::move(read);
    return true;
}
