#include "console_devices.h"

#include "devices/remote_devices/remote_conductivity_sensor.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_level_sensor.h"
#include "devices/remote_devices/remote_pressure_transducer.h"
#include "devices/remote_devices/remote_pump.h"
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

}  // namespace

// Device names are the ones each RevPi registers in its device_server
// (config_ro.cpp, config_uppertank.cpp). Add a line here to show a new device.
console_devices::console_devices(const std::string& ro_host, const std::string& upper_host)
    : ro_link(ro_host), upper_link(upper_host), ro(ro_link) {
    remote_link& r = ro_link;
    remote_link& u = upper_link;

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
        conductivity(r, "feed_conductivity"),
        conductivity(r, "permeate_conductivity"),
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

    pumps = {
        pumpRow(r, "feed_pump"),
        pumpRow(r, "ro_pump", true),
        pumpRow(r, "repressure_pump"),
        pumpRow(r, "asa_pump"),
        pumpRow(r, "high_ph_pump"),
        pumpRow(r, "low_ph_pump"),
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
}
