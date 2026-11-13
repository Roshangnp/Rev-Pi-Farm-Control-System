#include "wiseconn_irrigation.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>

namespace {

constexpr int MAX_DURATION_MINUTES = 24 * 60;

std::tm localNow() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    return local;
}

}  // namespace

wiseconn_irrigation::wiseconn_irrigation(wiseconn_link& link, std::string name, std::string zone_id)
    : link_(link), name_(std::move(name)), zone_id_(std::move(zone_id)) {
    if (!zone_id_.empty()) link_.watchHydraulics();
}

bool wiseconn_irrigation::readOn(bool& on) {
    return !zone_id_.empty() && link_.readZoneOn(zone_id_, on);
}

bool wiseconn_irrigation::scheduleIrrigation(int startHour, int startMinute, int endHour, int endMinute) {
    if (startHour < 0 || startHour > 23 || endHour < 0 || endHour > 23 ||
        startMinute < 0 || startMinute > 59 || endMinute < 0 || endMinute > 59) {
        std::fprintf(stderr, "wiseconn: %s: schedule refused: hours 0-23, minutes 0-59\n", name_.c_str());
        return false;
    }
    int start = startHour * 60 + startMinute;
    int end = endHour * 60 + endMinute;
    if (end <= start) {
        std::fprintf(stderr, "wiseconn: %s: schedule refused: end %02d:%02d is not after start %02d:%02d "
                     "(both are today)\n", name_.c_str(), endHour, endMinute, startHour, startMinute);
        return false;
    }

    std::tm now = localNow();
    if (start < now.tm_hour * 60 + now.tm_min) {
        std::fprintf(stderr, "wiseconn: %s: schedule refused: start %02d:%02d has already passed today\n",
                     name_.c_str(), startHour, startMinute);
        return false;
    }

    std::tm begin = now, finish = now;
    begin.tm_hour = startHour;
    begin.tm_min = startMinute;
    begin.tm_sec = 0;
    finish.tm_hour = endHour;
    finish.tm_min = endMinute;
    finish.tm_sec = 0;
    char what[64];
    std::snprintf(what, sizeof(what), "schedule today %02d:%02d-%02d:%02d", startHour, startMinute, endHour, endMinute);
    if (zone_id_.empty()) return report(false, what, "no zone id in wiseconn_keys.txt");
    std::string error, id;
    bool ok = link_.createIrrigation(zone_id_, begin, finish, id, error);
    saveScheduled(ok, id);
    return report(ok, what, error);
}

bool wiseconn_irrigation::scheduleIrrigation(int durationMinutes) {
    if (durationMinutes <= 0 || durationMinutes > MAX_DURATION_MINUTES) {
        std::fprintf(stderr, "wiseconn: %s: schedule refused: duration must be 1 to %d minutes\n",
                     name_.c_str(), MAX_DURATION_MINUTES);
        return false;
    }
    std::string what = "irrigate now for " + std::to_string(durationMinutes) + " min";
    if (zone_id_.empty()) return report(false, what, "no zone id in wiseconn_keys.txt");
    std::string error, id;
    bool ok = link_.createIrrigationNow(zone_id_, durationMinutes, id, error);
    saveScheduled(ok, id);
    return report(ok, what, error);
}

bool wiseconn_irrigation::report(bool ok, const std::string& what, const std::string& error) const {
    if (ok) std::fprintf(stderr, "wiseconn: %s: %s: accepted\n", name_.c_str(), what.c_str());
    else std::fprintf(stderr, "wiseconn: %s: %s failed: %s\n", name_.c_str(), what.c_str(), error.c_str());
    return ok;
}

void wiseconn_irrigation::saveScheduled(bool ok, const std::string& irrigation_id) {
    if (!ok) return;
    if (irrigation_id.empty()) {
        std::fprintf(stderr, "wiseconn: %s: WiseConn's reply had no irrigation id, it can't be cancelled from here\n",
                     name_.c_str());
        return;
    }
    scheduled_ids_.push_back(irrigation_id);
}

bool wiseconn_irrigation::readScheduledIrrigation(int& id) {
    if (scheduled_ids_.empty()) return false;
    id = std::atoi(scheduled_ids_.back().c_str());
    return true;
}

// Deletes every irrigation booked from here; the ones WiseConn refuses (or that
// can't be reached) stay in the list, so a later cancel retries them
bool wiseconn_irrigation::cancelIrrigation() {
    if (scheduled_ids_.empty()) return report(false, "cancel irrigations", "no irrigation scheduled from here");
    bool ok = true;
    std::vector<std::string> kept;
    for (const std::string& id : scheduled_ids_) {
        std::string error;
        bool deleted = link_.deleteIrrigation(id, error);
        report(deleted, "cancel irrigation " + id, error);
        if (!deleted) kept.push_back(id);
        ok = deleted && ok;
    }
    scheduled_ids_ = kept;
    return ok;
}
