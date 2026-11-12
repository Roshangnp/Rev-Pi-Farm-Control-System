#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "devices.h"

// The farm devices the console shows, grouped by tab: the config.h devices,
// like any user-space program. The console links the client config
// (farm_client, API/src/config/client_config.cpp): every device is remote on a
// connection to its owner (127.0.0.1 for this machine's own RevPi), WiseConn
// comes from this machine's main. Reads come from remote_link's snapshot cache,
// so they never block. The UI only sees the devices.h base classes; the RO is
// used through ro_config.h.

struct sensor_row {
    std::string label;
    std::string unit;
    std::function<bool(float&)> read;
};

struct float_row {
    std::string label;
    std::function<bool(bool&)> read;   // true = submerged
};

// Devices are config.h references (they live for the whole program); the
// pointers in the rows don't own them
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

// Irrigation zone (WiseConn), read only in the console (on/off)
struct irrigation_row {
    std::string label;
    std::shared_ptr<irrigation> device;
};

// Soil probe (WiseConn): moisture, EC and temperature per depth (shallowest first)
struct soil_row {
    std::string label;
    std::shared_ptr<soil_sensor> device;
    std::vector<std::string> depths;   // column headings, one per depth
};

class console_devices{
    public:
    console_devices();

    console_devices(const console_devices&) = delete;
    console_devices& operator=(const console_devices&) = delete;

    // Header lights: a machine counts as up while one of its values reads
    bool roUp();
    bool upperTankUp();

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
