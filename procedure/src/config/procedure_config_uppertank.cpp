#include "procedure_config.h"

// Upper tank RevPi (192.168.1.21): owns no procedure instance. ro_procedure (owned by the RO RevPi) is a
// remote_procedure on ro_revpi, the RO RevPi connection,
// shared with its devices and the RO.

#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"

// The RO RevPi connection, defined in API/src/config/config_uppertank.cpp
extern remote_link ro_revpi;

namespace {

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");

}  // namespace

procedure_control& ro_procedure = remote_ro_procedure;

// Nothing to share: no instance is owned here
bool addProcedures(line_server&, scheduler&) {
    return true;
}
