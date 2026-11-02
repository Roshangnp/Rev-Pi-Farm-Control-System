#include "config.h"

#include <chrono>
#include <thread>

#include "devices/remote_devices/remote_link.h"
#include "devices/remote_devices/remote_float_switch.h"
#include "devices/remote_devices/remote_level_sensor.h"
#include "devices/remote_devices/remote_flow_sensor.h"
#include "devices/remote_devices/remote_pressure_transducer.h"
#include "devices/remote_devices/remote_conductivity_sensor.h"
#include "devices/remote_devices/remote_temperature_sensor.h"
#include "devices/remote_devices/remote_pump.h"
#include "devices/remote_devices/remote_switched_load.h"
#include "devices/remote_devices/remote_valve.h"

// ---------------------------------------------------------------------------
// Ubuntu host PC: no local I/O, every device is owned by a RevPi.
// Each RevPi must open port 47100 to this PC's IP (see CLAUDE.md).
// ---------------------------------------------------------------------------

namespace {

remote_link upper_tank_revpi("192.168.1.195");
remote_link ro_revpi("192.168.1.116");

remote_float_switch remote_upper_tank_high_float(upper_tank_revpi, "upper_tank_high_float");
remote_level_sensor remote_upper_tank_water_level(upper_tank_revpi, "upper_tank_water_level");

remote_flow_sensor remote_permeate_flow(ro_revpi, "permeate_flow");
remote_flow_sensor remote_concentrate_flow(ro_revpi, "concentrate_flow");
remote_flow_sensor remote_feed_flow(ro_revpi, "feed_flow");
remote_float_switch remote_feed_tank_high_float(ro_revpi, "feed_tank_high_float");
remote_float_switch remote_high_ph_tank_float(ro_revpi, "high_ph_tank_float");
remote_float_switch remote_cip_tank_mid_float(ro_revpi, "cip_tank_mid_float");
remote_float_switch remote_cip_tank_low_float(ro_revpi, "cip_tank_low_float");
remote_float_switch remote_cip_tank_high_float(ro_revpi, "cip_tank_high_float");
remote_float_switch remote_low_ph_tank_float(ro_revpi, "low_ph_tank_float");
remote_float_switch remote_asa_tank_low_float(ro_revpi, "asa_tank_low_float");
remote_float_switch remote_permeate_tank_high_float(ro_revpi, "permeate_tank_high_float");
remote_float_switch remote_feed_tank_low_float(ro_revpi, "feed_tank_low_float");
remote_pressure_transducer remote_media_outlet_pressure(ro_revpi, "media_outlet_pressure");
remote_pressure_transducer remote_media_inlet_pressure(ro_revpi, "media_inlet_pressure");
remote_pressure_transducer remote_concentrate_pressure(ro_revpi, "concentrate_pressure");
remote_pressure_transducer remote_cip_inlet_pressure(ro_revpi, "cip_inlet_pressure");
remote_pressure_transducer remote_interstage_pressure(ro_revpi, "interstage_pressure");
remote_pressure_transducer remote_aqualine_outlet_pressure(ro_revpi, "aqualine_outlet_pressure");
remote_pressure_transducer remote_cip_outlet_pressure(ro_revpi, "cip_outlet_pressure");
remote_pressure_transducer remote_primary_pressure(ro_revpi, "primary_pressure");
remote_pressure_transducer remote_aqualine_inlet_pressure(ro_revpi, "aqualine_inlet_pressure");
remote_pressure_transducer remote_permeate_pressure(ro_revpi, "permeate_pressure");
remote_pressure_transducer remote_air_pressure(ro_revpi, "air_pressure");
remote_conductivity_sensor remote_permeate_conductivity(ro_revpi, "permeate_conductivity");
remote_conductivity_sensor remote_feed_conductivity(ro_revpi, "feed_conductivity");
remote_temperature_sensor remote_cip_temp(ro_revpi, "cip_temp");
remote_pump remote_feed_pump(ro_revpi, "feed_pump");
remote_pump remote_ro_pump(ro_revpi, "ro_pump");
remote_pump remote_repressure_pump(ro_revpi, "repressure_pump");
remote_switched_load remote_lights(ro_revpi, "lights");
remote_switched_load remote_cip_heater(ro_revpi, "cip_heater");
remote_pump remote_asa_pump(ro_revpi, "asa_pump");
remote_pump remote_high_ph_pump(ro_revpi, "high_ph_pump");
remote_pump remote_low_ph_pump(ro_revpi, "low_ph_pump");
remote_valve remote_flush_valve(ro_revpi, "flush_valve");
remote_valve remote_cip_inlet_valve(ro_revpi, "cip_inlet_valve");
remote_valve remote_inlet_valve(ro_revpi, "inlet_valve");
remote_valve remote_concentrate_cip_valve(ro_revpi, "concentrate_cip_valve");
remote_valve remote_media_backwash_1_valve(ro_revpi, "media_backwash_1_valve");
remote_valve remote_permeate_cip_valve(ro_revpi, "permeate_cip_valve");
remote_valve remote_media_backwash_2_valve(ro_revpi, "media_backwash_2_valve");

}  // namespace

// Public devices declared in config.h
float_switch& upper_tank_high_float = remote_upper_tank_high_float;
level_sensor& upper_tank_water_level = remote_upper_tank_water_level;

flow_sensor& permeate_flow = remote_permeate_flow;
flow_sensor& concentrate_flow = remote_concentrate_flow;
flow_sensor& feed_flow = remote_feed_flow;
float_switch& feed_tank_high_float = remote_feed_tank_high_float;
float_switch& high_ph_tank_float = remote_high_ph_tank_float;
float_switch& cip_tank_mid_float = remote_cip_tank_mid_float;
float_switch& cip_tank_low_float = remote_cip_tank_low_float;
float_switch& cip_tank_high_float = remote_cip_tank_high_float;
float_switch& low_ph_tank_float = remote_low_ph_tank_float;
float_switch& asa_tank_low_float = remote_asa_tank_low_float;
float_switch& permeate_tank_high_float = remote_permeate_tank_high_float;
float_switch& feed_tank_low_float = remote_feed_tank_low_float;
pressure_transducer& media_outlet_pressure = remote_media_outlet_pressure;
pressure_transducer& media_inlet_pressure = remote_media_inlet_pressure;
pressure_transducer& concentrate_pressure = remote_concentrate_pressure;
pressure_transducer& cip_inlet_pressure = remote_cip_inlet_pressure;
pressure_transducer& interstage_pressure = remote_interstage_pressure;
pressure_transducer& aqualine_outlet_pressure = remote_aqualine_outlet_pressure;
pressure_transducer& cip_outlet_pressure = remote_cip_outlet_pressure;
pressure_transducer& primary_pressure = remote_primary_pressure;
pressure_transducer& aqualine_inlet_pressure = remote_aqualine_inlet_pressure;
pressure_transducer& permeate_pressure = remote_permeate_pressure;
pressure_transducer& air_pressure = remote_air_pressure;
conductivity_sensor& permeate_conductivity = remote_permeate_conductivity;
conductivity_sensor& feed_conductivity = remote_feed_conductivity;
temperature_sensor& cip_temp = remote_cip_temp;
pump& feed_pump = remote_feed_pump;
pump& ro_pump = remote_ro_pump;
pump& repressure_pump = remote_repressure_pump;
switched_load& lights = remote_lights;
switched_load& cip_heater = remote_cip_heater;
pump& asa_pump = remote_asa_pump;
pump& high_ph_pump = remote_high_ph_pump;
pump& low_ph_pump = remote_low_ph_pump;
valve& flush_valve = remote_flush_valve;
valve& cip_inlet_valve = remote_cip_inlet_valve;
valve& inlet_valve = remote_inlet_valve;
valve& concentrate_cip_valve = remote_concentrate_cip_valve;
valve& media_backwash_1_valve = remote_media_backwash_1_valve;
valve& permeate_cip_valve = remote_permeate_cip_valve;
valve& media_backwash_2_valve = remote_media_backwash_2_valve;

// Nothing wired to this machine
bool attachAllInterfaces() {
    return true;
}

// Nothing to share, so no server is opened
bool startDeviceServer() {
    return true;
}

// Still sleeps so the main loop paces the same as on a RevPi
void serviceDeviceServer(int timeout_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}
