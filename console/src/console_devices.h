#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "remote_link.h"
#include "remote_ro/remote_ro.h"

class pump;
class valve;
class switched_load;

// The farm devices the console shows, grouped by tab. Every device is remote:
// the console is a client of the RevPis' line servers (on the RO RevPi itself,
// connect to 127.0.0.1), so it never touches hardware and runs anywhere.
// Reads come from remote_link's snapshot cache, so they never block.

struct sensor_row {
    std::string label;
    std::string unit;
    std::function<bool(float&)> read;
};

struct float_row {
    std::string label;
    std::function<bool(bool&)> read;   // true = submerged
};

struct pump_row {
    std::string label;
    std::shared_ptr<pump> device;
    bool speed_control = false;        // 0-1 speed box (RO pump VFD)
    bool pressure_control = false;     // psi setpoint box (irrigation pump)
};

struct valve_row {
    std::string label;
    std::shared_ptr<valve> device;
};

struct load_row {
    std::string label;
    std::shared_ptr<switched_load> device;
};

struct toggle_row {
    std::string label;
    std::function<bool(bool&)> read;
};

class console_devices{
    public:
    console_devices(const std::string& ro_host, const std::string& upper_host);

    console_devices(const console_devices&) = delete;
    console_devices& operator=(const console_devices&) = delete;

    remote_link ro_link;
    remote_link upper_link;
    remote_ro ro;

    // One vector per tab, in display order
    std::vector<sensor_row> ro_pressures;
    std::vector<sensor_row> ro_flows;          // plus conductivity and CIP temperature
    std::vector<sensor_row> tank_levels;
    std::vector<float_row> chemical_floats;
    std::vector<float_row> water_floats;
    std::vector<pump_row> pumps;
    std::vector<load_row> loads;               // CIP heater, on the pump tab
    std::vector<valve_row> valves;
    std::vector<toggle_row> toggles;           // panel switches, on the RO tab
};
