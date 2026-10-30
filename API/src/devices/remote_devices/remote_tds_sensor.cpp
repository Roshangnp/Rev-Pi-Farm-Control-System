#include "remote_tds_sensor.h"

bool remote_tds_sensor::readTds(float& ppm) {
    return link_->queryFloat(device_name_, "readTds", ppm);
}
