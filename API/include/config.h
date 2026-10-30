#pragma once

#include "devices.h"

inline constexpr int FLOAT_NUM = 10;
inline constexpr int PRESSURE_TRANS_NUM = 12;
inline constexpr int TDS_NUM = 2;
inline constexpr int PNUEMATIC_SOLENOID_NUM = 9;
inline constexpr int PNUEMATIC_VALVE_NUM = 5;

// Devices available to this RevPi. Whether each one is wired here or owned by
// another RevPi is decided in API/src/config.cpp; callers only see the base class.
extern pump& feed_pump;
extern float_switch& feed_tank_float;
extern valve& inlet_valve;
extern pressure_transducer& feed_pressure;
extern tds_sensor& permeate_tds;
extern flow_sensor& permeate_flow;

// Call once at startup, before using any device. False if any pin is missing
// from the PiCtory config or doesn't match its interface type.
bool attachAllInterfaces();

// Shares this RevPi's own devices with the other RevPis. Call startDeviceServer()
// once at startup, then serviceDeviceServer() every control cycle: remote
// requests are only handled inside it, and it waits up to timeout_ms for them,
// so it can replace the loop's sleep.
bool startDeviceServer();
void serviceDeviceServer(int timeout_ms);
