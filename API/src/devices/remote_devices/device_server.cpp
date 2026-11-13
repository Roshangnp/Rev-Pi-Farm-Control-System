#include "device_server.h"

#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>

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

// {18, 22.3, 25.1} -> "ok 18,22.3,25.1" (no spaces, ';' or '=', so it fits a snapshot entry)
std::string okFloats(bool success, const std::vector<float>& values) {
    if (!success) return "fail";
    std::string reply = "ok ";
    char text[32];
    for (size_t i = 0; i < values.size(); ++i) {
        std::snprintf(text, sizeof(text), "%.6g", static_cast<double>(values[i]));
        if (i > 0) reply += ',';
        reply += text;
    }
    return reply;
}

// Whole text is a non-negative integer of at most 4 digits
bool parseSmallInt(const std::string& text, int& value) {
    if (text.empty() || text.size() > 4 || text.find_first_not_of("0123456789") != std::string::npos) return false;
    value = std::atoi(text.c_str());
    return true;
}

// "HH:MM-HH:MM" -> start and end hour/minute (ranges are checked by the device)
bool parseTimeRange(const std::string& text, int& sh, int& sm, int& eh, int& em) {
    size_t dash = text.find('-');
    if (dash == std::string::npos) return false;
    auto parseTime = [](const std::string& t, int& h, int& m) {
        size_t colon = t.find(':');
        return colon != std::string::npos && parseSmallInt(t.substr(0, colon), h) &&
               parseSmallInt(t.substr(colon + 1), m);
    };
    return parseTime(text.substr(0, dash), sh, sm) && parseTime(text.substr(dash + 1), eh, em);
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
bool device_server::addDevice(const std::string& name, soil_sensor& device) {return addDeviceRef(name, &device);}
bool device_server::addDevice(const std::string& name, irrigation& device) {return addDeviceRef(name, &device);}

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
    (void)client_id;
    auto found = devices_.find(name);
    if (found == devices_.end()) return "err unknown device";

    return std::visit([&](auto* device) -> std::string {
        using T = std::remove_pointer_t<decltype(device)>;
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
            else if (method == "setOn") {reply = okIf(device->setOn());}
            else if (method == "setOff") {reply = okIf(device->setOff());}
            else if (method == "setSpeed") {
                float speed;
                if (!parseFloat(arg, speed)) return "err bad argument";
                reply = okIf(device->setSpeed(speed));
            }
            else if (method == "setPressure") {
                float pressure;
                if (!parseFloat(arg, pressure)) return "err bad argument";
                reply = okIf(device->setPressure(pressure));
            }
        } else if constexpr (std::is_same_v<T, valve>) {
            if (method == "setOpen") {reply = okIf(device->setOpen());}
            else if (method == "setClosed") {reply = okIf(device->setClosed());}
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
            if (method == "setOn") {reply = okIf(device->setOn());}
            else if (method == "setOff") {reply = okIf(device->setOff());}
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
            if (method == "readPressTotal") {
                uint32_t total = 0;
                bool success = device->readPressTotal(total);
                reply = okUint(success, total);
            }
        } else if constexpr (std::is_same_v<T, soil_sensor>) {
            std::vector<float> values;
            if (method == "readMoisture") {reply = okFloats(device->readMoisture(values), values);}
            else if (method == "readETC") {reply = okFloats(device->readETC(values), values);}
            else if (method == "readTemperature") {reply = okFloats(device->readTemperature(values), values);}
        } else if constexpr (std::is_same_v<T, irrigation>) {
            if (method == "readOn") {
                bool on = false;
                bool success = device->readOn(on);
                reply = success ? okBool(on) : "fail";
            } else if (method == "scheduleIrrigation") {
                int sh, sm, eh, em, minutes;
                if (parseTimeRange(arg, sh, sm, eh, em)) reply = okIf(device->scheduleIrrigation(sh, sm, eh, em));
                else if (parseSmallInt(arg, minutes)) reply = okIf(device->scheduleIrrigation(minutes));
                else return "err bad argument";
            } else if (method == "readScheduledIrrigation") {
                int id = 0;
                reply = device->readScheduledIrrigation(id) ? "ok " + std::to_string(id) : "fail";
            } else if (method == "cancelIrrigation") {
                reply = okIf(device->cancelIrrigation());
            } else if (method == "cancelTopIrrigation") {
                reply = okIf(device->cancelTopIrrigation());
            }
        }

        return reply;
    }, found->second);
}

void device_server::snapshot(std::string& out) {
    for (const auto& entry : devices_) {
        const std::string& name = entry.first;
        // The read methods handleRequest() answers for each device type
        std::vector<const char*> methods = std::visit([](auto* device) -> std::vector<const char*> {
            using T = std::remove_pointer_t<decltype(device)>;
            if constexpr (std::is_same_v<T, pump>)
                return {"hasSpeedControl", "hasAlarm", "hasPressureControl", "readAlarm", "readOn", "readSpeed", "readPressure"};
            else if constexpr (std::is_same_v<T, valve>) return {"readOpen"};
            else if constexpr (std::is_same_v<T, float_switch>) return {"readSubmerged"};
            else if constexpr (std::is_same_v<T, pressure_transducer>) return {"readPressure"};
            else if constexpr (std::is_same_v<T, tds_sensor>) return {"readTds"};
            else if constexpr (std::is_same_v<T, flow_sensor>) return {"readFlow"};
            else if constexpr (std::is_same_v<T, level_sensor>) return {"readLevel"};
            else if constexpr (std::is_same_v<T, conductivity_sensor>) return {"readConductivity"};
            else if constexpr (std::is_same_v<T, temperature_sensor>) return {"readTemperature"};
            else if constexpr (std::is_same_v<T, switched_load>) return {"readOn"};
            else if constexpr (std::is_same_v<T, toggle_switch>) return {"readValue"};
            else if constexpr (std::is_same_v<T, toggle_switch_double_pole>) return {"readPole1", "readPole2"};
            else if constexpr (std::is_same_v<T, push_button>) return {"readPressTotal"};
            else if constexpr (std::is_same_v<T, soil_sensor>) return {"readMoisture", "readETC", "readTemperature"};
            else if constexpr (std::is_same_v<T, irrigation>) return {"readOn", "readScheduledIrrigation"};
            else return {};
        }, entry.second);
        for (const char* method : methods) appendSnapshotEntry(out, name + "." + method, handleRequest(0, name, method, ""));
    }
}
