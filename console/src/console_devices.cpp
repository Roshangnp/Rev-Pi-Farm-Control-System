#include "console_devices.h"

#include "devices/remote_devices/remote_conductivity_sensor.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_irrigation.h"
#include "devices/remote_devices/remote_level_sensor.h"
#include "devices/remote_devices/remote_pressure_transducer.h"
#include "devices/remote_devices/remote_pump.h"
#include "devices/remote_devices/remote_soil_sensor.h"
#include "devices/remote_devices/remote_switched_load.h"
#include "devices/remote_devices/remote_temperature_sensor.h"
#include "devices/remote_devices/remote_toggle_switch.h"
#include "devices/remote_devices/remote_valve.h"

namespace {

// "media_outlet_pressure" -> "media outlet pressure"
std::string labelFor(std::string name) {
    for (char& c : name) {
        if (c == '_') c = ' ';
    }
    return name;
}

sensor_row pressure(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_pressure_transducer>(link, name);
    return {labelFor(name), "psi", [d](float& v) { return d->readPressure(v); }};
}

sensor_row flow(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_flow_sensor>(link, name);
    return {labelFor(name), "gpm", [d](float& v) { return d->readFlow(v); }};
}

sensor_row conductivity(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_conductivity_sensor>(link, name);
    return {labelFor(name), "uS/cm", [d](float& v) { return d->readConductivity(v); }};
}

sensor_row temperature(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_temperature_sensor>(link, name);
    return {labelFor(name), "F", [d](float& v) { return d->readTemperature(v); }};
}

sensor_row level(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_level_sensor>(link, name);
    return {labelFor(name), "", [d](float& v) { return d->readLevel(v); }};
}

float_row floatSwitch(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_float_switch>(link, name);
    return {labelFor(name), [d](bool& v) { return d->readSubmerged(v); }};
}

pump_row pumpRow(remote_link& link, const std::string& name, bool speed = false, bool pressure = false) {
    return {labelFor(name), std::make_shared<remote_pump>(link, name), speed, pressure};
}

valve_row valveRow(remote_link& link, const std::string& name) {
    return {labelFor(name), std::make_shared<remote_valve>(link, name)};
}

load_row loadRow(remote_link& link, const std::string& name) {
    return {labelFor(name), std::make_shared<remote_switched_load>(link, name)};
}

toggle_row toggleSwitch(remote_link& link, const std::string& name) {
    auto d = std::make_shared<remote_toggle_switch>(link, name);
    return {labelFor(name), [d](bool& v) { return d->readValue(v); }};
}

// "valve_10_3" -> "valve 10-3", "overhead_2" -> "overhead 2", like WiseConn's names
std::string irrigationLabel(const std::string& name) {
    const std::string prefix = "valve_";
    if (name.compare(0, prefix.size(), prefix) == 0 && name.find_first_not_of("0123456789_", prefix.size()) == std::string::npos) {
        std::string rest = name.substr(prefix.size());
        for (char& c : rest) {
            if (c == '_') c = '-';
        }
        return "valve " + rest;
    }
    return labelFor(name);
}

// Read only: the console never calls scheduleIrrigation
irrigation_row irrigationRow(remote_link& link, const std::string& name) {
    return {irrigationLabel(name), std::make_shared<remote_irrigation>(link, name)};
}

// "zone_10_3_soil" -> "zone 10-3"
std::string soilLabel(const std::string& name) {
    std::string label = name;
    const std::string suffix = "_soil";
    if (label.size() > suffix.size() && label.compare(label.size() - suffix.size(), suffix.size(), suffix) == 0) {
        label.erase(label.size() - suffix.size());
    }
    const std::string prefix = "zone_";
    if (label.compare(0, prefix.size(), prefix) == 0) {
        std::string rest = label.substr(prefix.size());
        for (char& c : rest) {
            if (c == '_') c = '-';
        }
        return "zone " + rest;
    }
    return labelFor(label);
}

// depths: column headings, shallowest first (the order of the ids in wiseconn_keys.txt)
soil_row soilRow(remote_link& link, const std::string& name, std::vector<std::string> depths) {
    return {soilLabel(name), std::make_shared<remote_soil_sensor>(link, name), std::move(depths)};
}

}  // namespace

// Device names are the config.h names, which every machine's service serves.
// Add a line here to show a new device (it must be served by the machines the
// console runs on, see CLAUDE.md).
console_devices::console_devices()
    : local("127.0.0.1"), ro(local), ro_procedures(local, "ro_procedure") {
    // Everything comes through the one local connection; r/u/w only keep the
    // list readable by owner (RO RevPi, upper tank, WiseConn)
    remote_link& r = local;
    remote_link& u = local;
    remote_link& w = local;

    ro_pressures = {
        pressure(r, "media_inlet_pressure"),
        pressure(r, "media_outlet_pressure"),
        pressure(r, "aqualine_inlet_pressure"),
        pressure(r, "aqualine_outlet_pressure"),
        pressure(r, "primary_pressure"),
        pressure(r, "interstage_pressure"),
        pressure(r, "concentrate_pressure"),
        pressure(r, "permeate_pressure"),
        pressure(r, "cip_inlet_pressure"),
        pressure(r, "cip_outlet_pressure"),
        pressure(r, "air_pressure"),
    };

    ro_flows = {
        flow(r, "feed_flow"),
        flow(r, "permeate_flow"),
        flow(r, "concentrate_flow"),
    };

    conductivities = {
        conductivity(r, "feed_conductivity"),
        conductivity(r, "permeate_conductivity"),
    };

    cip_temps = {
        temperature(r, "cip_temp"),
    };

    tank_levels = {
        level(r, "feed_tank_water_level"),
        level(u, "upper_tank_water_level"),
    };

    chemical_floats = {
        floatSwitch(r, "high_ph_tank_float"),
        floatSwitch(r, "low_ph_tank_float"),
        floatSwitch(r, "asa_tank_low_float"),
        floatSwitch(r, "cip_tank_low_float"),
        floatSwitch(r, "cip_tank_mid_float"),
        floatSwitch(r, "cip_tank_high_float"),
    };

    water_floats = {
        floatSwitch(r, "feed_tank_low_float"),
        floatSwitch(r, "permeate_tank_high_float"),
        floatSwitch(u, "upper_tank_high_float"),
    };

    ro_pumps = {
        pumpRow(r, "feed_pump"),
        pumpRow(r, "ro_pump", true),
        pumpRow(r, "repressure_pump"),
        pumpRow(r, "asa_pump"),
        pumpRow(r, "high_ph_pump"),
        pumpRow(r, "low_ph_pump"),
    };

    upper_pumps = {
        pumpRow(u, "well_4_pump"),
        pumpRow(u, "irrigation_pump", false, true),
    };

    loads = {
        loadRow(r, "cip_heater"),
    };

    valves = {
        valveRow(r, "inlet_valve"),
        valveRow(r, "flush_valve"),
        valveRow(r, "cip_inlet_valve"),
        valveRow(r, "concentrate_cip_valve"),
        valveRow(r, "permeate_cip_valve"),
        valveRow(r, "media_backwash_1_valve"),
        valveRow(r, "media_backwash_2_valve"),
    };

    toggles = {
        toggleSwitch(r, "repressure_toggle_switch"),
        toggleSwitch(r, "ro_toggle_switch"),
    };

    irrigations = {
        irrigationRow(w, "valve_1_1"),
        irrigationRow(w, "valve_1_2"),
        irrigationRow(w, "valve_2_1"),
        irrigationRow(w, "valve_3_1"),
        irrigationRow(w, "valve_4_1"),
        irrigationRow(w, "valve_5_1"),
        irrigationRow(w, "valve_5_2"),
        irrigationRow(w, "valve_5_3"),
        irrigationRow(w, "valve_5_4"),
        irrigationRow(w, "valve_6_1"),
        irrigationRow(w, "valve_6_2"),
        irrigationRow(w, "valve_7_1"),
        irrigationRow(w, "valve_8_1"),
        irrigationRow(w, "valve_9_1"),
        irrigationRow(w, "valve_10_1"),
        irrigationRow(w, "valve_10_2"),
        irrigationRow(w, "valve_10_3"),
        irrigationRow(w, "overhead_1"),
        irrigationRow(w, "overhead_2"),
        irrigationRow(w, "overhead_3"),
        irrigationRow(w, "overhead_4"),
        irrigationRow(w, "overhead_5"),
        irrigationRow(w, "overhead_6"),
        irrigationRow(w, "city_water_fill_valve"),
    };

    soil_probes = {
        soilRow(w, "zone_7_soil", {"4 in", "8 in", "12 in"}),
        soilRow(w, "zone_8_soil", {"4 in", "8 in", "12 in"}),
        soilRow(w, "zone_10_3_soil", {"4 in", "8 in", "12 in"}),
    };
}

bool console_devices::roUp() {
    RO_state s;
    return ro.readState(s);
}

bool console_devices::upperTankUp() {
    std::string value;
    return local.query("upper_tank_high_float", "readSubmerged", value);
}
