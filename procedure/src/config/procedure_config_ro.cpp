#include "procedure_config.h"

#include "line_server.h"
#include "remote_link.h"
#include "ro_config.h"
#include "scheduler.h"
#include "remote_procedure/procedure_server.h"
#include "remote_procedure/remote_procedure.h"

// ---------------------------------------------------------------------------
// RO RevPi (192.168.1.20): the procedures this machine owns, run on main's
// scheduler (sched, a global in main_ro.cpp).
// ---------------------------------------------------------------------------

extern scheduler sched;

namespace {

// The RO procedures; the RO goes OFF when a user stops one
procedure local_ro_procedure(sched, [] { return setROState(RO_state::OFF); });

// Shares the procedures owned here with the other machines
procedure_server ro_procedure_server(local_ro_procedure, "ro_procedure");

// irrigation_procedure is owned by emerald-server (procedure_config_host_pc.cpp)
remote_link host_pc("192.168.1.194");
remote_procedure remote_irrigation_procedure(host_pc, "irrigation_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& irrigation_procedure = remote_irrigation_procedure;
procedure_control& ro_procedure = local_ro_procedure;

bool addLocalProcedures(line_server& server) {
    return ro_procedure_server.registerWith(server);
}
