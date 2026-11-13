#include "procedure_config.h"

#include "config.h"
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

// The irrigation procedures (OVERHEAD_PATTERN_1: WiseConn irrigations). A user stop
// cancels the irrigation each overhead still has booked (cancelIrrigation: only one
// not yet sent to the field; zones with nothing booked from here are skipped)
procedure local_irrigation_procedure(sched, [] {
    bool ok = true;
    for (irrigation* overhead : {&overhead_1, &overhead_2, &overhead_3, &overhead_4, &overhead_5, &overhead_6}) {
        int id;
        if (overhead->readScheduledIrrigation(id)) ok = overhead->cancelIrrigation() && ok;
    }
    return ok;
});

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
