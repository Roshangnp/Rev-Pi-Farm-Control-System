#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

inline constexpr uint16_t DEVICE_SERVER_PORT = 47100;

// One TCP connection to another RevPi's device_server, shared by every remote
// device owned by that RevPi. Connects and reconnects on its own, and sends a
// heartbeat so the owner's watchdog knows this controller is still alive.
//
// Protocol: one request line, one reply line.
//   "<device> <method> [arg]"  ->  "ok [value]" | "fail" | "err <reason>"
class remote_link{
    public:
    remote_link(std::string host, uint16_t port = DEVICE_SERVER_PORT);
    ~remote_link();

    remote_link(const remote_link&) = delete;
    remote_link& operator=(const remote_link&) = delete;

    // True if the remote device replied "ok".
    bool command(const std::string& device, const std::string& method, const std::string& arg = "");
    // True if the remote device replied "ok <value>"; value gets the text after "ok ".
    bool query(const std::string& device, const std::string& method, std::string& value);
    bool queryBool(const std::string& device, const std::string& method, bool& value);
    bool queryFloat(const std::string& device, const std::string& method, float& value);

    bool isConnected() const;

    private:
    bool call(const std::string& request, std::string& reply);
    bool connectLocked();
    void disconnectLocked();
    void heartbeatLoop();

    std::string host_;
    uint16_t port_;
    mutable std::mutex mutex_;
    int fd_ = -1;
    std::string rx_buffer_;
    std::chrono::steady_clock::time_point last_connect_attempt_{};
    std::chrono::steady_clock::time_point last_activity_{};
    std::atomic<bool> running_{true};
    std::thread heartbeat_;
};
