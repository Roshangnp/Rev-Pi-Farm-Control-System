#pragma once

#include "types.h"

// The RO controller, like config.h for the API: every machine includes this
// header and calls the same functions; each machine's implementation
// (RO/src/config/ro_config_<DEVICE_CONFIG>.cpp) decides where the controller is.
//   ro      : the controller runs here (RO/src/ro.h)
//   host_pc : the calls go to the RO RevPi (192.168.1.20) through remote_ro
// Other configs have no implementation: a program using these fails to link.

class line_server;  // comms/src/line_server.h

// RO RevPi: creates the controller, drives the RO outputs to OFF (a known state
// after a restart: the relay board holds its last state), and registers it as
// "ro" on server so other machines can command it. Call once at startup, after
// attachAllInterfaces() and before server.start(). False if driving OFF failed
// (the reason is printed) or "ro" is already taken on the server.
// Host PC: nothing to share, returns true.
bool addRO(line_server& server);

// RO RevPi: call every loop pass, next to server.poll(): starts the pumps of the
// current state once the valves have settled. False only if starting a pump
// failed. Host PC: returns true.
bool updateRO();

// Move the RO to state s. Non-blocking: valves move at once, pumps start
// VALVE_SETTLE_MS later (in updateRO() on the RO RevPi). False if any device
// failed (the RO is in state s anyway) or the RO RevPi can't be reached.
bool setROState(RO_state s);

// The state last set (OFF after a restart). False if the RO RevPi can't be reached.
bool readROState(RO_state& s);

// True between setROState() and the pumps starting. False if the RO RevPi can't be reached.
bool readROSettling(bool& settling);

// RO pump VFD speed, 0-1 (4-20 mA). False if out of range or the RO RevPi can't be reached.
bool setROSpeed(float speed);
