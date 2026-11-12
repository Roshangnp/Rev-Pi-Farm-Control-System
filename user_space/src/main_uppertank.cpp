#include <cstdio>
#include "config.h"
#include "line_server.h"

// Upper tank RevPi: attaches its I/O and shares its devices with the other
// machines (e.g. the host PC's web UI). No control logic or web UI yet.
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");

    line_server server;   // port 47100, drops clients idle 20 s
    if (!addLocalDevices(server) || !server.start()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        server.poll(20);   // handles remote requests, doubles as the loop sleep
    }
}
