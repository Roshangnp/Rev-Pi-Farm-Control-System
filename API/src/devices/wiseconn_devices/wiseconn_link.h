#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
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
// keeps the old values; they just age out. Read only: nothing is ever sent to
// WiseConn except GET requests.
class wiseconn_link{
    public:
    explicit wiseconn_link(const wiseconn_keys& keys);
    wiseconn_link(std::string api_key, std::string base_url, std::chrono::seconds poll_interval,
                  std::chrono::minutes max_age);
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

    private:
    struct sample{
        float value;
        std::chrono::system_clock::time_point measured;
    };

    void pollLoop();
    void pollZone(const std::string& zone_id);

    std::string api_key_;
    std::string base_url_;
    std::chrono::seconds poll_interval_;
    std::chrono::minutes max_age_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<std::string> zones_;
    std::map<std::string, bool> zone_ok_;      // last poll result per zone, to log only changes
    std::map<std::string, sample> samples_;    // measure id -> latest value
    bool poll_now_ = false;
    bool stopping_ = false;
    bool warned_future_ = false;
    std::atomic<int> curl_pid_{0};             // running request, killed by the destructor
    std::thread worker_;
};
