#include "remote_irrigation.h"

#include <cstdio>

bool remote_irrigation::readOn(bool& on) {
    return link_->queryBool(device_name_, "readOn", on);
}

bool remote_irrigation::scheduleIrrigation(int startHour, int startMinute, int endHour, int endMinute) {
    if (startHour < 0 || startHour > 99 || startMinute < 0 || startMinute > 99 ||
        endHour < 0 || endHour > 99 || endMinute < 0 || endMinute > 99) {
        return false;   // the owner checks the real ranges; this only keeps the text well formed
    }
    char arg[16];
    std::snprintf(arg, sizeof(arg), "%02d:%02d-%02d:%02d", startHour, startMinute, endHour, endMinute);
    return link_->command(device_name_, "scheduleIrrigation", arg);
}

bool remote_irrigation::scheduleIrrigation(int durationMinutes) {
    return link_->command(device_name_, "scheduleIrrigation", std::to_string(durationMinutes));
}
