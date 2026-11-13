#include "procedure_config.h"

#include "line_server.h"
#include "remote_link.h"
#include "scheduler.h"
#include "remote_procedure/procedure_server.h"
#include "remote_procedure/remote_procedure.h"

// ---------------------------------------------------------------------------
// Host PC (emerald-server): owns irrigation_procedure, run on main's scheduler
// (sched, a global in main_host_pc.cpp); ro_procedure is a remote_procedure on
// the connection to the RO RevPi.
// ---------------------------------------------------------------------------

extern scheduler sched;
extern remote_link ro_revpi;   // config_host_pc.cpp

namespace {

// The irrigation procedures (OVERHEAD_PATTERN_1: WiseConn irrigations)
procedure local_irrigation_procedure(sched);

// Shares the procedures owned here
procedure_server irrigation_procedure_server(local_irrigation_procedure, "irrigation_procedure");

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& ro_procedure = remote_ro_procedure;
procedure_control& irrigation_procedure = local_irrigation_procedure;

bool addLocalProcedures(line_server& server) {
    return irrigation_procedure_server.registerWith(server);
}
