#pragma once

#include "devices.h"

inline constexpr int FLOAT_NUM = 10;
inline constexpr int PRESSURE_TRANS_NUM = 12;
inline constexpr int TDS_NUM = 2;
inline constexpr int PNUEMATIC_SOLENOID_NUM = 9;
inline constexpr int PNUEMATIC_VALVE_NUM = 5;

// Every device on the farm. Each machine's config (API/src/config/config_<name>.cpp,
// chosen with cmake -DDEVICE_CONFIG=<name>) binds the devices it uses to a local_*
// or remote_* device; callers only see the base class. A program that uses a device
// its config doesn't bind fails to link.

// Test bench (config_test)
extern pump& feed_pump;
extern float_switch& feed_tank_float;
extern valve& inlet_valve;
extern pressure_transducer& feed_pressure;
extern tds_sensor& permeate_tds;
extern flow_sensor& permeate_flow;

// Upper tank RevPi (config_uppertank)
extern float_switch& upper_tank_high_float;
extern level_sensor& upper_tank_water_level;

// Call once at startup, before using any device. False if any pin is missing
// from the PiCtory config or doesn't match its interface type.
bool attachAllInterfaces();

// Shares this RevPi's own devices with the other RevPis. Call startDeviceServer()
// once at startup, then serviceDeviceServer() every control cycle: remote
// requests are only handled inside it, and it waits up to timeout_ms for them,
// so it can replace the loop's sleep.
bool startDeviceServer();
void serviceDeviceServer(int timeout_ms);
