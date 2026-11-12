#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "devices/wiseconn_devices/wiseconn_keys.h"
#include "devices/wiseconn_devices/wiseconn_link.h"
#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"
#include "remote_ro/remote_ro.h"

class pump;
class valve;
class switched_load;
class irrigation;
class soil_sensor;

// The farm devices the console shows, grouped by tab. RevPi devices are remote:
// the console is a client of the RevPis' line servers (on the RO RevPi itself,
// connect to 127.0.0.1), so it never touches hardware and runs anywhere.
// WiseConn devices are read from the WiseConn cloud API directly (wiseconn_link,
// keys built in from wiseconn_keys.txt), the same way on every machine.
// Reads come from remote_link's snapshot cache or wiseconn_link's poll cache,
// so they never block. The UI only sees the devices.h base classes.

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

// WiseConn irrigation zone, read only in the console (on/off)
struct irrigation_row {
    std::string label;
    std::shared_ptr<irrigation> device;
};

// WiseConn soil probe: moisture, EC and temperature per depth (shallowest first)
struct soil_row {
    std::string label;
    std::shared_ptr<soil_sensor> device;
    std::vector<std::string> depths;   // column headings, one per depth
};

class console_devices{
    public:
    console_devices(const std::string& ro_host, const std::string& upper_host);

    console_devices(const console_devices&) = delete;
    console_devices& operator=(const console_devices&) = delete;

    remote_link ro_link;
    remote_link upper_link;
    remote_ro ro;
    remote_procedure ro_procedures;   // the RO's procedures ("ro_procedure" on the RO RevPi)
    wiseconn_keys wiseconn_config;    // from wiseconn_keys.txt (zone ids), loaded before the link
    wiseconn_link wiseconn;           // WiseConn cloud API (before the devices that use it)

    // One vector per group (tab or dashboard tile), in display order
    std::vector<sensor_row> ro_pressures;
    std::vector<sensor_row> ro_flows;
    std::vector<sensor_row> conductivities;    // feed and permeate TDS (uS/cm)
    std::vector<sensor_row> cip_temps;         // CIP tank temperature, in the CIP heater tile
    std::vector<sensor_row> tank_levels;
    std::vector<float_row> chemical_floats;
    std::vector<float_row> water_floats;
    std::vector<pump_row> ro_pumps;            // RO-Dashboard and the pump tab
    std::vector<pump_row> upper_pumps;         // well 4, irrigation: Main-Dashboard and the pump tab
    std::vector<load_row> loads;               // CIP heater, with the RO pumps
    std::vector<valve_row> valves;
    std::vector<toggle_row> toggles;           // panel switches, on the RO tab
    std::vector<irrigation_row> irrigations;   // WiseConn zones, Main-Dashboard
    std::vector<soil_row> soil_probes;         // WiseConn soil probes, Main-Dashboard
};
