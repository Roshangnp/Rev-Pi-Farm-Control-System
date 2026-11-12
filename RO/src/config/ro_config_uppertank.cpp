#include "ro_config.h"

// Upper tank RevPi (192.168.1.21): the RO controller runs on the RO RevPi;
// state calls go to its ro_server through remote_ro, procedure calls to its
// procedure_server ("ro_procedure") through remote_procedure. Same as
// ro_config_host_pc.cpp. main_uppertank doesn't use the RO; this gives its
// user space the same RO access as every other machine

#include "remote_link.h"
#include "remote_procedure/remote_procedure.h"
#include "remote_ro/remote_ro.h"

// The RO RevPi connection defined in config_uppertank.cpp, shared with its
// devices (one snapshot poll for both). Only a reference is bound here, so the
// order the two files' globals are built in doesn't matter
extern remote_link ro_revpi;

namespace {

remote_ro ro_plant(ro_revpi);
remote_procedure ro_procedures(ro_revpi, "ro_procedure");

}  // namespace

// Nothing to share: the controller isn't here (the RO RevPi serves it)
bool addRO(line_server&, scheduler&) {
    return true;
}

// Nothing to run: the RO RevPi's own loop starts its pumps
bool updateRO() {
    return true;
}

bool setROState(RO_state s) {
    return ro_plant.setState(s);
}

bool readROState(RO_state& s) {
    return ro_plant.readState(s);
}

bool readROSettling(bool& settling) {
    return ro_plant.isSettling(settling);
}

bool setROSpeed(float speed) {
    return ro_plant.setRoSpeed(speed);
}

bool runROProcedure(procedure_types p) {
    return ro_procedures.run(p);
}

bool stopROProcedure() {
    return ro_procedures.stop();
}

bool readROProcedure(bool& running, procedure_types& p) {
    return ro_procedures.readProcedure(running, p);
}
