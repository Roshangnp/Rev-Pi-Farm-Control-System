#include "ro_config.h"

// Host PC (emerald-server, dev PC): the RO controller runs on the RO RevPi;
// every call goes to its ro_server through remote_ro

#include "remote_link.h"
#include "remote_ro.h"

namespace {

// Own connection to the RO RevPi (config_host_pc has another one for its
// devices). Connects in the background as soon as the program starts
remote_link ro_revpi("192.168.1.20");
remote_ro ro_plant(ro_revpi);

}  // namespace

// Nothing to share: the controller isn't here
bool addRO(line_server&) {
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
