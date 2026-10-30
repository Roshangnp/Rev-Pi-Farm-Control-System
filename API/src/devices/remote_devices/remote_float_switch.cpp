#include "remote_float_switch.h"

bool remote_float_switch::readSubmerged(bool& submerged) {
    return link_->queryBool(device_name_, "readSubmerged", submerged);
}
