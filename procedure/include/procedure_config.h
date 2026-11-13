#pragma once

#include "procedure.h"

// Every farm procedure instance, like config.h for the devices: every machine
// includes this header; each machine's procedure config
// (procedure/src/config/procedure_config_<DEVICE_CONFIG>.cpp, or
// procedure_client_config.cpp for the console) binds each one: a procedure on
// the machine that owns it (on main's global scheduler, sched), a
// remote_procedure everywhere else. A program on a machine that doesn't bind an
// instance fails to link.

class line_server;  // comms/src/line_server.h

// Shares this machine's own procedures with the other machines: registers each
// one under its name on server (owned by main). Call once at startup, before
// server.start(). False if a name is already taken on the server.
bool addLocalProcedures(line_server& server);

// The RO procedures (CIP cycles, media backwash, empty feed tank). Owner: the RO RevPi, served as "ro_procedure"; a user stop sets the
// RO OFF (finished procedures leave the RO as their last step left it)
extern procedure_control& ro_procedure;

// The WiseConn procedures, one per set of valves. Owner: emerald-server
// (procedure_config_host_pc.cpp), served under the same names. Stopping a pattern
// leaves its bookings; its cancel procedure cancels them (one zone every 2 s, so
// it can run while the pattern still counts as running)
extern procedure_control& overhead_pattern_procedure;     // overhead_1..overhead_6 (OVERHEAD_PATTERN_1)
extern procedure_control& irrigation_pattern_procedure;   // valve_1_1..valve_10_3 (IRRIGATION_PATTERN_1)
extern procedure_control& overhead_cancel_procedure;      // CANCEL_OVERHEAD_PATTERN_1
extern procedure_control& irrigation_cancel_procedure;    // CANCEL_IRRIGATION_PATTERN_1
extern procedure_control& city_fill_valve_procedure;      // city_water_fill_valve; a user stop cancels its booking

// The repressure pump control (REPRESSURE_CONTROL). Owner: the RO RevPi
// (procedure_config_ro.cpp), served as "repressure_procedure"; main_ro starts it
// at startup; a user stop switches the repressure pump off
extern procedure_control& repressure_procedure;
