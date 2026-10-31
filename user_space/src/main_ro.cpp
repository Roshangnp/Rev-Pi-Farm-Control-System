#include <cstdio>
#include "config.h"

// RO RevPi: attaches its I/O and shares its devices with the other machines.
// No control logic or web UI yet.
int main() {
    if (!attachAllInterfaces()) std::fprintf(stderr, "Some interfaces failed to attach (check PiCtory pin names)\n");
    if (!startDeviceServer()) {
        std::fprintf(stderr, "Device server failed to start\n");
        return 1;
    }

    while (true) {
        serviceDeviceServer(20);
    }
}
