#include "device_server.h"

#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

std::string okBool(bool value) {
    return value ? "ok 1" : "ok 0";
}

std::string okFloat(bool success, float value) {
    if (!success) return "fail";
    char text[32];
    std::snprintf(text, sizeof(text), "ok %.6g", static_cast<double>(value));
    return text;
}

std::string okUint(bool success, uint32_t value) {
    if (!success) return "fail";
    return "ok " + std::to_string(value);
}

std::string okIf(bool success) {
    return success ? "ok" : "fail";
}

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtof(text.c_str(), &end);
    return *end == '\0';
}

}  // namespace

bool device_server::addDevice(const std::string& name, pump& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, float_switch& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, valve& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, pressure_transducer& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, tds_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, flow_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, level_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, conductivity_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, temperature_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, switched_load& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, toggle_switch& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, toggle_switch_double_pole& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, push_button& device) {return addDeviceRef(name, &device);}

bool device_server::addDeviceRef(const std::string& name, device_ref device) {
    if (name.empty() || name.find_first_of(" \t\r\n") != std::string::npos || name == "ping") return false;
    return devices_.emplace(name, device).second;
}

bool device_server::registerWith(line_server& server) {
    for (const auto& entry : devices_) {
        if (!server.addName(entry.first, *this)) return false;
    }
    return true;
}

std::string device_server::handleRequest(int client_id, const std::string& name,
                                         const std::string& method, const std::string& arg) {
    auto found = devices_.find(name);
    if (found == devices_.end()) return "err unknown device";

    return std::visit([&](auto* device) -> std::string {
        using T = std::remove_pointer_t<decltype(device)>;
        bool is_command = false;
        std::string reply = "err unknown method";

        if constexpr (std::is_same_v<T, pump>) {
            if (method == "hasSpeedControl") reply = okBool(device->hasSpeedControl());
            else if (method == "hasAlarm") reply = okBool(device->hasAlarm());
            else if (method == "hasPressureControl") reply = okBool(device->hasPressureControl());
            else if (method == "readAlarm") reply = okBool(device->readAlarm());
            else if (method == "readOn") {
                bool on = false;
                bool success = device->readOn(on);
                reply = success ? okBool(on) : "fail";
            }
            else if (method == "readSpeed") {
                float speed = 0.0f;
                bool success = device->readSpeed(speed);
                reply = okFloat(success, speed);
            }
            else if (method == "readPressure") {
                float pressure = 0.0f;
                bool success = device->readPressure(pressure);
                reply = okFloat(success, pressure);
            }
            else if (method == "setOn") {is_command = true; reply = okIf(device->setOn());}
            else if (method == "setOff") {is_command = true; reply = okIf(device->setOff());}
            else if (method == "setSpeed") {
                float speed;
                if (!parseFloat(arg, speed)) return "err bad argument";
                is_command = true;
                reply = okIf(device->setSpeed(speed));
            }
            else if (method == "setPressure") {
                float pressure;
                if (!parseFloat(arg, pressure)) return "err bad argument";
                is_command = true;
                reply = okIf(device->setPressure(pressure));
            }
        } else if constexpr (std::is_same_v<T, valve>) {
            if (method == "setOpen") {is_command = true; reply = okIf(device->setOpen());}
            else if (method == "setClosed") {is_command = true; reply = okIf(device->setClosed());}
            else if (method == "readOpen") {
                bool open = false;
                bool success = device->readOpen(open);
                reply = success ? okBool(open) : "fail";
            }
        } else if constexpr (std::is_same_v<T, float_switch>) {
            if (method == "readSubmerged") {
                bool submerged = false;
                bool success = device->readSubmerged(submerged);
                reply = success ? okBool(submerged) : "fail";
            }
        } else if constexpr (std::is_same_v<T, pressure_transducer>) {
            if (method == "readPressure") {
                float psi = 0.0f;
                bool success = device->readPressure(psi);
                reply = okFloat(success, psi);
            }
        } else if constexpr (std::is_same_v<T, tds_sensor>) {
            if (method == "readTds") {
                float ppm = 0.0f;
                bool success = device->readTds(ppm);
                reply = okFloat(success, ppm);
            }
        } else if constexpr (std::is_same_v<T, flow_sensor>) {
            if (method == "readFlow") {
                float gpm = 0.0f;
                bool success = device->readFlow(gpm);
                reply = okFloat(success, gpm);
            }
        } else if constexpr (std::is_same_v<T, level_sensor>) {
            if (method == "readLevel") {
                float level = 0.0f;
                bool success = device->readLevel(level);
                reply = okFloat(success, level);
            }
        } else if constexpr (std::is_same_v<T, conductivity_sensor>) {
            if (method == "readConductivity") {
                float us_per_cm = 0.0f;
                bool success = device->readConductivity(us_per_cm);
                reply = okFloat(success, us_per_cm);
            }
        } else if constexpr (std::is_same_v<T, temperature_sensor>) {
            if (method == "readTemperature") {
                float deg_f = 0.0f;
                bool success = device->readTemperature(deg_f);
                reply = okFloat(success, deg_f);
            }
        } else if constexpr (std::is_same_v<T, switched_load>) {
            if (method == "setOn") {is_command = true; reply = okIf(device->setOn());}
            else if (method == "setOff") {is_command = true; reply = okIf(device->setOff());}
            else if (method == "readOn") {
                bool on = false;
                bool success = device->readOn(on);
                reply = success ? okBool(on) : "fail";
            }
        } else if constexpr (std::is_same_v<T, toggle_switch>) {
            if (method == "readValue") {
                bool value = false;
                bool success = device->readValue(value);
                reply = success ? okBool(value) : "fail";
            }
        } else if constexpr (std::is_same_v<T, toggle_switch_double_pole>) {
            if (method == "readPole1") {
                bool on = false;
                bool success = device->readPole1(on);
                reply = success ? okBool(on) : "fail";
            } else if (method == "readPole2") {
                bool on = false;
                bool success = device->readPole2(on);
                reply = success ? okBool(on) : "fail";
            }
        } else if constexpr (std::is_same_v<T, push_button>) {
            if (method == "readPushButtonCount") {
                uint32_t presses = 0;
                bool success = device->readPushButtonCount(presses);
                reply = okUint(success, presses);
            }
        }

        if (is_command) commanded_by_[name] = client_id;
        return reply;
    }, found->second);
}

void device_server::clientGone(int client_id) {
    for (auto it = commanded_by_.begin(); it != commanded_by_.end();) {
        if (it->second != client_id) {
            ++it;
            continue;
        }
        std::visit([](auto* device) {
            using T = std::remove_pointer_t<decltype(device)>;
            if constexpr (std::is_same_v<T, pump>) device->setOff();
            else if constexpr (std::is_same_v<T, valve>) device->setClosed();
            else if constexpr (std::is_same_v<T, switched_load>) device->setOff();
        }, devices_.at(it->first));
        it = commanded_by_.erase(it);
    }
}
