#include "ro_config.h"

// RO RevPi (192.168.1.20): the RO controller runs here, served to the other
// machines as "ro" on main's line_server

#include <cstdio>

#include "line_server.h"
#include "ro.h"
#include "ro_server.h"

namespace {

// Function-local statics: created on the first addRO() call, after the device
// objects in config_ro.cpp, so they are destroyed before them (ro's destructor
// still switches real devices)
ro& plant() {
    static ro instance;
    return instance;
}

ro_server& handler() {
    static ro_server instance(plant());
    return instance;
}

}  // namespace

bool addRO(line_server& server) {
    bool ok = plant().setState(RO_state::OFF);
    if (!ok) std::fprintf(stderr, "RO controller: driving the outputs to OFF failed\n");
    if (!handler().registerWith(server)) {
        std::fprintf(stderr, "RO controller: name \"ro\" is already taken on the server\n");
        ok = false;
    }
    return ok;
}

bool updateRO() {
    return plant().update();
}

bool setROState(RO_state s) {
    return plant().setState(s);
}

bool readROState(RO_state& s) {
    s = plant().readState();
    return true;
}

bool readROSettling(bool& settling) {
    settling = plant().isSettling();
    return true;
}

bool setROSpeed(float speed) {
    return plant().setRoSpeed(speed);
}
