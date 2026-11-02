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
#include "devices/wiseconn_devices/wiseconn_keys.h"
#include "devices/wiseconn_devices/wiseconn_link.h"
#include "devices/wiseconn_devices/wiseconn_soil_sensor.h"
#include "devices/wiseconn_devices/wiseconn_irrigation.h"
#include "wiseconn_keys_text.h"

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

// WiseConn cloud, from wiseconn_keys.txt (built in by CMake). Defined in this
// order on purpose: the keys are parsed before the link and sensors use them.
const wiseconn_keys wiseconn_config = loadWiseconnKeys(WISECONN_KEYS_TEXT);
wiseconn_link wiseconn(wiseconn_config);
wiseconn_soil_sensor wiseconn_zone_7_soil(wiseconn, wiseconn_config.soil("zone_7_soil"));
wiseconn_soil_sensor wiseconn_zone_8_soil(wiseconn, wiseconn_config.soil("zone_8_soil"));
wiseconn_soil_sensor wiseconn_zone_10_3_soil(wiseconn, wiseconn_config.soil("zone_10_3_soil"));
// Irrigation zones: on/off from the WiseConn hydraulics list
wiseconn_irrigation wiseconn_valve_1_1(wiseconn, "valve_1_1", wiseconn_config.irrigation("valve_1_1"));
wiseconn_irrigation wiseconn_valve_1_2(wiseconn, "valve_1_2", wiseconn_config.irrigation("valve_1_2"));
wiseconn_irrigation wiseconn_valve_2_1(wiseconn, "valve_2_1", wiseconn_config.irrigation("valve_2_1"));
wiseconn_irrigation wiseconn_valve_3_1(wiseconn, "valve_3_1", wiseconn_config.irrigation("valve_3_1"));
wiseconn_irrigation wiseconn_valve_4_1(wiseconn, "valve_4_1", wiseconn_config.irrigation("valve_4_1"));
wiseconn_irrigation wiseconn_valve_5_1(wiseconn, "valve_5_1", wiseconn_config.irrigation("valve_5_1"));
wiseconn_irrigation wiseconn_valve_5_2(wiseconn, "valve_5_2", wiseconn_config.irrigation("valve_5_2"));
wiseconn_irrigation wiseconn_valve_5_3(wiseconn, "valve_5_3", wiseconn_config.irrigation("valve_5_3"));
wiseconn_irrigation wiseconn_valve_5_4(wiseconn, "valve_5_4", wiseconn_config.irrigation("valve_5_4"));
wiseconn_irrigation wiseconn_valve_6_1(wiseconn, "valve_6_1", wiseconn_config.irrigation("valve_6_1"));
wiseconn_irrigation wiseconn_valve_6_2(wiseconn, "valve_6_2", wiseconn_config.irrigation("valve_6_2"));
wiseconn_irrigation wiseconn_valve_7_1(wiseconn, "valve_7_1", wiseconn_config.irrigation("valve_7_1"));
wiseconn_irrigation wiseconn_valve_8_1(wiseconn, "valve_8_1", wiseconn_config.irrigation("valve_8_1"));
wiseconn_irrigation wiseconn_valve_9_1(wiseconn, "valve_9_1", wiseconn_config.irrigation("valve_9_1"));
wiseconn_irrigation wiseconn_valve_10_1(wiseconn, "valve_10_1", wiseconn_config.irrigation("valve_10_1"));
wiseconn_irrigation wiseconn_valve_10_2(wiseconn, "valve_10_2", wiseconn_config.irrigation("valve_10_2"));
wiseconn_irrigation wiseconn_valve_10_3(wiseconn, "valve_10_3", wiseconn_config.irrigation("valve_10_3"));
wiseconn_irrigation wiseconn_overhead_1(wiseconn, "overhead_1", wiseconn_config.irrigation("overhead_1"));
wiseconn_irrigation wiseconn_overhead_2(wiseconn, "overhead_2", wiseconn_config.irrigation("overhead_2"));
wiseconn_irrigation wiseconn_overhead_3(wiseconn, "overhead_3", wiseconn_config.irrigation("overhead_3"));
wiseconn_irrigation wiseconn_overhead_4(wiseconn, "overhead_4", wiseconn_config.irrigation("overhead_4"));
wiseconn_irrigation wiseconn_overhead_5(wiseconn, "overhead_5", wiseconn_config.irrigation("overhead_5"));
wiseconn_irrigation wiseconn_overhead_6(wiseconn, "overhead_6", wiseconn_config.irrigation("overhead_6"));
wiseconn_irrigation wiseconn_city_water_fill_valve(wiseconn, "city_water_fill_valve", wiseconn_config.irrigation("city_water_fill_valve"));

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
soil_sensor& zone_7_soil = wiseconn_zone_7_soil;
soil_sensor& zone_8_soil = wiseconn_zone_8_soil;
soil_sensor& zone_10_3_soil = wiseconn_zone_10_3_soil;
irrigation& valve_1_1 = wiseconn_valve_1_1;
irrigation& valve_1_2 = wiseconn_valve_1_2;
irrigation& valve_2_1 = wiseconn_valve_2_1;
irrigation& valve_3_1 = wiseconn_valve_3_1;
irrigation& valve_4_1 = wiseconn_valve_4_1;
irrigation& valve_5_1 = wiseconn_valve_5_1;
irrigation& valve_5_2 = wiseconn_valve_5_2;
irrigation& valve_5_3 = wiseconn_valve_5_3;
irrigation& valve_5_4 = wiseconn_valve_5_4;
irrigation& valve_6_1 = wiseconn_valve_6_1;
irrigation& valve_6_2 = wiseconn_valve_6_2;
irrigation& valve_7_1 = wiseconn_valve_7_1;
irrigation& valve_8_1 = wiseconn_valve_8_1;
irrigation& valve_9_1 = wiseconn_valve_9_1;
irrigation& valve_10_1 = wiseconn_valve_10_1;
irrigation& valve_10_2 = wiseconn_valve_10_2;
irrigation& valve_10_3 = wiseconn_valve_10_3;
irrigation& overhead_1 = wiseconn_overhead_1;
irrigation& overhead_2 = wiseconn_overhead_2;
irrigation& overhead_3 = wiseconn_overhead_3;
irrigation& overhead_4 = wiseconn_overhead_4;
irrigation& overhead_5 = wiseconn_overhead_5;
irrigation& overhead_6 = wiseconn_overhead_6;
irrigation& city_water_fill_valve = wiseconn_city_water_fill_valve;

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
