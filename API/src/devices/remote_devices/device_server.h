#pragma once
#include <map>
#include <string>
#include <variant>
#include "devices.h"
#include "line_server.h"


// Serves this RevPi's devices to remote_* devices on other machines, as a
// handler on a line_server (comms/src): each device is registered under its
// name, and requests are "<device> <method> [arg]" with method = the C++ method
// name. The line_server owns the socket and the client watchdog; requests are
// only handled inside its poll(), so devices are never touched at the same time
// as the local control code.
//
// Safe state: when the line_server drops a client (it disconnected, or sent
// nothing for the server's watchdog time, default 20 s), every pump it last
// commanded is turned off, every valve it last commanded is closed, and every
// switched load it last commanded is turned off.
class device_server : public line_handler{
    public:
    device_server() = default;

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
    bool addDevice(const std::string& name, toggle_switch& device);
    bool addDevice(const std::string& name, toggle_switch_double_pole& device);
    bool addDevice(const std::string& name, push_button& device);

    // Registers every device added so far on server. Call after the addDevice
    // calls. False if a name is already taken on the server.
    bool registerWith(line_server& server);

    std::string handleRequest(int client_id, const std::string& name,
                              const std::string& method, const std::string& arg) override;
    // Every read method of every device ("<device>.<method>=<value>"), for remote_link's cache
    void snapshot(std::string& out) override;
    void clientGone(int client_id) override;

    private:
    using device_ref = std::variant<pump*, float_switch*, valve*,
                                    pressure_transducer*, tds_sensor*, flow_sensor*,
                                    level_sensor*, conductivity_sensor*, temperature_sensor*,
                                    switched_load*, toggle_switch*,
                                    toggle_switch_double_pole*, push_button*>;

    bool addDeviceRef(const std::string& name, device_ref device);

    std::map<std::string, device_ref> devices_;
    std::map<std::string, int> commanded_by_; // device name -> client id that last commanded it
};
