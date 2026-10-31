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
extern pump& ro_pump;          // run relay + VFD alarm
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

// Web page listing this machine's devices (local and remote) with their values,
// and controls for outputs. Call startWebUI() once at startup, then
// serviceWebUI() every control cycle: like the device server, requests are only
// handled inside it. No authentication: restrict the port with the firewall.
// When both run, give one of them the sleep: serviceWebUI(0); serviceDeviceServer(20);
bool startWebUI();
void serviceWebUI(int timeout_ms);
