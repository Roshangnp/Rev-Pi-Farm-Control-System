#pragma once

#include "procedure.h"

// Every farm procedure instance, like config.h for the devices and ro_config.h
// for the RO: every machine includes this header; each machine's implementation
// (procedure/src/config/procedure_config_<DEVICE_CONFIG>.cpp, or
// procedure_client_config.cpp for the console) binds each instance. One owner per
// instance: on its machine it's a procedure on main's scheduler, served by a
// procedure_server under the same name; everywhere else a remote_procedure on the
// connection to the owner. A program on a machine that doesn't bind an instance
// fails to link.

class line_server;  // comms/src/line_server.h
class scheduler;    // scheduler/include/scheduler.h

// Creates this machine's own instances on sched (main's one scheduler, which
// must outlive them, e.g. a global in main_*) and serves them on server. Call
// once at startup, after addRO() and before server.start(), and before using an
// instance this machine owns (that aborts). False if a name is already taken on
// the server. A machine that owns none: returns true.
bool addProcedures(line_server& server, scheduler& sched);

// The RO procedures (CIP cycles, media backwash, empty feed tank; only those
// types). Owner: the RO RevPi (procedure_config_ro.cpp), served as
// "ro_procedure"; main_ro sets the RO OFF when it stops running (stopped or
// finished)
extern procedure_control& ro_procedure;
