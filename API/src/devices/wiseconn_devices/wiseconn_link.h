#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "wiseconn_keys.h"

// One connection to the WiseConn cloud API, shared by every wiseconn_* device.
// A background thread fetches GET /zones/<id>/measures for each registered zone
// every poll interval (one HTTPS request per zone, about 1 s each, made with
// the curl program) and caches every measure's latest value. Reads only look
// at the cache, so they never block on the network.
//
// A value counts as current while the time WiseConn says it was measured
// (lastDataDate) is within max_age of now. WiseConn writes lastDataDate in the
// farm's local time even though it ends in "Z", so it is read as local time:
// this machine's time zone must be the farm's (checked against the keys'
// time_zone at startup, a mismatch is logged). Soil data arrives every 15 min, so
// the default 45 min allows two missed reports before reads fail. A failed poll
// keeps the old values; they just age out.
//
// Irrigation zones: once watchHydraulics() is called, the thread also fetches
// GET /farms/<farm_id>/hydraulics (every valve with its live status: "Operating" (running) or "Off"
// and the zones it feeds, one request for the whole farm) every hydraulics
// interval (default 10 s, much more often than the soil measures, so a zone
// starting or stopping shows within seconds of WiseConn knowing). That list has
// no timestamps, so it counts as current for 3 hydraulics intervals (at least
// 1 min) after the last good fetch.
//
// Polling only sends GET requests. The two createIrrigation calls and
// deleteIrrigation are the only ones that change anything on WiseConn: each
// sends one request (POST /irrigations, DELETE /irrigations/<id>; never retried,
// so a slow reply can't schedule twice) and blocks the caller until WiseConn
// answers (about 1 s, at most 20 s). The irrigation endpoints only work on
// https://api.wiseconn.com (apiv2 answers them with HTTP 500).
class wiseconn_link{
    public:
    explicit wiseconn_link(const wiseconn_keys& keys);
    wiseconn_link(std::string api_key, std::string base_url, std::chrono::seconds poll_interval,
                  std::chrono::minutes max_age, std::string farm_id = "",
                  std::chrono::seconds hydraulics_interval = std::chrono::seconds(10));
    ~wiseconn_link();

    wiseconn_link(const wiseconn_link&) = delete;
    wiseconn_link& operator=(const wiseconn_link&) = delete;

    // False if there is no API key (nothing is ever fetched)
    bool enabled() const;

    // Starts polling this zone (at once, then every interval). Duplicates are ignored.
    void addZone(const std::string& zone_id);

    // Latest value of a measure. False if it hasn't been fetched yet, WiseConn
    // has no value for it, or the value is older than max_age.
    bool readMeasure(const std::string& measure_id, float& value) const;

    // Starts polling the farm's hydraulics (valves). Needs a farm_id.
    void watchHydraulics();

    // True if any valve feeding the zone is "On". False (unknown) if the zone has
    // no valve in the list, a valve's status is neither On nor Off, or the last
    // good fetch is too old.
    bool readZoneOn(const std::string& zone_id, bool& on) const;

    // Schedules an irrigation on a zone from start to end (farm local time, which
    // is this machine's local time):
    //   POST /irrigations {"zoneId", "initTime", "endTime", "directSchedule": true}
    // True if WiseConn accepted it (2xx); otherwise error has the reason and reply.
    // irrigation_id: the new scheduled irrigation's id from the reply ("" if the
    // reply had none, e.g. an unexpected format).
    bool createIrrigation(const std::string& zone_id, const std::tm& start, const std::tm& end,
                          std::string& irrigation_id, std::string& error);

    // Irrigates a zone starting now for duration_minutes:
    //   POST /irrigations {"zoneId", "duration"}
    bool createIrrigationNow(const std::string& zone_id, int duration_minutes,
                             std::string& irrigation_id, std::string& error);

    // Deletes a scheduled irrigation: DELETE /irrigations/<id>. WiseConn only
    // deletes one in the future with status "Saved" or "Sending irrigation has
    // failed" (not one that is running or done). True on a 2xx.
    bool deleteIrrigation(const std::string& irrigation_id, std::string& error);

    private:
    struct sample{
        float value;
        std::chrono::system_clock::time_point measured;
    };

    void pollLoop();
    void pollZone(const std::string& zone_id);
    void pollHydraulics();
    bool postIrrigation(const std::string& json_body, std::string& irrigation_id, std::string& error);
    std::chrono::steady_clock::duration hydraulicsMaxAge() const;

    std::string api_key_;
    std::string base_url_;
    std::chrono::seconds poll_interval_;          // zone measures
    std::chrono::seconds hydraulics_interval_;    // valve list
    std::chrono::minutes max_age_;
    std::string farm_id_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<std::string> zones_;
    std::map<std::string, bool> zone_ok_;      // last poll result per zone, to log only changes
    std::map<std::string, sample> samples_;    // measure id -> latest value
    bool hydraulics_wanted_ = false;
    int hydraulics_ok_ = -1;                       // last poll result (-1 = none yet), to log only changes
    struct hydraulic{
        std::string type;                  // "Valve", "Pump", ...
        std::string status;                // "Operating" (running) / "Off"
        std::vector<std::string> zone_ids;
    };
    std::vector<hydraulic> hydraulics_;
    std::chrono::steady_clock::time_point hydraulics_fetched_{};
    bool poll_now_ = false;
    bool stopping_ = false;
    bool warned_future_ = false;
    std::atomic<int> curl_pid_{0};             // running request, killed by the destructor
    std::thread worker_;
};
