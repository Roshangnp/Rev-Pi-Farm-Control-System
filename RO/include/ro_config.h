#pragma once

#include "types.h"

// The RO controller, like config.h for the API: every machine includes this
// header and calls the same functions; each machine's implementation
// (RO/src/config/ro_config_<DEVICE_CONFIG>.cpp) decides where the controller is.
//   ro                : the controller runs here (RO/src/ro.h)
//   host_pc, uppertank : the calls go to the RO RevPi (192.168.1.20) through remote_ro
// The RO RevPi serves these functions as "ro" (RO/src/remote_ro/ro_server.h)
// once addRO() has run; every other machine reaches them through remote_ro.
// test has no implementation: a program using these fails to link there.
// The RO procedures are not part of the RO: main_ro owns that procedure
// instance (procedure/include/procedure_config.h, ro_procedure).

class line_server;  // comms/src/line_server.h
class scheduler;    // scheduler/include/scheduler.h

// RO RevPi: creates the controller, drives the RO outputs to OFF (a known state
// after a restart: the relay board holds its last state) and registers it as
// "ro" on server (ro_server, built on the functions below) so other machines can
// use it. Call it after attachAllInterfaces(), before any other function here
// and before server.start(). False if driving OFF failed (the reason is
// printed) or "ro" is already taken on the server.
// Elsewhere: nothing to share, returns true.
// sched is not used (kept so existing programs, e.g. tests/, still build).
bool addRO(line_server& server, scheduler& sched);

// RO RevPi: call every loop pass, next to server.poll(): starts the pumps of the
// current state once the valves have settled. False if that failed.
// Elsewhere: returns true.
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
