#pragma once

#include "procedure.h"   // procedure_types (procedure/include)
#include "types.h"

// The RO controller, like config.h for the API: every machine includes this
// header and calls the same functions; each machine's implementation
// (RO/src/config/ro_config_<DEVICE_CONFIG>.cpp) decides where the controller is.
//   ro                : the controller runs here (RO/src/ro.h)
//   host_pc, uppertank : the calls go to the RO RevPi (192.168.1.20) through remote_ro
// Every machine with an implementation serves these functions as "ro" and
// "ro_procedure" (RO/src/remote_ro/ro_server.h) once addRO() has run, so the RO
// is in the same space as the devices. test has no implementation: a program
// using these fails to link there.

class line_server;  // comms/src/line_server.h
class scheduler;    // scheduler/include/scheduler.h

// Every machine: registers "ro" and "ro_procedure" on server (ro_server, built
// on the functions below) so other machines and local clients can use the RO
// through this machine. Call once at startup, before server.start(). False if
// a name is already taken on the server.
// RO RevPi: also creates the controller on sched (the machine's one scheduler,
// owned by main: procedures are scheduled on it, main calls sched.update() every
// loop pass; it must outlive the controller, e.g. a global in main_ro.cpp) and
// drives the RO outputs to OFF (a known state after a restart: the relay board
// holds its last state). Call it after attachAllInterfaces() and before any
// other function here. False also if driving OFF failed (the reason is printed).
// Elsewhere sched is not used.
bool addRO(line_server& server, scheduler& sched);

// RO RevPi: call every loop pass, next to server.poll(): moves the RO to OFF
// when a procedure has just ended, and starts the pumps of the current state once
// the valves have settled. False if either failed. Elsewhere: returns true.
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

// Starts procedure p (e.g. procedure_types::LOW_PH_CIP_CYCLE) on the RO RevPi.
// Non-blocking: its steps run in the RO RevPi's scheduler update(). False if a
// procedure is still running, p isn't an RO procedure, or the RO RevPi can't be
// reached.
bool runROProcedure(procedure_types p);

// Cancels the running procedure's remaining steps; the RO then moves to OFF on the
// RO RevPi's next updateRO() (it goes OFF whenever a procedure ends). False if none
// is running or the RO RevPi can't be reached.
bool stopROProcedure();

// running = an RO procedure is running, p = which one (meaningful while running).
// False if the RO RevPi can't be reached.
bool readROProcedure(bool& running, procedure_types& p);
