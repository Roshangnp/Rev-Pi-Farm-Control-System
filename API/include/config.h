#pragma once

#include "devices.h"

// Every device on the farm. Each machine's config (API/src/config/config_<name>.cpp,
// chosen with cmake -DDEVICE_CONFIG=<name>) binds the devices it uses to a local_*
// or remote_* device; callers only see the base class. A program that uses a device
// its config doesn't bind fails to link.

// Upper tank RevPi, 192.168.1.195 (config_uppertank)
extern float_switch& upper_tank_high_float;
extern level_sensor& upper_tank_water_level;

// RO RevPi, 192.168.1.116 (config_ro)
extern flow_sensor& permeate_flow;
extern flow_sensor& concentrate_flow;
extern flow_sensor& feed_flow;

extern float_switch& feed_tank_high_float;
extern float_switch& feed_tank_low_float;
extern float_switch& high_ph_tank_float;
extern float_switch& low_ph_tank_float;
extern float_switch& cip_tank_low_float;
extern float_switch& cip_tank_mid_float;
extern float_switch& cip_tank_high_float;
extern float_switch& asa_tank_low_float;
extern float_switch& permeate_tank_high_float;

extern pressure_transducer& media_outlet_pressure;
extern pressure_transducer& media_inlet_pressure;
extern pressure_transducer& concentrate_pressure;
extern pressure_transducer& cip_inlet_pressure;
extern pressure_transducer& cip_outlet_pressure;
extern pressure_transducer& interstage_pressure;
extern pressure_transducer& aqualine_inlet_pressure;
extern pressure_transducer& aqualine_outlet_pressure;
extern pressure_transducer& primary_pressure;
extern pressure_transducer& permeate_pressure;
extern pressure_transducer& air_pressure;

extern conductivity_sensor& permeate_conductivity;
extern conductivity_sensor& feed_conductivity;
extern temperature_sensor& cip_temp;

extern pump& feed_pump;        // run relay + VFD alarm
extern pump& ro_pump;          // run relay + VFD alarm + 4-20 mA speed
extern pump& repressure_pump;
extern pump& asa_pump;
extern pump& high_ph_pump;
extern pump& low_ph_pump;

extern switched_load& lights;
extern switched_load& cip_heater;

extern valve& flush_valve;
extern valve& inlet_valve;
extern valve& cip_inlet_valve;
extern valve& concentrate_cip_valve;
extern valve& permeate_cip_valve;
extern valve& media_backwash_1_valve;
extern valve& media_backwash_2_valve;

// WiseConn cloud soil probes (host PC only, read only). Values per depth,
// 4 in / 8 in / 12 in; refreshed from the cloud every 60 s, measured every 15 min.
// Ids and the API key come from wiseconn_keys.txt.
extern soil_sensor& zone_7_soil;
extern soil_sensor& zone_8_soil;
extern soil_sensor& zone_10_3_soil;

// WiseConn irrigation zones (host PC only): readOn() is the zone's live valve
// status (refreshed every 60 s); scheduleIrrigation() creates a WiseConn
// irrigation, not enabled yet (always returns false). Zone ids come from
// wiseconn_keys.txt [irrigations].
extern irrigation& valve_1_1;
extern irrigation& valve_1_2;
extern irrigation& valve_2_1;
extern irrigation& valve_3_1;
extern irrigation& valve_4_1;
extern irrigation& valve_5_1;
extern irrigation& valve_5_2;
extern irrigation& valve_5_3;
extern irrigation& valve_5_4;
extern irrigation& valve_6_1;
extern irrigation& valve_6_2;
extern irrigation& valve_7_1;
extern irrigation& valve_8_1;
extern irrigation& valve_9_1;
extern irrigation& valve_10_1;
extern irrigation& valve_10_2;
extern irrigation& valve_10_3;
extern irrigation& overhead_1;
extern irrigation& overhead_2;
extern irrigation& overhead_3;
extern irrigation& overhead_4;
extern irrigation& overhead_5;
extern irrigation& overhead_6;
extern irrigation& city_water_fill_valve;

// Test bench only (config_test, also on 192.168.1.116). It binds feed_pump,
// inlet_valve and permeate_flow above to its example wiring, plus these:
extern float_switch& feed_tank_float;
extern pressure_transducer& feed_pressure;
extern tds_sensor& permeate_tds;

// Call once at startup, before using any device. False if any pin is missing
// from the PiCtory config or doesn't match its interface type.
bool attachAllInterfaces();

// Shares this RevPi's own devices with the other RevPis. Call startDeviceServer()
// once at startup, then serviceDeviceServer() every control cycle: remote
// requests are only handled inside it, and it waits up to timeout_ms for them,
// so it can replace the loop's sleep.
bool startDeviceServer();
void serviceDeviceServer(int timeout_ms);
