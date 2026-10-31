#include "remote_valve.h"

bool remote_valve::setOpen() {
    return link_->command(device_name_, "setOpen");
}

bool remote_valve::setClosed() {
    return link_->command(device_name_, "setClosed");
}

bool remote_valve::readOpen(bool& open) {
    return link_->queryBool(device_name_, "readOpen", open);
}
