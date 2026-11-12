#include "wiseconn_flow_sensor.h"

wiseconn_flow_sensor::wiseconn_flow_sensor(wiseconn_link& link, const wiseconn_flow_ids& ids)
    : link_(link), ids_(ids) {
    if (!ids_.zone_id.empty()) link_.addZone(ids_.zone_id);
}

bool wiseconn_flow_sensor::readFlow(float& gpm) {
    return !ids_.measure_id.empty() && link_.readMeasure(ids_.measure_id, gpm);
}
