#include "procedure_config.h"

#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"

// ---------------------------------------------------------------------------
// Client config (procedure_client, linked by ro_console): owns no procedures; each is a remote_procedure
// on the connection to its owner.
// ---------------------------------------------------------------------------

extern remote_link ro_revpi;   // client_config.cpp (127.0.0.1 on the RO RevPi)

namespace {

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");
remote_procedure remote_repressure_procedure(ro_revpi, "repressure_procedure");

// The WiseConn procedures are owned by emerald-server: 127.0.0.1 when built for it,
// else its LAN address (FARM_HOST_PC_HOST, set by procedure.cmake)
remote_link host_pc(FARM_HOST_PC_HOST);
remote_procedure remote_overhead_pattern_procedure(host_pc, "overhead_pattern_procedure");
remote_procedure remote_irrigation_pattern_procedure(host_pc, "irrigation_pattern_procedure");
remote_procedure remote_city_fill_valve_procedure(host_pc, "city_fill_valve_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& overhead_pattern_procedure = remote_overhead_pattern_procedure;
procedure_control& irrigation_pattern_procedure = remote_irrigation_pattern_procedure;
procedure_control& city_fill_valve_procedure = remote_city_fill_valve_procedure;
procedure_control& ro_procedure = remote_ro_procedure;
procedure_control& repressure_procedure = remote_repressure_procedure;

// Nothing owned here, so nothing to share
bool addLocalProcedures(line_server&) {
    return true;
}
