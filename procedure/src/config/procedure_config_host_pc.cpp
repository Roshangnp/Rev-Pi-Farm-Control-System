#include "procedure_config.h"

#include "config.h"
#include "line_server.h"
#include "remote_link.h"
#include "scheduler.h"
#include "remote_procedure/procedure_server.h"
#include "remote_procedure/remote_procedure.h"

// ---------------------------------------------------------------------------
// Host PC (emerald-server): owns the WiseConn procedures, run on main's
// scheduler (sched, a global in main_host_pc.cpp); ro_procedure and
// repressure_procedure are remote_procedures on the connection to the RO RevPi.
// ---------------------------------------------------------------------------

extern scheduler sched;
extern remote_link ro_revpi;   // config_host_pc.cpp

namespace {

// The WiseConn procedures, one per set of valves. Stopping a pattern does nothing
// to its bookings (cancelling them all in the stop blocked the loop for seconds,
// past the console's command timeout, and hit WiseConn's rate limit); the cancel
// procedures cancel them, one zone every 2 s

// Overheads (OVERHEAD_PATTERN_1) and their cancel (CANCEL_OVERHEAD_PATTERN_1)
procedure local_overhead_pattern_procedure(sched, [] { return true; });
procedure local_overhead_cancel_procedure(sched);

// Irrigation valves (IRRIGATION_PATTERN_1) and their cancel (CANCEL_IRRIGATION_PATTERN_1)
procedure local_irrigation_pattern_procedure(sched, [] { return true; });
procedure local_irrigation_cancel_procedure(sched);

// City water fill valve: a user stop cancels its booking (one zone, one request)
procedure local_city_fill_valve_procedure(sched, [] {
    int id;
    return !city_water_fill_valve.readScheduledIrrigation(id) || city_water_fill_valve.cancelIrrigation();
});

// Shares the procedures owned here
procedure_server overhead_pattern_procedure_server(local_overhead_pattern_procedure, "overhead_pattern_procedure");
procedure_server irrigation_pattern_procedure_server(local_irrigation_pattern_procedure, "irrigation_pattern_procedure");
procedure_server overhead_cancel_procedure_server(local_overhead_cancel_procedure, "overhead_cancel_procedure");
procedure_server irrigation_cancel_procedure_server(local_irrigation_cancel_procedure, "irrigation_cancel_procedure");
procedure_server city_fill_valve_procedure_server(local_city_fill_valve_procedure, "city_fill_valve_procedure");

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");
remote_procedure remote_repressure_procedure(ro_revpi, "repressure_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& ro_procedure = remote_ro_procedure;
procedure_control& repressure_procedure = remote_repressure_procedure;
procedure_control& overhead_pattern_procedure = local_overhead_pattern_procedure;
procedure_control& irrigation_pattern_procedure = local_irrigation_pattern_procedure;
procedure_control& overhead_cancel_procedure = local_overhead_cancel_procedure;
procedure_control& irrigation_cancel_procedure = local_irrigation_cancel_procedure;
procedure_control& city_fill_valve_procedure = local_city_fill_valve_procedure;

bool addLocalProcedures(line_server& server) {
    return overhead_pattern_procedure_server.registerWith(server) &&
           irrigation_pattern_procedure_server.registerWith(server) &&
           overhead_cancel_procedure_server.registerWith(server) &&
           irrigation_cancel_procedure_server.registerWith(server) &&
           city_fill_valve_procedure_server.registerWith(server);
}
