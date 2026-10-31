#include "remote_conductivity_sensor.h"

bool remote_conductivity_sensor::readConductivity(float& us_per_cm) {
    return link_->queryFloat(device_name_, "readConductivity", us_per_cm);
}
