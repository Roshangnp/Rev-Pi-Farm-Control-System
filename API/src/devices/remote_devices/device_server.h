#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include "devices.h"
#include "remote_link.h"


// Serves this RevPi's devices to remote_* devices on other RevPis.
// Single threaded: requests are only handled inside poll(), so devices are
// never touched at the same time as the local control code.
//
// Watchdog: if a remote controller disconnects or sends nothing for
// watchdog_ms, every pump it last commanded is turned off and every valve it
// last commanded is closed, and every switched load it last commanded is turned off.
class device_server{
    public:
    device_server(uint16_t port = DEVICE_SERVER_PORT, int watchdog_ms = 2000)
    : port_(port), watchdog_(watchdog_ms){};
    ~device_server();

    device_server(const device_server&) = delete;
    device_server& operator=(const device_server&) = delete;

    // Names must be unique and contain no spaces. Devices must outlive the server.
    bool addDevice(const std::string& name, pump& device);
    bool addDevice(const std::string& name, float_switch& device);
    bool addDevice(const std::string& name, valve& device);
    bool addDevice(const std::string& name, pressure_transducer& device);
    bool addDevice(const std::string& name, tds_sensor& device);
    bool addDevice(const std::string& name, flow_sensor& device);
    bool addDevice(const std::string& name, level_sensor& device);
    bool addDevice(const std::string& name, conductivity_sensor& device);
    bool addDevice(const std::string& name, temperature_sensor& device);
    bool addDevice(const std::string& name, switched_load& device);

    // Opens the listening socket. False if the port can't be opened.
    bool start();

    // Handles pending requests and runs the watchdog. Waits up to timeout_ms
    // for activity, so it can double as the control loop's sleep.
    void poll(int timeout_ms);

    private:
    using device_ref = std::variant<pump*, float_switch*, valve*,
                                    pressure_transducer*, tds_sensor*, flow_sensor*,
                                    level_sensor*, conductivity_sensor*, temperature_sensor*,
                                    switched_load*>;

    struct client {
        int fd;
        int id;
        std::string rx_buffer;
        std::chrono::steady_clock::time_point last_seen;
    };

    bool addDeviceRef(const std::string& name, device_ref device);
    void acceptClients();
    bool readClient(client& c); // false if the client should be dropped
    std::string handleRequest(client& c, const std::string& line);
    void dropClient(size_t index);
    void applySafeState(int client_id);

    uint16_t port_;
    std::chrono::milliseconds watchdog_;
    int listen_fd_ = -1;
    int next_client_id_ = 1;
    std::vector<client> clients_;
    std::map<std::string, device_ref> devices_;
    std::map<std::string, int> commanded_by_; // device name -> client id that last commanded it
};
