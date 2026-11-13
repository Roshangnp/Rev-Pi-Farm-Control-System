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

// The WiseConn procedures, one per set of valves. A user stop cancels the
// irrigations each of its valves still has booked (cancelIrrigation: only ones
// not yet sent to the field; valves with nothing booked from here are skipped)

// Overheads (OVERHEAD_PATTERN_1)
procedure local_overhead_pattern_procedure(sched, [] {
    bool ok = true;
    for (irrigation* overhead : {&overhead_1, &overhead_2, &overhead_3, &overhead_4, &overhead_5, &overhead_6}) {
        int id;
        if (overhead->readScheduledIrrigation(id)) ok = overhead->cancelIrrigation() && ok;
    }
    return ok;
});

// Irrigation valves
procedure local_irrigation_pattern_procedure(sched, [] {
    bool ok = true;
    for (irrigation* valve : {&valve_1_1, &valve_1_2, &valve_2_1, &valve_3_1, &valve_4_1, &valve_5_1,
                              &valve_5_2, &valve_5_3, &valve_5_4, &valve_6_1, &valve_6_2, &valve_7_1,
                              &valve_8_1, &valve_9_1, &valve_10_1, &valve_10_2, &valve_10_3}) {
        int id;
        if (valve->readScheduledIrrigation(id)) ok = valve->cancelIrrigation() && ok;
    }
    return ok;
});

// City water fill valve
procedure local_city_fill_valve_procedure(sched, [] {
    int id;
    return !city_water_fill_valve.readScheduledIrrigation(id) || city_water_fill_valve.cancelIrrigation();
});

// Shares the procedures owned here
procedure_server overhead_pattern_procedure_server(local_overhead_pattern_procedure, "overhead_pattern_procedure");
procedure_server irrigation_pattern_procedure_server(local_irrigation_pattern_procedure, "irrigation_pattern_procedure");
procedure_server city_fill_valve_procedure_server(local_city_fill_valve_procedure, "city_fill_valve_procedure");

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");
remote_procedure remote_repressure_procedure(ro_revpi, "repressure_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& ro_procedure = remote_ro_procedure;
procedure_control& repressure_procedure = remote_repressure_procedure;
procedure_control& overhead_pattern_procedure = local_overhead_pattern_procedure;
procedure_control& irrigation_pattern_procedure = local_irrigation_pattern_procedure;
procedure_control& city_fill_valve_procedure = local_city_fill_valve_procedure;

bool addLocalProcedures(line_server& server) {
    return overhead_pattern_procedure_server.registerWith(server) &&
           irrigation_pattern_procedure_server.registerWith(server) &&
           city_fill_valve_procedure_server.registerWith(server);
}
