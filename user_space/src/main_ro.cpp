#include <cstdio>
#include "config.h"
#include "line_server.h"
#include "procedure_config.h"
#include "ro_config.h"
#include "scheduler.h"

// This machine's one scheduler: procedures and user space schedule tasks on it.
scheduler sched;

// RO RevPi: attaches its I/O, shares its devices with the other machines, and
// runs the RO controller (state set remotely, e.g. from the web UI via remote_ro).
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");

    line_server server;   // port 47100, drops clients idle 20 s
    if (!addLocalDevices(server)) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }
    // Not fatal: the device server keeps running so the outputs can still be read and switched
    if (!addRO(server, sched)) std::fprintf(stderr, "RO controller failed to start cleanly\n");
    if (!addLocalProcedures(server)) std::fprintf(stderr, "Procedure server failed to register\n");
    // The repressure pump control runs from startup (stop / run it from the console)
    if (!repressure_procedure.run(procedure_types::REPRESSURE_CONTROL)) std::fprintf(stderr, "Repressure procedure failed to start\n");
    if (!server.start()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        // Rolling flow averages: readFlow() only returns what these compute
        permeate_flow.update();
        concentrate_flow.update();
        feed_flow.update();

        if (!sched.update()) std::fprintf(stderr, "Scheduler: a task failed\n");   // procedure steps
        if (!updateRO()) std::fprintf(stderr, "RO controller: starting a pump failed\n");
        server.poll(20);   // handles remote requests, doubles as the loop sleep
    }
}
