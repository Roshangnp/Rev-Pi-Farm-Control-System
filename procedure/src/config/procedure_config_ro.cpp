#include "procedure_config.h"

#include "line_server.h"
#include "scheduler.h"
#include "remote_procedure/procedure_server.h"

// ---------------------------------------------------------------------------
// RO RevPi (192.168.1.20): the procedures this machine owns, run on main's
// scheduler (sched, a global in main_ro.cpp).
// ---------------------------------------------------------------------------

extern scheduler sched;

namespace {

// The RO procedures; only the RO types may be started on it
procedure local_ro_procedure(sched);

// Shares the procedures owned here with the other machines
procedure_server ro_procedure_server(local_ro_procedure, "ro_procedure",
    {procedure_types::LOW_PH_CIP_CYCLE, procedure_types::HIGH_PH_CIP_CYCLE,
     procedure_types::MEDIA_BACKWASH_CYCLE, procedure_types::EMPTY_FEED_TANK});

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& ro_procedure = local_ro_procedure;

bool addLocalProcedures(line_server& server) {
    return ro_procedure_server.registerWith(server);
}
