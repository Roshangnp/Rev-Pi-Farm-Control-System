#include "procedure_config.h"

#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"

// ---------------------------------------------------------------------------
// Upper tank RevPi (192.168.1.21): owns no procedures; each is a remote_procedure
// on the connection to its owner.
// ---------------------------------------------------------------------------

extern remote_link ro_revpi;   // config_uppertank.cpp

namespace {

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");

}  // namespace

// Public procedures declared in procedure_config.h
procedure_control& ro_procedure = remote_ro_procedure;

// Nothing owned here, so nothing to share
bool addLocalProcedures(line_server&) {
    return true;
}
