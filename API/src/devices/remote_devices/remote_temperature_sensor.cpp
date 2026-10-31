#include "remote_temperature_sensor.h"

bool remote_temperature_sensor::readTemperature(float& deg_f) {
    return link_->queryFloat(device_name_, "readTemperature", deg_f);
}
