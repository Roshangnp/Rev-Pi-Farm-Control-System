#include "ro_config.h"

// RO RevPi (192.168.1.20): the RO controller runs here, served to the other
// machines as "ro" and "ro_procedure" on main's line_server (ro_server)

#include <cstdio>
#include <cstdlib>

#include "line_server.h"
#include "ro.h"
#include "scheduler.h"
#include "remote_ro/ro_server.h"

namespace {

// main's scheduler, set by addRO() before the controller is created
scheduler* main_scheduler = nullptr;

// Function-local statics: created on the first addRO() call, after the device
// objects in config_ro.cpp and main's scheduler, so they are destroyed before
// them (ro's destructor still switches real devices, and its procedure removes
// its tasks from the scheduler)
ro& plant() {
    if (!main_scheduler) {
        std::fprintf(stderr, "RO controller: used before addRO()\n");
        std::abort();
    }
    static ro instance(*main_scheduler);
    return instance;
}

// Serves the functions below as "ro" and "ro_procedure"
ro_server& handler() {
    static ro_server instance;
    return instance;
}

// Only the RO procedures may be started on the RO's procedure instance (not
// e.g. a future repressure one)
bool isROProcedure(procedure_types p) {
    switch (p) {
        case procedure_types::LOW_PH_CIP_CYCLE:
        case procedure_types::HIGH_PH_CIP_CYCLE:
        case procedure_types::MEDIA_BACKWASH_CYCLE:
        case procedure_types::EMPTY_FEED_TANK:
            return true;
    }
    return false;
}

}  // namespace

bool addRO(line_server& server, scheduler& sched) {
    main_scheduler = &sched;
    bool ok = plant().setState(RO_state::OFF);
    if (!ok) std::fprintf(stderr, "RO controller: driving the outputs to OFF failed\n");
    if (!handler().registerWith(server)) {
        std::fprintf(stderr, "RO controller: name \"ro\" or \"ro_procedure\" is already taken on the server\n");
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

bool runROProcedure(procedure_types p) {
    if (!isROProcedure(p)) {
        std::fprintf(stderr, "RO controller: %s is not an RO procedure\n", toString(p));
        return false;
    }
    return plant().procedures().run(p);
}

// The RO goes OFF on the next updateRO() (ro::update sees the procedure end)
bool stopROProcedure() {
    procedure& p = plant().procedures();
    if (!p.isRunning()) {
        std::fprintf(stderr, "RO controller: no procedure is running\n");
        return false;
    }
    return p.stop();
}

bool readROProcedure(bool& running, procedure_types& p) {
    procedure& proc = plant().procedures();
    running = proc.isRunning();
    p = proc.type();
    return true;
}
