#include "remote_flow_sensor.h"

bool remote_flow_sensor::readFlow(float& gpm) {
    return link_->queryFloat(device_name_, "readFlow", gpm);
}
