#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "protocol.h"

// One TCP connection to another machine's line_server, shared by every remote
// device (and remote_ro) owned by that machine.
//
// A background thread connects (and reconnects), and asks for a "snapshot" of
// every readable value every link_timing::SNAPSHOT_PERIOD (protocol.h). Reads
// (query*) answer from that cache at once and never touch the network: a value
// older than STALE_TIMEOUT, or one the owner couldn't read, is a failed read.
// The snapshot requests also keep the owner from idling the connection out. When the link
// drops (the owner closed it, e.g. a restart) the cache is cleared, so reads fail
// at once; a stall with the link still up fails reads after STALE_TIMEOUT.
//
// Commands (command()) are sent on the same connection and wait up to
// COMMAND_TIMEOUT for their own reply; a late reply is discarded when it
// arrives. A successful command asks for a fresh snapshot right away, so its
// readback shows up within a round trip. While disconnected, commands fail at
// once.
//
// Thread safe: any thread may call any method.
class remote_link{
    public:
    remote_link(std::string host, uint16_t port = LINE_SERVER_PORT);
    ~remote_link();

    remote_link(const remote_link&) = delete;
    remote_link& operator=(const remote_link&) = delete;

    // True if the remote replied "ok".
    bool command(const std::string& device, const std::string& method, const std::string& arg = "");
    // True if the latest snapshot has a fresh, successful value for device.method.
    bool query(const std::string& device, const std::string& method, std::string& value);
    bool queryBool(const std::string& device, const std::string& method, bool& value);
    bool queryFloat(const std::string& device, const std::string& method, float& value);

    bool isConnected() const;

    private:
    using clock_type = std::chrono::steady_clock;

    struct pending {
        bool snapshot = false;
        bool done = false;
        bool replied = false;  // false: the link dropped before the reply
        std::string reply;
    };
    struct cached {
        bool ok;
        std::string value;
        clock_type::time_point time;
    };

    void ioLoop();
    bool connectNow();                          // io thread, mutex not held
    void disconnectLocked();                    // io thread, mutex held
    bool sendLocked();                          // io thread, mutex held
    bool receive();                             // io thread, mutex not held
    void handleReplyLocked(const std::string& line);
    void storeSnapshotLocked(const std::string& reply, clock_type::time_point now);
    void wake();

    std::string host_;
    uint16_t port_;
    clock_type::time_point created_;

    mutable std::mutex mutex_;
    std::condition_variable replies_;           // a reply or snapshot arrived, or the link dropped
    int fd_ = -1;                               // only the io thread opens and closes it
    bool connected_ = false;
    std::string tx_buffer_;
    std::string rx_buffer_;
    std::deque<std::shared_ptr<pending>> pending_;  // in request order
    bool snapshot_outstanding_ = false;
    bool snapshot_soon_ = false;
    bool have_snapshot_ = false;
    clock_type::time_point next_snapshot_{};
    clock_type::time_point last_receive_{};
    clock_type::time_point last_connect_attempt_{};
    std::unordered_map<std::string, cached> cache_;  // "<device>.<method>"

    int wake_fd_ = -1;
    std::atomic<bool> running_{true};
    std::thread io_;                            // last: starts after everything above exists
};
