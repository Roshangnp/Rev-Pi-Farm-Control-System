#include "remote_ro.h"

#include <cstdio>

bool remote_ro::setState(RO_state s) {
    return link_.command(name_, "setState", toString(s));
}

bool remote_ro::readState(RO_state& s) {
    std::string value;
    return link_.query(name_, "readState", value) && fromString(value, s);
}

bool remote_ro::isSettling(bool& settling) {
    return link_.queryBool(name_, "isSettling", settling);
}

bool remote_ro::setRoSpeed(float speed) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.6g", static_cast<double>(speed));
    return link_.command(name_, "setRoSpeed", text);
}
