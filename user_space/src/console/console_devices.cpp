#include "console_devices.h"

#include "config.h"
#include "ro_config.h"

namespace {

// "media_outlet_pressure" -> "media outlet pressure"
std::string labelFor(std::string name) {
    for (char& c : name) {
        if (c == '_') c = ' ';
    }
    return name;
}

sensor_row pressure(const char* name, pressure_transducer& d) {
    return {labelFor(name), "psi", [&d](float& v) { return d.readPressure(v); }};
}

sensor_row flow(const char* name, flow_sensor& d) {
    return {labelFor(name), "gpm", [&d](float& v) { return d.readFlow(v); }};
}

sensor_row conductivity(const char* name, conductivity_sensor& d) {
    return {labelFor(name), "uS/cm", [&d](float& v) { return d.readConductivity(v); }};
}

sensor_row temperature(const char* name, temperature_sensor& d) {
    return {labelFor(name), "F", [&d](float& v) { return d.readTemperature(v); }};
}

sensor_row level(const char* name, level_sensor& d) {
    return {labelFor(name), "", [&d](float& v) { return d.readLevel(v); }};
}

float_row floatSwitch(const char* name, float_switch& d) {
    return {labelFor(name), [&d](bool& v) { return d.readSubmerged(v); }};
}

pump_row pumpRow(const char* name, pump& d, bool speed = false, bool pressure = false) {
    return {labelFor(name), &d, speed, pressure};
}

valve_row valveRow(const char* name, valve& d) {
    return {labelFor(name), &d};
}

load_row loadRow(const char* name, switched_load& d) {
    return {labelFor(name), &d};
}

toggle_row toggleSwitch(const char* name, toggle_switch& d) {
    return {labelFor(name), [&d](bool& v) { return d.readValue(v); }};
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
irrigation_row irrigationRow(const char* name, irrigation& d) {
    return {irrigationLabel(name), &d};
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
soil_row soilRow(const char* name, soil_sensor& d, std::vector<std::string> depths) {
    return {soilLabel(name), &d, std::move(depths)};
}

}  // namespace

// The config.h name and the device, so the label always matches the name
#define DEVICE(name) #name, name

// Every device here must be bound in config_host_pc.cpp (an unbound one fails to
// link). Add a line here to show a new device.
console_devices::console_devices() {
    ro_pressures = {
        pressure(DEVICE(media_inlet_pressure)),
        pressure(DEVICE(media_outlet_pressure)),
        pressure(DEVICE(aqualine_inlet_pressure)),
        pressure(DEVICE(aqualine_outlet_pressure)),
        pressure(DEVICE(primary_pressure)),
        pressure(DEVICE(interstage_pressure)),
        pressure(DEVICE(concentrate_pressure)),
        pressure(DEVICE(permeate_pressure)),
        pressure(DEVICE(cip_inlet_pressure)),
        pressure(DEVICE(cip_outlet_pressure)),
        pressure(DEVICE(air_pressure)),
    };

    ro_flows = {
        flow(DEVICE(feed_flow)),
        flow(DEVICE(permeate_flow)),
        flow(DEVICE(concentrate_flow)),
    };

    conductivities = {
        conductivity(DEVICE(feed_conductivity)),
        conductivity(DEVICE(permeate_conductivity)),
    };

    cip_temps = {
        temperature(DEVICE(cip_temp)),
    };

    tank_levels = {
        level(DEVICE(feed_tank_water_level)),
        level(DEVICE(upper_tank_water_level)),
    };

    chemical_floats = {
        floatSwitch(DEVICE(high_ph_tank_float)),
        floatSwitch(DEVICE(low_ph_tank_float)),
        floatSwitch(DEVICE(asa_tank_low_float)),
        floatSwitch(DEVICE(cip_tank_low_float)),
        floatSwitch(DEVICE(cip_tank_mid_float)),
        floatSwitch(DEVICE(cip_tank_high_float)),
    };

    water_floats = {
        floatSwitch(DEVICE(feed_tank_low_float)),
        floatSwitch(DEVICE(permeate_tank_high_float)),
        floatSwitch(DEVICE(upper_tank_high_float)),
    };

    ro_pumps = {
        pumpRow(DEVICE(feed_pump)),
        pumpRow(DEVICE(ro_pump), true),
        pumpRow(DEVICE(repressure_pump)),
        pumpRow(DEVICE(asa_pump)),
        pumpRow(DEVICE(high_ph_pump)),
        pumpRow(DEVICE(low_ph_pump)),
    };

    upper_pumps = {
        pumpRow(DEVICE(well_4_pump)),
        pumpRow(DEVICE(irrigation_pump), false, true),
    };

    loads = {
        loadRow(DEVICE(cip_heater)),
    };

    valves = {
        valveRow(DEVICE(inlet_valve)),
        valveRow(DEVICE(flush_valve)),
        valveRow(DEVICE(cip_inlet_valve)),
        valveRow(DEVICE(concentrate_cip_valve)),
        valveRow(DEVICE(permeate_cip_valve)),
        valveRow(DEVICE(media_backwash_1_valve)),
        valveRow(DEVICE(media_backwash_2_valve)),
    };

    toggles = {
        toggleSwitch(DEVICE(repressure_toggle_switch)),
        toggleSwitch(DEVICE(ro_toggle_switch)),
    };

    irrigations = {
        irrigationRow(DEVICE(valve_1_1)),
        irrigationRow(DEVICE(valve_1_2)),
        irrigationRow(DEVICE(valve_2_1)),
        irrigationRow(DEVICE(valve_3_1)),
        irrigationRow(DEVICE(valve_4_1)),
        irrigationRow(DEVICE(valve_5_1)),
        irrigationRow(DEVICE(valve_5_2)),
        irrigationRow(DEVICE(valve_5_3)),
        irrigationRow(DEVICE(valve_5_4)),
        irrigationRow(DEVICE(valve_6_1)),
        irrigationRow(DEVICE(valve_6_2)),
        irrigationRow(DEVICE(valve_7_1)),
        irrigationRow(DEVICE(valve_8_1)),
        irrigationRow(DEVICE(valve_9_1)),
        irrigationRow(DEVICE(valve_10_1)),
        irrigationRow(DEVICE(valve_10_2)),
        irrigationRow(DEVICE(valve_10_3)),
        irrigationRow(DEVICE(overhead_1)),
        irrigationRow(DEVICE(overhead_2)),
        irrigationRow(DEVICE(overhead_3)),
        irrigationRow(DEVICE(overhead_4)),
        irrigationRow(DEVICE(overhead_5)),
        irrigationRow(DEVICE(overhead_6)),
        irrigationRow(DEVICE(city_water_fill_valve)),
    };

    soil_probes = {
        soilRow(DEVICE(zone_7_soil), {"4 in", "8 in", "12 in"}),
        soilRow(DEVICE(zone_8_soil), {"4 in", "8 in", "12 in"}),
        soilRow(DEVICE(zone_10_3_soil), {"4 in", "8 in", "12 in"}),
    };

    // A machine counts as up while it answers: the RO RevPi's RO state, and a
    // float on the upper tank RevPi (fails at once when the link drops, 3 s after a stall)
    ro_up = [] { RO_state s; return readROState(s); };
    upper_up = [] { bool s; return upper_tank_high_float.readSubmerged(s); };
}
