#include "procedure_config.h"

#include "config.h"
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

// The repressure pump control; the pump goes off when a user stops it
procedure local_repressure_procedure(sched, [] { return repressure_pump.setOff(); });

// Shares the procedures owned here with the other machines
procedure_server ro_procedure_server(local_ro_procedure, "ro_procedure");
procedure_server repressure_procedure_server(local_repressure_procedure, "repressure_procedure");

// The WiseConn procedures are owned by emerald-server (procedure_config_host_pc.cpp)
remote_link host_pc("192.168.1.194");
remote_procedure remote_overhead_pattern_procedure(host_pc, "overhead_pattern_procedure");
remote_procedure remote_irrigation_pattern_procedure(host_pc, "irrigation_pattern_procedure");
remote_procedure remote_city_fill_valve_procedure(host_pc, "city_fill_valve_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& overhead_pattern_procedure = remote_overhead_pattern_procedure;
procedure_control& irrigation_pattern_procedure = remote_irrigation_pattern_procedure;
procedure_control& city_fill_valve_procedure = remote_city_fill_valve_procedure;
procedure_control& ro_procedure = local_ro_procedure;
procedure_control& repressure_procedure = local_repressure_procedure;

bool addLocalProcedures(line_server& server) {
    return ro_procedure_server.registerWith(server) && repressure_procedure_server.registerWith(server);
}
