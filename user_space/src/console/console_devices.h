#pragma once
#include <functional>
#include <string>
#include <vector>

class pump;
class valve;
class switched_load;
class irrigation;
class soil_sensor;

// The farm devices the console shows, grouped by tab. A user-space program like
// any other: every device comes from config.h (bound in config_host_pc.cpp:
// RevPi devices as remote_*, WiseConn zones and probes as wiseconn_*) and the
// RO from ro_config.h, so anything the console does, any host_pc program can do.
// Reads come from remote_link's snapshot cache or wiseconn_link's poll cache,
// so they never block. Rows hold non-owning pointers to the config's devices.

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
    pump* device;
    bool speed_control = false;        // 0-1 speed box (RO pump VFD)
    bool pressure_control = false;     // psi setpoint box (irrigation pump)
};

struct valve_row {
    std::string label;
    valve* device;
};

struct load_row {
    std::string label;
    switched_load* device;
};

struct toggle_row {
    std::string label;
    std::function<bool(bool&)> read;
};

// WiseConn irrigation zone, read only in the console (on/off)
struct irrigation_row {
    std::string label;
    irrigation* device;
};

// WiseConn soil probe: moisture, EC and temperature per depth (shallowest first)
struct soil_row {
    std::string label;
    soil_sensor* device;
    std::vector<std::string> depths;   // column headings, one per depth
};

class console_devices{
    public:
    console_devices();

    console_devices(const console_devices&) = delete;
    console_devices& operator=(const console_devices&) = delete;

    // One vector per group (dashboard tile), in display order
    std::vector<sensor_row> ro_pressures;
    std::vector<sensor_row> ro_flows;
    std::vector<sensor_row> conductivities;    // feed and permeate TDS (uS/cm)
    std::vector<sensor_row> cip_temps;         // CIP tank temperature, in the CIP heater tile
    std::vector<sensor_row> tank_levels;
    std::vector<float_row> chemical_floats;
    std::vector<float_row> water_floats;
    std::vector<pump_row> ro_pumps;            // RO-Dashboard
    std::vector<pump_row> upper_pumps;         // well 4, irrigation: Main-Dashboard
    std::vector<load_row> loads;               // CIP heater, with the RO pumps
    std::vector<valve_row> valves;
    std::vector<toggle_row> toggles;           // panel switches, on the RO tab
    std::vector<irrigation_row> irrigations;   // WiseConn zones, Main-Dashboard
    std::vector<soil_row> soil_probes;         // WiseConn soil probes, Main-Dashboard

    // Header link lights, from a read on each machine (true = it answered)
    std::function<bool()> ro_up;
    std::function<bool()> upper_up;
};
