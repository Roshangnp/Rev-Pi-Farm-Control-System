#include "procedure_config.h"

// Procedure instances owned by another machine, as remote_procedure on this
// machine's connection to the owner. Linked by programs that don't own them:
// main_host_pc, main_uppertank and the console (not on the RO RevPi, whose
// main_ro owns ro_procedure).

#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"

// The RO RevPi connection, defined by this machine's config (config_host_pc.cpp,
// config_uppertank.cpp, client_config.cpp) and shared with its devices
extern remote_link ro_revpi;

namespace {

remote_procedure remote_ro_procedure(ro_revpi, "ro_procedure");

}  // namespace

procedure_control& ro_procedure = remote_ro_procedure;
