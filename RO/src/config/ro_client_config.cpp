#include "ro_config.h"

// Client RO config (ro_client, linked by ro_console): the ro_config.h functions go
// to the RO RevPi through remote_ro on ro_revpi, the connection
// defined in API/src/config/client_config.cpp (127.0.0.1 on the RO RevPi itself).
// Same as ro_config_host_pc.cpp; nothing is served and no controller runs here.

#include "remote_link.h"
#include "remote_ro/remote_ro.h"

// The RO RevPi connection defined in client_config.cpp, shared with its devices
extern remote_link ro_revpi;

namespace {

remote_ro ro_plant(ro_revpi);

}  // namespace

// Nothing to share: the controller isn't here
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
