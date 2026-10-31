#include "remote_level_sensor.h"

bool remote_level_sensor::readLevel(float& level) {
    return link_->queryFloat(device_name_, "readLevel", level);
}
