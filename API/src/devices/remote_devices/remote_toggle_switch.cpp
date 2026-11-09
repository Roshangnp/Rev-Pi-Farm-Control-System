#include "remote_toggle_switch.h"

bool remote_toggle_switch::readValue(bool& value) {
    return link_->queryBool(device_name_, "readValue", value);
}
