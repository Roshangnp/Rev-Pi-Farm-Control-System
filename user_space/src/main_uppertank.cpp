#include <cstdio>
#include "config.h"
#include "line_server.h"
#include "procedure_config.h"
#include "ro_config.h"
#include "scheduler.h"

// This machine's one scheduler (user space schedules tasks on it). A global, so
// it outlives anything that holds a reference to it.
scheduler sched;

// Upper tank RevPi: attaches its I/O and shares its devices, the WiseConn
// devices and the RO (passed on to the RO RevPi) with the other machines.
// No control logic yet.
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");

    line_server server;   // port 47100, drops clients idle 20 s
    if (!addLocalDevices(server)) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }
    if (!addRO(server, sched)) std::fprintf(stderr, "RO server failed to register\n");
    addProcedures(server, sched);   // owns none here (ro_procedure is the RO RevPi's)
    if (!server.start()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        sched.update();
        updateRO();
        server.poll(20);   // handles remote requests, doubles as the loop sleep
    }
}
