#include <cstdio>
#include "config.h"
#include "line_server.h"
#include "ro_config.h"
#include "scheduler.h"

// This machine's one scheduler: procedures (ro) and user space schedule tasks on
// it. Global so it outlives the RO controller (a function-local static created
// later, in addRO, so destroyed first: its procedure removes tasks from here).
scheduler sched;

// RO RevPi: attaches its I/O, shares its devices with the other machines, and
// runs the RO controller (state set remotely, e.g. from the web UI via remote_ro).
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");

    line_server server;   // port 47100, 20 s client watchdog
    if (!addLocalDevices(server)) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }
    // Not fatal: the device server keeps running so the outputs can still be read and switched
    if (!addRO(server, sched)) std::fprintf(stderr, "RO controller failed to start cleanly\n");
    if (!server.start()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        if (!sched.update()) std::fprintf(stderr, "Scheduler: a task failed\n");   // procedure steps
        if (!updateRO()) std::fprintf(stderr, "RO controller: starting a pump failed\n");
        server.poll(20);   // handles remote requests, doubles as the loop sleep
    }
}
