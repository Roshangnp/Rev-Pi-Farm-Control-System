#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "devices.h"

inline constexpr uint16_t WEB_UI_PORT = 8080;

// Something that is in one of a fixed list of named states and can be told to
// change state, e.g. the RO controller (main_host_pc adapts ro_config.h to this).
// Shown as the current state, a dropdown with a Set button (browser confirm),
// and a read only "settling" row.
class state_selector{
    public:
    virtual ~state_selector() = default;

    // Every state, in the order the dropdown lists them
    virtual std::vector<std::string> states() const = 0;
    // False if the state can't be read
    virtual bool readState(std::string& state) = 0;
    // Only called with a name from states(). False if it was refused or failed
    virtual bool setState(const std::string& state) = 0;
    // True while a state change is still in progress. False if it can't be read
    virtual bool readSettling(bool& settling) = 0;
};

// Minimal HTTP server listing this machine's devices (local or remote) with
// their values, plus a switch for on/off outputs and a text box for setpoints.
// Single threaded like device_server: requests are only handled inside poll(),
// so devices are never touched at the same time as the local control code.
//
// Password: hardcoded at build time from webui_password.txt in the repo root
// (see CMakeLists.txt). Every /api request must send it, URL-encoded, in an
// X-Password header; the page has a field for it. A wrong password gets 401 and
// locks that IP out (429) for 1 s. No password built in: start() fails.
// Plain HTTP, so anyone sniffing the LAN can read the password.
//
//   GET  /             the page (no password needed, it holds no data)
//   GET  /api/devices  JSON list of devices and their current values
//   POST /api/command  body "<device> <method> [arg]" -> "ok" | "fail" | "err <reason>"
class web_ui{
    public:
    // bind_address: "0.0.0.0" for every interface, "127.0.0.1" for this machine only
    web_ui(uint16_t port = WEB_UI_PORT, std::string bind_address = "0.0.0.0")
    : port_(port), bind_address_(std::move(bind_address)){};
    ~web_ui();

    web_ui(const web_ui&) = delete;
    web_ui& operator=(const web_ui&) = delete;

    // Devices are listed in the order added. Names must be unique and contain
    // no spaces. Devices must outlive the web UI.
    bool addDevice(const std::string& name, pump& device);
    bool addDevice(const std::string& name, float_switch& device);
    bool addDevice(const std::string& name, valve& device);
    // "on" indicator plus start/end time boxes and a Schedule button
    // (scheduleIrrigation(start, end) for today; starts a real irrigation)
    bool addDevice(const std::string& name, irrigation& device);
    bool addDevice(const std::string& name, pressure_transducer& device);
    bool addDevice(const std::string& name, tds_sensor& device);
    bool addDevice(const std::string& name, flow_sensor& device);
    bool addDevice(const std::string& name, level_sensor& device);
    bool addDevice(const std::string& name, conductivity_sensor& device);
    bool addDevice(const std::string& name, temperature_sensor& device);
    bool addDevice(const std::string& name, switched_load& device);
    // Read only: one field per depth for moisture, ETC and temperature, labelled
    // with depth_labels (e.g. {"4 in", "8 in", "12 in"}), or 1, 2, 3... if empty
    bool addDevice(const std::string& name, soil_sensor& device, std::vector<std::string> depth_labels = {});
    // Current state, a dropdown of states() with a Set button (confirmed in the
    // browser; command "<name> setState <STATE>"), and "settling"
    bool addDevice(const std::string& name, state_selector& device);

    // Opens the listening socket. False if the port can't be opened or no
    // password was built in (the reason is printed to stderr).
    bool start();

    // Handles pending HTTP requests. Waits up to timeout_ms for activity,
    // so it can double as the control loop's sleep.
    void poll(int timeout_ms);

    private:
    using device_ref = std::variant<pump*, float_switch*, valve*, pressure_transducer*,
                                    tds_sensor*, flow_sensor*, level_sensor*,
                                    conductivity_sensor*, temperature_sensor*, switched_load*,
                                    soil_sensor*, irrigation*, state_selector*>;

    struct client {
        int fd;
        uint32_t ip; // peer IPv4 address, network byte order
        std::string rx_buffer;
        std::string tx_buffer;
        std::chrono::steady_clock::time_point last_seen;
    };

    bool addDeviceRef(const std::string& name, device_ref device);
    void acceptClients();
    bool readClient(client& c);  // false if the client should be dropped
    bool writeClient(client& c); // false once the reply is sent or the client is gone
    std::string handleRequest(const client& c, const std::string& method, const std::string& path,
                              const std::string& headers, const std::string& body);
    bool checkPassword(const client& c, const std::string& headers, std::string& refusal);
    std::string devicesJson();
    std::string runCommand(const std::string& line);

    uint16_t port_;
    std::string bind_address_;
    int listen_fd_ = -1;
    std::vector<client> clients_;
    std::vector<std::pair<std::string, device_ref>> devices_;
    std::map<std::string, std::vector<std::string>> soil_depth_labels_;  // soil sensor name -> labels
    std::map<uint32_t, std::chrono::steady_clock::time_point> locked_until_; // IP -> end of lockout
};
