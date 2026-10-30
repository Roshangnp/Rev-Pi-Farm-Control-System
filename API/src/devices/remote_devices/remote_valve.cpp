#include "remote_valve.h"

bool remote_valve::setOpen() {
    return link_->command(device_name_, "setOpen");
}

bool remote_valve::setClosed() {
    return link_->command(device_name_, "setClosed");
}
